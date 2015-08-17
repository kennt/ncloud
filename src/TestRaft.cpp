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

void runMessageLoop(vector<shared_ptr<NetworkNode>>& nodes, Params *par, int tm)
{
    for (auto & node : nodes) {
        runMessageLoop(node, par, 1);
    }
}

void sendAppendEntries(shared_ptr<IConnection> conn,
                       const Address& addr,
                       int transid,
                       int term,
                       const Address& leaderAddr,
                       int lastIndex,
                       int lastTerm,
                       int commitIndex,
                       Raft::RaftLogEntry *entry = nullptr)
{
    AppendEntriesMessage   append;
    append.transId = transid;
    append.term = term;
    append.leaderAddress = leaderAddr;
    append.prevLogIndex = lastIndex;
    append.prevLogTerm = lastTerm;
    append.leaderCommit = commitIndex;

    if (entry)
        append.entries.push_back(*entry);
    
    auto raw = append.toRawMessage(conn->address(), addr);
    conn->send(raw.get());
}

// Initializes the store with the data to start up
// a node with the specified cluster configuration.
// The leader gets added at term 0.  All other nodes
// are added at term 1 by default.
void initializeStore(Raft::ContextStoreInterface& store,
                     const Address& leader,
                     const vector<Address>& nodes)
{
    Json::Value root;
    Address     nullAddress;

    root["currentTerm"] = 1;
    root["votedFor"] = nullAddress.toAddressString();
    root["votedForPort"] = nullAddress.getPort();

    Json::Value log;
    
    Json::Value logEntry;
    logEntry["term"] = 0;
    logEntry["command"] = static_cast<int>(Command::CMD_ADD_SERVER);
    logEntry["address"] = leader.toAddressString();
    logEntry["port"] = leader.getPort();
    log.append(logEntry);

    for (auto & address: nodes) {
        logEntry["term"] = 1;
        logEntry["command"] = static_cast<int>(Command::CMD_ADD_SERVER);
        logEntry["address"] = address.toAddressString();
        logEntry["port"] = address.getPort();
        log.append(logEntry);
    }
    root["log"] = log;

    store.write(root);
}

// This will instantiate a single node with a MockNetwork.
// It will simulate the leaders and followers to get the
// instantiated node into the desired state.
// To start a node as a leader, let leaderAddr = nodeAddr;
// nodeAddr must ALWAYS be supplied.
//
// Parameters
//  par
//  store - the context store to use for the node
//      To setup the store for a mulit-node cluster,
//      use initializeStore().
//  leaderAddr - the address of the startup leader
//      may be the same as nodeAddr
//  nodeAddr - the main node that will be created
//
// Returns: a tuple
//  <0> : shared pointer to the MockNetwork
//  <1> : shared pointer to the NetworkNode
//  <2> : shared pointer to the RaftHandler
//
std::tuple<shared_ptr<MockNetwork>, shared_ptr<NetworkNode>, shared_ptr<RaftHandler>> 
createNode(Params *par,
           Raft::ContextStoreInterface *store,
           const Address& leaderAddr,
           const Address& nodeAddr)
{
    string  name("mock");
    auto network = MockNetwork::createNetwork(par);

    auto conn = network->create(nodeAddr);
    auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
    auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, store, netnode, conn);
    netnode->registerHandler(ConnectType::MEMBER,
                             conn,
                             rafthandler);

    netnode->nodeStart(leaderAddr, par->idleTimeout);

    return std::make_tuple(network, netnode, rafthandler);
}

// Pulls the RequestVotes off of the queue and replies
// with a voteGranted=true
// Does not do any validation.
void electLeader(shared_ptr<MockNetwork> network,
                 Params *par,
                 shared_ptr<NetworkNode> netnode,
                 const Address& leaderAddr,
                 const vector<Address>& nodes)
{
    // The election should have started, pull the votes
    // off and reply
    RequestVoteMessage  request;
    RequestVoteReply    reply;

    for (auto & addr : nodes) {
        auto conn = network->find(addr);
        if (conn == nullptr)
            conn = network->create(addr);
        auto raw = conn->recv(0);
        request.load(raw.get());

        reply.transId = request.transId;
        reply.term = 1;
        reply.voteGranted = true;
        raw = reply.toRawMessage(addr, leaderAddr);
        conn->send(raw.get());
    }
    runMessageLoop(netnode, par, par->getElectionTimeout());
}

