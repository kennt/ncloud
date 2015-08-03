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
    auto leaderConn = network->create(leaderAddr);

    // Setup the timeouts
    par->electionTimeout = 10;
    par->idleTimeout = 5;   // not used by the mock network
    par->rpcTimeout = 5;

    // Startup (as a leader)
    SECTION("simple startup as a leader") {
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftMessageHandler>(nullptr, par, &store, netnode, conn);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);

        // Node will start up as a follower, but follows
        // a special codepath where it will initialize the
        // log.
        netnode->nodeStart(myAddr, 10);

        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);

        // Should be no changes in the membership list since
        // there is no leader yet, we are still a follower.
        REQUIRE(netnode->member.inited == true);
        REQUIRE(netnode->member.inGroup == false);
        REQUIRE(netnode->member.memberList.size() == 0);

        int term = netnode->context.currentTerm;

        // After it timesout, should transition to a candidate
        // then transition to a leader (since there is only one
        // node).
        //
        par->addToCurrtime(par->electionTimeout);
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        REQUIRE(netnode->context.currentState == Raft::State::CANDIDATE);

        // Since this became a candidate
        // (check for incremented term)
        REQUIRE((term+1) == netnode->context.currentTerm);
        // (check for vote for self)
        REQUIRE(myAddr == netnode->context.votedFor);
        // (check for RPCs), should be none since only one node
        REQUIRE(mockconn->messagesSent == 0);
        // Should still be no leader
        REQUIRE(netnode->context.leaderAddress.isZero());

        // On the next timeout, check for majority of votes (should have
        // it, since this is the only node).
        par->addToCurrtime(1);
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        REQUIRE(netnode->context.currentState == Raft::State::LEADER);


        // Election won, should be a leader      

        // Send a query to see who is the leader?  

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

