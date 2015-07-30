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

#if 0
shared_ptr<Raft::AddServerReply> makeAddServerReply(const MockMessage *message,
                                                    bool status,
                                                    const Address& leaderHint)
{
    istringstream ss(std::string((const char *)message->data.get(),
                                 message->dataSize));
    MessageType msgtype = static_cast<MessageType>(read_raw<int>(ss));
    ss.seekg(0, ss.beg);    // reset to start of the buffer

    // Validate that the MockMessage is an ADD_SERVER
    if (msgtype != Raft::MessageType::ADD_SERVER)
        throw AppException("not an ADD_SERVER message, cannot build reply");

    // Construct a reply from the AddServerMessage
    AddServerMessage    addServerMessage;
    addServerMessage.load(ss);

    auto reply = make_shared<AddServerReply>();
    reply->transId = addServerMessage.transId;
    reply->status = status;
    reply->leaderHint = leaderHint;

    return reply;
}
#endif

TEST_CASE("Raft single-node startup", "[raft][startup]")
{
    string  name("mockleader");
    // Basic startup test case
    // Create a mock test network
    Params *    par = new Params();
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    auto network = MockNetwork::createNetwork(par);
    auto conn = network->create(myAddr);
    Address     nullAddress;    // 0.0.0.0:0

    // Connection and address for a dummy leader node
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    auto leaderConn = network->create(leaderAddr);


    // Setup the timeouts
    par->electionTimeout = 10;
    par->idleTimeout = 5;   // not used by the mock network

    // Startup with a null address
    SECTION("simple startup") {
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftMessageHandler>(nullptr, par, &store, netnode, conn);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);

        REQUIRE( nullAddress.isZero() );

        // Node will start up normally as a follower
        // Not really different from the normal case
        netnode->nodeStart(nullAddress, 10);

        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);

        // Should be no changes in the membership list since
        // there is no leader yet, we are still a follower.
        REQUIRE(netnode->member.inited == true);
        REQUIRE(netnode->member.inGroup == false);
        REQUIRE(netnode->member.memberList.size() == 0);

        // After it timesout, should transition to a candidate
        // then transition to a leader (since there is only one
        // node).
        //
        par->addToCurrtime(par->electionTimeout);
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        REQUIRE(netnode->context.currentState == Raft::State::CANDIDATE);

        // Since this became a candidate
        // Increment current term
        // (check for vote for self)
        // (check for RPCs)

        // On the next timeout, check for majority of votes (should have
        // it, since this is the only node).
        par->addToCurrtime(1);
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        REQUIRE(netnode->context.currentState == Raft::State::LEADER);


        // Election won, should be a leader        

        // What is the expected log state?
        // Check the log, there should be a single entry for adding itself
        // (Also no messages are sent out beause there are no other nodes)
    }

    // Startup of a follower, which transitions to a candidate
    // but is then contacted by a leader
    SECTION("startup->follower->candidate then joins a group") {
        Address     bogusAddr(0x64656667, 9000); // 100.101.102.103:9000

        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftMessageHandler>(nullptr, par, &store, netnode, conn);
        auto mockconn = network->findMockConnection(myAddr);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);

        // According to our protocol, pass it it's own address, this should
        // start it up as the leader.
        netnode->nodeStart(bogusAddr, 10);

        // Check the state
        // Run through a single loop.
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        // Wait for some time, it should transition to a candidate
        par->addToCurrtime(par->electionTimeout);
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        REQUIRE(netnode->context.currentState == Raft::State::CANDIDATE);

        // Send it a heartbeat (appendEntries RPC). It should transition
        // to a FOLLOWER
        //$ TODO: add this
    }

    // Startup of a follower, which transitions to a candidate,
    // then to a leader.  But is then contacted by another leader.
    SECTION("startup->follower->candidate->leader then joins a group") {
    }

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