// Simulates the child nodes accepting AppendEntries until
// they are "up-to-date"
// Does not do any validation of the messages.
void handleNodeUpdates(shared_ptr<MockNetwork> network,
                       Params *par,
                       shared_ptr<NetworkNode> netnode,
                       const Address& leaderAddr,
                       const vector<Address>& nodes)
{
    // Have each node pull off data
    vector<int>     logTerms;
    AppendEntriesMessage append;
    AppendEntriesReply reply;

    for (auto & addr : nodes) {
        logTerms.clear();
        logTerms.push_back(0);

        auto conn = network->find(addr);
        auto raw = conn->recv(0);;
        for (; raw != nullptr; raw = conn->recv(0)) {
            append.load(raw.get());

            if (append.prevLogIndex > (logTerms.size()-1))
                reply.success = false;
            else if (logTerms[append.prevLogIndex] != append.prevLogTerm)
                reply.success = false;
            else {
                logTerms.resize(append.prevLogIndex+1);
                for (auto & entry : append.entries) {
                    logTerms.push_back(entry.termReceived);
                }
                reply.success = true;
            }
            reply.transId = append.transId;
            reply.term = append.term;
            auto rawreply = reply.toRawMessage(addr, leaderAddr);
            conn->send(rawreply.get());
            
            runMessageLoop(netnode, par, 0);
        }
    }

}

// Server state testing (see if the Raft states transition
// properly)
TEST_CASE("Raft state testing", "[raft][state]")
{
    Params      params;
    Address     leaderAddr(0x64656667, 9000);
    Address     nodeAddr(0x64656667, 8100);

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    SECTION("Follower (times out) -> Candidate") {
        // Start up a network where the main node is a follower
        Raft::MemoryBasedContextStore store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        initializeStore(store, leaderAddr, nodes);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   nodeAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.getElectionTimeout());

        REQUIRE(netnode->context.currentState == State::CANDIDATE);
    }

    // Candidate (times out) -> new election
    SECTION("Candidate (times out) -> (new election)") {
        Raft::MemoryBasedContextStore store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.emplace_back(0x64656667, 8200);

        initializeStore(store, leaderAddr, nodes);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   nodeAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.getElectionTimeout());

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  
        REQUIRE(netnode->context.currentTerm == 2);      

        // The election should timeout and move to a new term
        runMessageLoop(netnode, &params, params.getElectionTimeout());

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  
        REQUIRE(netnode->context.currentTerm == 3);
    }

    // Candidate (receive majority vote) -> leader
    SECTION("Candiate (receives majority vote) -> leader") {
        Raft::MemoryBasedContextStore store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.emplace_back(0x64656667, 8200);

        initializeStore(store, leaderAddr, nodes);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   nodeAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto mockconn = network->findMockConnection(nodeAddr);
        network->create(nodes[1]);
        auto mockconn2 = network->findMockConnection(nodes[1]);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.getElectionTimeout());

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  
        REQUIRE(netnode->context.currentTerm == 2);      

        // There are three nodes : leader and two followers
        // There should be two request votes
        // one reply should be enough for a majority
        REQUIRE(mockconn->messagesSent == 2);

        // RequestVote : node --> other node
        auto raw = mockconn2->recv(0);
        HeaderOnlyMessage   header;
        RequestVoteMessage  request;
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::REQUEST_VOTE);
        request.load(raw.get());
        REQUIRE(request.term == 2);
        REQUIRE(request.candidate == nodeAddr);
        REQUIRE(request.lastLogIndex == 3);
        REQUIRE(request.lastLogTerm == 1);

        // RequestVoteReply : other node --> node
        RequestVoteReply    reply;
        reply.transId = request.transId;
        reply.term = 1;
        reply.voteGranted = true;
        raw = reply.toRawMessage(nodes[1], nodeAddr);
        mockconn2->send(raw.get());

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  

        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::LEADER);
    }

    SECTION("Candidate (discovers new leader or term) -> follower") {
        Raft::MemoryBasedContextStore store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.emplace_back(0x64656667, 8200);

        initializeStore(store, leaderAddr, nodes);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   nodeAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto mockconn = network->findMockConnection(nodeAddr);
        network->create(nodes[1]);
        auto mockconn2 = network->findMockConnection(nodes[1]);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.getElectionTimeout());

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  
        REQUIRE(netnode->context.currentTerm == 2);      

        // This should cause the node to fall back to a follower
        sendAppendEntries(mockconn2, nodeAddr,
                          1 /*transid*/, 3 /*term*/, leaderAddr,
                          1 /*index*/, 0 /*term*/, 1 /*commit*/);
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);
    }

    SECTION("Leader (discovers server with higher term) -> follower") {
        Raft::MemoryBasedContextStore store(&params);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   leaderAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto rafthandler = std::get<2>(nettuple);
        auto conn = network->create(nodeAddr);

        params.resetCurrtime();
        network->reset();

        // Run through the election timeout
        runMessageLoop(netnode, &params, params.getElectionTimeout());
        REQUIRE(netnode->context.currentState == Raft::State::LEADER);

        sendAppendEntries(conn, leaderAddr,
                          1 /*transid*/, 3 /*term*/, leaderAddr,
                          1 /*index*/, 0 /*term*/, 1 /*commit*/);
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);
    }
}

