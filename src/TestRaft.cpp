/*****
 * TestRaft.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "stdincludes.h"
#include "catch.hpp"

#include "Raft.h"
#include "MockNetwork.h"

using namespace Raft;

// In general, the approach is to do black-box testing of the RAFT protocol.
// Although, sometimes specific calls to look at the internals of the RAFT
// state may be needed.


void runMessageLoop(shared_ptr<NetworkNode> node, Params *par, int tm)
{
    par->addToCurrtime(tm);
    node->receiveMessages();
    node->processQueuedMessages();
}

// Test some basic message-handling
TEST_CASE("Raft message handling", "[raft][messages]")
{
}

TEST_CASE("Raft single-node startup", "[raft][startup]")
{
    string  name("mockleader");
    // Basic startup test case
    // Create a mock test network
    Params *    par = new Params();
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    auto network = MockNetwork::createNetwork(par);
    auto conn = network->create(myAddr);
    auto mockconn = network->findMockConnection(myAddr);
    Address     nullAddress;    // 0.0.0.0:0

    // Connection and address for a dummy leader node
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    auto leaderconn = network->create(leaderAddr);
    auto mockleaderconn = network->findMockConnection(leaderAddr);

    // Setup the timeouts
    par->electionTimeout = 10;
    par->idleTimeout = 5;   // not used by the mock network
    par->rpcTimeout = 5;

    // Startup (as a leader)
    SECTION("simple startup as a leader") {
        // This is the cluster startup model. This is the first
        // node that will startup, thus it will create a log and
        // start up as a candidate.
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, &store, netnode, conn);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);

        // Node will start up as a candidate, but follows
        // a special codepath where it will initialize the
        // log.
        netnode->nodeStart(myAddr, 10);

        REQUIRE(netnode->context.currentState == Raft::State::CANDIDATE);
        REQUIRE(netnode->member.inited == true);
        REQUIRE(netnode->member.inGroup == true);
        REQUIRE(netnode->member.memberList.size() == 1);
        REQUIRE(netnode->member.isMember(myAddr));

        // Since this became a candidate
        // (check for incremented term)
        REQUIRE(netnode->context.currentTerm == 1);
        // (check for vote for self)
        REQUIRE(myAddr == netnode->context.votedFor);
        // Should still be no leader
        REQUIRE(!netnode->context.currentLeader);
        // (check for RPCs), should be none since only one node
        REQUIRE(mockconn->messagesSent == 0);

        //$ TODO: check the log
        REQUIRE(store.entries.size() == 1);
        // Size is 2 because there is always one empty element
        REQUIRE(store.current["log"].size() == 2);
        REQUIRE(store.current["log"][0]["command"].asInt() == static_cast<int>(Raft::Command::CMD_NOOP));
        REQUIRE(store.current["log"][1]["address"].asString() == myAddr.toAddressString());
        REQUIRE(store.current["log"][1]["port"].asInt() == myAddr.getPort());
        REQUIRE(store.current["log"][1]["command"].asInt() == static_cast<int>(Raft::Command::CMD_ADD_SERVER));

        // After timeout, it should check the results which would
        // indicate an election success.
        // Note that the check occurs on election timeout (we would
        // check during receiving a vote, but there are no other nodes,
        // thus no other votes).
        runMessageLoop(netnode, par, par->electionTimeout);

        // Election won, should be a leader      
        REQUIRE(netnode->context.currentState == Raft::State::LEADER);
        REQUIRE(netnode->context.currentLeader == myAddr);

        // Send a query to see who is the leader?  

        // What is the expected log state?
        // The log should not have changed.
        REQUIRE(store.entries.size() == 1);
        REQUIRE(store.current["log"].size() == 2);
    }

    // Startup of a follower, which is then contacted by a leader
    SECTION("startup->follower->candidate then joins a group") {
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, &store, netnode, conn);
        auto mockconn = network->findMockConnection(myAddr);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);

        // According to our protocol, pass in a null address
        netnode->nodeStart(Address(), 10);

        // Check the startup state
        int term = netnode->context.currentTerm;
        REQUIRE(netnode->context.currentTerm == 0);
        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        // Run through a single loop.
        runMessageLoop(netnode, par, 1);

        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);
        REQUIRE(mockconn->messagesSent == 0);

        // Have the leader "contact" the node via an append entries RPC.
        AppendEntriesMessage    message;
        message.transId = 1;
        message.term = 1;
        message.leaderAddress = leaderAddr;
        message.prevLogIndex = 1;
        message.prevLogTerm = 0;
        message.leaderCommit = 1;
        auto raw = message.toRawMessage(leaderAddr, myAddr);
        leaderconn->send(raw.get());

        runMessageLoop(netnode, par, 1);

        // It should still be a follower, but it should be following
        // the leader
        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);
        REQUIRE(netnode->context.currentLeader == leaderAddr);
        REQUIRE(netnode->context.currentTerm == 1);
        REQUIRE(mockconn->messagesSent == 1);

        // Pull the message off of the connection and check it
        auto recvMessage = leaderconn->recv(0);
        REQUIRE(recvMessage);

        // Should be an AppendEntriesReply
        AppendEntriesReply reply;
        reply.load(recvMessage.get());

        // The logs don't match up, should get a false reply
        REQUIRE(reply.success == false);
        REQUIRE(reply.term == 1);
    }
}

// Test cases for log operations
TEST_CASE("Raft log ops", "[raft][log]")
{
    string  name("mockleader");
    // Basic startup test case
    // Create a mock test network
    Params *    par = new Params();
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    auto network = MockNetwork::createNetwork(par);
    auto conn = network->create(myAddr);
    auto mockconn = network->findMockConnection(myAddr);

    // Connection and address for a dummy leader node
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    auto leaderconn = network->create(leaderAddr);
    auto mockleaderconn = network->findMockConnection(leaderAddr);

    // Setup the timeouts
    par->electionTimeout = 10;
    par->idleTimeout = 5;   // not used by the mock network
    par->rpcTimeout = 5;

    // Test that the leader sends the correct log update
    // (update of a single log entry)
    SECTION("leader - update of a single log entry") {
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        // Startup the leader
        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, &store, netnode, conn);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);
        netnode->nodeStart(myAddr, 10);

        // Run through the election timeout
        runMessageLoop(netnode, par, par->electionTimeout);
        REQUIRE(netnode->context.currentState == Raft::State::LEADER);

        // Ok, Add a follower node
        // Have the server send the follower a log update  
    }

    // Test that a follower receives and applies the
    // correct log update (single log entry)
    SECTION("follower - update of a single log entry") {
    }

    // Test for multiple updates
    // (multiple log entry update)

    // Test for maximum time allowed
}

// Test cases for various election scenarios
TEST_CASE("Raft elections", "[raft][election]")
{
    // election timeout, does the node become a leader and
    // start an election?

    // election timeout with multiple nodes, do they
    // backoff randomly

    // multiple leader scenarios
}

// Test cases for failover scenarios
TEST_CASE("Raft failover", "[raft][failover]") {
    // Test startup with a follower (and a leader that is down)
    // have the follower redirect the AddServer (which will fail)
    // timeout and then try again
}

// Test cases for log replication

// Test cases for log compaction

// Full multi-node scenario test (no errors)
// (tests full node-node interaction rather than simulated nodes)
TEST_CASE("Raft multi-node startup", "[raft][full]") {
    // Startup three nodes and have them communicate with each other
    // One will be picked as the leader
}

// Test AddServer
TEST_CASE("AddServer test cases", "[raft][AddServer]") {
    string  name("mockleader");
    // Basic startup test case
    // Create a mock test network
    Params *    par = new Params();
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    auto network = MockNetwork::createNetwork(par);
    auto conn = network->create(myAddr);
    auto mockconn = network->findMockConnection(myAddr);

    // Connection and address for a dummy leader node
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    auto leaderconn = network->create(leaderAddr);
    auto mockleaderconn = network->findMockConnection(leaderAddr);

    // Setup the timeouts
    par->electionTimeout = 10;
    par->idleTimeout = 5;   // not used by the mock network
    par->rpcTimeout = 5;

    SECTION("Basic AddServer functionality") {
        // Startup a leader
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        // Startup the leader
        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, 
                                &store, netnode, leaderconn);
        netnode->registerHandler(ConnectType::MEMBER,
                                 leaderconn,
                                 rafthandler);
        netnode->nodeStart(leaderAddr, 10);

        // Run through the election timeout
        runMessageLoop(netnode, par, par->electionTimeout);
        REQUIRE(netnode->context.currentState == Raft::State::LEADER);

        // Call through the onAddServerCommand()
        auto command = make_shared<CommandMessage>();
        command->type = CommandType::CRAFT_ADDSERVER;
        command->transId = 1;
        command->address = myAddr;
        command->to = leaderAddr;
        command->from = myAddr;

        rafthandler->onAddServerCommand(command, myAddr);

        // Should attempt to update to previous config
        // Start with an appendEntries
        // Receive empty appendEntries on mockconn
        auto raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        HeaderOnlyMessage       header;
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);

        AppendEntriesMessage    append;
        append.load(raw.get());
        REQUIRE(append.term == 1);
        REQUIRE(append.prevLogTerm == 0);
        REQUIRE(append.prevLogIndex == 1);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a reply
        AppendEntriesReply reply;
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = false;
        raw = reply.toRawMessage(myAddr, leaderAddr);
        mockconn->send(raw.get());

        runMessageLoop(netnode, par, 1);

        // Receive appendEntries with data
        raw = mockconn->recv(0);
        REQUIRE(raw.get() != nullptr);
        append.load(raw.get());

        // Send a reply

        // Check to see that the state is up-to-date

    }
}

// Test RemoveServer

// Heartbeat tests
// Test changing from leader->candidate
// Test changing from candidate->leader