TEST_CASE("Raft single-node startup", "[raft][startup]")
{
    Params      params;
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    // Startup (as a leader)
    SECTION("simple startup as a leader") {
        // This is the cluster startup model. This is the first
        // node that will startup, thus it will create a log and
        // start up as a candidate.
        Raft::MemoryBasedContextStore store(&params);

        params.resetCurrtime();

        auto nettuple = createNode(&params,
                                   &store,
                                   myAddr,
                                   myAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto mockconn = network->findMockConnection(myAddr);

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
        runMessageLoop(netnode, &params, params.getElectionTimeout());

        // Election won, should be a leader      
        REQUIRE(netnode->context.currentState == Raft::State::LEADER);
        REQUIRE(netnode->context.currentLeader == myAddr);

        // What is the expected log state?
        // The log should not have changed.
        REQUIRE(store.entries.size() == 1);
        REQUIRE(store.current["log"].size() == 2);
    }

    // Startup of a follower, which is then contacted by a leader
    SECTION("startup->follower->candidate then joins a group") {
        Raft::MemoryBasedContextStore store(&params);

        params.resetCurrtime();

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   myAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto mockconn = network->findMockConnection(myAddr);
        auto leaderconn = network->create(leaderAddr);

        // According to our protocol, pass in a null address
        netnode->nodeStart(Address(), 10);

        // Check the startup state
        int term = netnode->context.currentTerm;
        REQUIRE(netnode->context.currentTerm == 0);
        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        // Run through a single loop.
        runMessageLoop(netnode, &params, 1);

        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);
        REQUIRE(mockconn->messagesSent == 0);

        // Have the leader "contact" the node via an append entries RPC.
        // AppendEntries : leader --> node
        sendAppendEntries(leaderconn, myAddr,
                          1 /*transid*/, 1 /*term*/, leaderAddr,
                          1 /*index*/, 0 /*term*/, 1 /*commit*/);

        runMessageLoop(netnode, &params, 1);

        // It should still be a follower, but it should be following
        // the leader
        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);
        REQUIRE(netnode->context.currentLeader == leaderAddr);
        REQUIRE(netnode->context.currentTerm == 1);
        REQUIRE(mockconn->messagesSent == 1);

        // Pull the message off of the connection and check it
        // AppendEntriesReply : node --> leader
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
// Typical message flow
//
// (Catch up the new node to the leader's previous config)
// leader -> node
//      AppendEntries: transid(1), lastIndex(0), lastTerm(0), data[1]
// node -> leader
//      AppendEntriesReply: success(true)
// leader -> node
//      AppendEntries: transid(1), lastIndex(1), lastTerm(0), no data
// node -> leader
//      AppendEntriesReply: success(true)
//
// (Catch up all nodes to the current config)
// leader -> node
//      AppendEntries: transid(2), lastIndex(2), lastTerm(1), no data
// node -> leader
//      AppendEntriesReply: success(false)
// leader -> node
//      AppendEntries: transid(2), lastIndex(1), lastTerm(0), data[2]
// node -> leader
//      AppendEntriesReply: success(true)
// leader -> node
//      AppendEntries: transid(2), lastIndex(2), lastTerm(1), no data
// node -> leader
//      AppendEntriesReply: success(true)
//
TEST_CASE("AddServer test cases", "[raft][AddServer]") {
    Params      params;
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    // AddServer test - leader functionality
    // Main node: 9000
    // Nodes: 9000 (leader), 8080
    // Action: Adding 8080 to the cluster
    SECTION("Basic AddServer functionality - leader") {
        // Startup a leader
        Raft::MemoryBasedContextStore store(&params);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   leaderAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto rafthandler = std::get<2>(nettuple);
        auto conn = network->create(myAddr);
        auto mockconn = network->findMockConnection(myAddr);

        params.resetCurrtime();
        network->reset();

        // Run through the election timeout
        runMessageLoop(netnode, &params, params.getElectionTimeout());
        REQUIRE(netnode->context.currentState == Raft::State::LEADER);

        // Call through the onAddServerCommand()
        auto command = make_shared<CommandMessage>();
        command->type = CommandType::CRAFT_ADDSERVER;
        command->transId = 1;
        command->address = myAddr;
        command->to = leaderAddr;
        command->from = myAddr;

        rafthandler->onChangeServerCommand(command,
                                           Command::CMD_ADD_SERVER,
                                           myAddr);

        // Should update the new server to current config
        // There should be a message to the new server

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
        REQUIRE(append.prevLogIndex == 0);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].termReceived == 0);
        REQUIRE(append.entries[0].command == Command::CMD_ADD_SERVER);
        REQUIRE(append.entries[0].address == leaderAddr);
        REQUIRE(append.leaderCommit == 1);

        // Send a reply
        AppendEntriesReply reply;
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = true;
        raw = reply.toRawMessage(myAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Should get another AppendEntries
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 1);
        REQUIRE(append.prevLogTerm == 0);
        REQUIRE(append.prevLogIndex == 1);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a reply
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = true;
        raw = reply.toRawMessage(myAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Ok, the new server is caught up to the leader
        // Now it should try to have all servers commit
        // this config (although there is only the new follower.
        // so nothing will be sent).

        // Ok, now the new server should be added to the config
        // This will send an update to the new server

        // Should add the new server to config
        // We have only been sent the log[1]
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 1);
        REQUIRE(append.prevLogTerm == 1);
        REQUIRE(append.prevLogIndex == 2);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a failure reply (we are not fully up-to-date)
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = false;
        raw = reply.toRawMessage(myAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Receive the next entry
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 1);
        REQUIRE(append.prevLogTerm == 0);
        REQUIRE(append.prevLogIndex == 1);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].termReceived == 1);
        REQUIRE(append.entries[0].command == Command::CMD_ADD_SERVER);
        REQUIRE(append.entries[0].address == myAddr);
        REQUIRE(append.leaderCommit == 1);

        // Send a success
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = true;
        raw = reply.toRawMessage(myAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Should expect one more AppendEntries
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 1);
        REQUIRE(append.prevLogTerm == 1);
        REQUIRE(append.prevLogIndex == 2);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a success
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = true;
        raw = reply.toRawMessage(myAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Should receive a command reply
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        CommandMessage  cmdreply;
        cmdreply.load(raw.get());
        REQUIRE(cmdreply.success == true);
    }

    // AddServer test - follower functionality
    // Assume that there's a leader that is adding this
    // node to the cluster
    // Main node: 8080
    // Nodes: 9000 (leader), 8080
    // Action: Adding 8080 to the cluster
    SECTION("Basic AddServer functionality - follower") {
        Raft::MemoryBasedContextStore store(&params);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   myAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto mockconn = network->findMockConnection(myAddr);
        auto leaderconn = network->create(leaderAddr);
        AppendEntriesReply  reply;
        HeaderOnlyMessage   header;
        RaftLogEntry        entry;

        params.resetCurrtime();
        network->reset();

        // Assuming the leader has been sent an Add Server command.

        // It will catch up the node
        entry.termReceived = 0;
        entry.command = Command::CMD_ADD_SERVER;
        entry.address = leaderAddr;
        sendAppendEntries(leaderconn, myAddr,
                          1 /*transid*/, 1 /*term*/, leaderAddr,
                          0 /*index*/, 0 /*term*/, 1 /*commit*/,
                          &entry);
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->member.isMember(leaderAddr));
        REQUIRE(!netnode->member.isMember(myAddr));

        auto raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        // Expect a true since everyone has a 0th entry
        REQUIRE(reply.success == true);

        sendAppendEntries(leaderconn, myAddr,
                          1 /*transid*/, 1 /*term*/, leaderAddr,
                          1 /*index*/, 0 /*term*/, 1 /*commit*/);
        runMessageLoop(netnode, &params, 0);

        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        // Expect true, we sent this entry on the previous AppendEntries
        REQUIRE(reply.success == true);

        // Now we should be adding the new node
        sendAppendEntries(leaderconn, myAddr,
                          2 /*transid*/, 1 /*term*/, leaderAddr,
                          2 /*index*/, 1 /*term*/, 1 /*commit*/);
        runMessageLoop(netnode, &params, 0);
        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        // Expect a true since everyone has a 0th entry
        REQUIRE(reply.success == false);

        REQUIRE(!netnode->member.isMember(myAddr));

        entry.termReceived = 1;
        entry.command = Command::CMD_ADD_SERVER;
        entry.address = myAddr;
        sendAppendEntries(leaderconn, myAddr,
                          2 /*transid*/, 1 /*term*/, leaderAddr,
                          1 /*index*/, 0 /*term*/, 1 /*commit*/,
                          &entry);
        runMessageLoop(netnode, &params, 0);
        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        // Expect a true since it has the previous config
        REQUIRE(reply.success == true);

        // Verify it has the new config        
        sendAppendEntries(leaderconn, myAddr,
                          2 /*transid*/, 1 /*term*/, leaderAddr,
                          2 /*index*/, 1 /*term*/, 1 /*commit*/);
        runMessageLoop(netnode, &params, 0);
        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        // Expect a true since it has the previous config
        REQUIRE(reply.success == true);

        REQUIRE(netnode->member.isMember(myAddr));
    }
}

// Test RemoveServer
TEST_CASE("RemoveServer test cases", "[raft][RemoveServer]") {
    Params      params;
    Address     nodeAddr(0x64656667, 8100); // 100.101.102.103:8100
    Address     node2Addr(0x64656667, 8200); // 100.101.102.103:8200
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    // AddServer test - leader functionality
    // Main node: 9000
    // Nodes: 9000 (leader), 8100, 8200
    // Action: remove 8200 from the cluster
    SECTION("Basic RemoveServer functionality - leader") {
        // Startup a leader with 2 follower (total of 3)
        Raft::MemoryBasedContextStore store(&params);
        HeaderOnlyMessage header;
        AppendEntriesMessage append;
        AppendEntriesReply reply;

        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.push_back(node2Addr);

        initializeStore(store, leaderAddr, nodes);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   leaderAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto rafthandler = std::get<2>(nettuple);

        params.resetCurrtime();

        // Elect this node as a leader
        electLeader(network, &params, netnode, leaderAddr, nodes);

        // Take care of any updates from the leader
        // assumes nodes have nothing
        handleNodeUpdates(network, &params, netnode, leaderAddr, nodes);

        auto mockconn = network->findMockConnection(nodeAddr);

        REQUIRE(netnode->context.currentState == Raft::State::LEADER);
        REQUIRE(netnode->member.isMember(nodeAddr));
        REQUIRE(netnode->member.isMember(node2Addr));

        // Call through the onAddServerCommand()
        // node --> leader (remove node2)
        auto command = make_shared<CommandMessage>();
        command->type = CommandType::CRAFT_REMOVESERVER;
        command->transId = 1;
        command->address = node2Addr;
        command->to = leaderAddr;
        command->from = nodeAddr;

        rafthandler->onChangeServerCommand(command,
                                           Command::CMD_REMOVE_SERVER,
                                           node2Addr);

        // Now it should try to have all servers commit
        // this config (although there is only the one other
        // follower).

        // Should see the removal of the server from
        // the config
        auto raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 1);
        REQUIRE(append.prevLogIndex == 3);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 3);

        // The nodes are up-to-date with the previous config
        reply.transId = append.transId;
        reply.term = 1;
        reply.success = true;
        raw = reply.toRawMessage(nodeAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Waiting for the update to the current config
        // Receive the next entry
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogIndex == 4);
        REQUIRE(append.prevLogTerm == 2);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 3);

        // Send a failure, we don't have the last change
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = false;
        raw = reply.toRawMessage(nodeAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogIndex == 3);
        REQUIRE(append.prevLogTerm == 1);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].termReceived == 2);
        REQUIRE(append.entries[0].command == Command::CMD_REMOVE_SERVER);
        REQUIRE(append.entries[0].address == node2Addr);
        REQUIRE(append.leaderCommit == 3);

        // Send a success
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = true;
        raw = reply.toRawMessage(nodeAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // Should expect one more AppendEntries
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 2);
        REQUIRE(append.prevLogIndex == 4);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 3);

        // Send a success
        reply.transId = append.transId;
        reply.term = append.term;
        reply.success = true;
        raw = reply.toRawMessage(nodeAddr, leaderAddr);
        mockconn->send(raw.get());
        runMessageLoop(netnode, &params, 0);

        // The commit index should go up now that it has
        // verified that we have received the 4th index
        REQUIRE(netnode->context.commitIndex == 4);

        // Should receive a command reply
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        CommandMessage  cmdreply;
        cmdreply.load(raw.get());
        REQUIRE(cmdreply.success == true);

        // Verify that the change is no longer in the log
        REQUIRE(netnode->member.isMember(nodeAddr));
        REQUIRE(!netnode->member.isMember(node2Addr));
    }

    // RemoveServer test - follower functionality
    // Assume that there's a leader that is telling us
    // to remove a node from the cluster.
    //
    // Main node: 8100
    // Nodes: 9000 (leader), 8100, 8200
    // Action: remove 8200 from the cluster
    SECTION("Basic RemoveServer functionality - follower") {
        Raft::MemoryBasedContextStore store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.push_back(node2Addr);
        initializeStore(store, leaderAddr, nodes);

        auto nettuple = createNode(&params,
                                   &store,
                                   leaderAddr,
                                   nodeAddr);
        auto network = std::get<0>(nettuple);
        auto netnode = std::get<1>(nettuple);
        auto mockconn = network->findMockConnection(nodeAddr);
        auto leaderconn = network->create(leaderAddr);
        AppendEntriesReply  reply;
        HeaderOnlyMessage   header;
        RaftLogEntry        entry;

        params.resetCurrtime();
        network->reset();

        // Now assume that the leader has received a remove server
        // command

        // See if the nodes have the previous config
        sendAppendEntries(leaderconn, nodeAddr,
                        1 /*transid*/, 2/*term*/, leaderAddr,
                        3 /*index*/, 1 /*term*/, 3 /*commit*/);
        runMessageLoop(netnode, &params, 0);

        auto raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        REQUIRE(reply.success == true);

        // The node is up-to-date with the previous config
        // Move up to the new config
        sendAppendEntries(leaderconn, nodeAddr,
                          2 /*transid*/, 2 /*term*/, leaderAddr,
                          4 /*index*/, 2 /*term*/, 3 /*commit*/);
        runMessageLoop(netnode, &params, 0);

        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        REQUIRE(reply.success == false);

        entry.termReceived = 2;
        entry.command = Command::CMD_REMOVE_SERVER;
        entry.address = node2Addr;
        sendAppendEntries(leaderconn, nodeAddr,
                          2 /*transid*/, 2 /*term*/, leaderAddr,
                          3 /*index*/, 1 /*term*/, 3 /*commit*/,
                          &entry);
        runMessageLoop(netnode, &params, 0);

        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        REQUIRE(reply.success == true);
        REQUIRE(netnode->context.commitIndex == 3);

        sendAppendEntries(leaderconn, nodeAddr,
                          2 /*transid*/, 2 /*term*/, leaderAddr,
                          4 /*index*/, 2 /*term*/, 3 /*commit*/);
        runMessageLoop(netnode, &params, 0);

        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        REQUIRE(reply.success == true);

        // OUr commitIndex is still 3, we haven't received a request
        // from the leader with a commitIndex of 4 yet
        REQUIRE(netnode->context.commitIndex == 3);
        REQUIRE(netnode->member.isMember(nodeAddr));
        REQUIRE(!netnode->member.isMember(node2Addr));

        // Test the commit index update
        sendAppendEntries(leaderconn, nodeAddr,
                          2 /*transid*/, 2 /*term*/, leaderAddr,
                          4 /*index*/, 2 /*term*/, 4 /*commit*/);
        runMessageLoop(netnode, &params, 0);
        REQUIRE(netnode->context.commitIndex == 4);
    }
}


TEST_CASE("System test", "[raft][system]") {
    Params      params;
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    vector<Address> nodes;
    nodes.emplace_back(0x64656667, 8010);
    nodes.emplace_back(0x64656667, 8020);

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    SECTION("3-node startup") {
        vector<shared_ptr<NetworkNode>> nodes;

        // Create the leader
        // Create the two follower nodes
        // Tell the leader to add the child nodes
        // Let the system settle

        // Verify the expected system state
    }
}

