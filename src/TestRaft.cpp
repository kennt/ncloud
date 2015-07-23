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

// In general, the approach is to do black-box testing of the RAFT protocol.
// Although, sometimes specific calls to look at the internals of the RAFT
// state may be needed.

TEST_CASE("Raft", "[startup]")
{
    string  name("mockleader");
    // Basic startup test case
    // Startup of THE initial leader node

    // Create a much more minimal test network
    Params *    par = new Params();
    Address     myAddr(0x64656667, 8080); // 100.101.102.103:8080
    auto network = MockNetwork::createNetwork(par);
    auto conn = network->create(myAddr);

    // Setup the timeouts
    par->electionTimeout = 10;
    par->heartbeatTimeout = 5;

    // Startup with a null address
    SECTION("zero address startup - error") {
        Address     nullAddress;    // 0.0.0.0:0
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
    }

    // Startup of a single node (specified as leader)
    SECTION("single node startup") {
        Raft::MemoryBasedContextStore store(par);

        par->resetCurrtime();
        network->reset();

        auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
        auto rafthandler = make_shared<Raft::RaftMessageHandler>(nullptr, par, &store, netnode, conn);
        netnode->registerHandler(ConnectType::MEMBER,
                                 conn,
                                 rafthandler);

        // According to our protocol, pass it it's own address, this should
        // start it up as the leader.
        netnode->nodeStart(myAddr, 10);

        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);

        // Check the state
        // Run through a single loop.
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        // Not enough time has passed for us, so the state should not
        // have changed.

        REQUIRE(netnode->context.currentState == Raft::State::FOLLOWER);

        // May have to force it to timeout
        // (Change the time to a far future)
        // Need to have enough time has passed so that it transitions
        // to a candidate
        par->addToCurrtime(par->electionTimeout);
        netnode->receiveMessages();
        netnode->processQueuedMessages();

        REQUIRE(netnode->context.currentState == Raft::State::CANDIDATE);

        // Need another cycle for it to transition from candidate
        // to leader
        par->addToCurrtime(1);   // Move past the timeouts

        REQUIRE(netnode->context.currentState == Raft::State::LEADER);

        // What is the expected log state?
        // Check the log, there should be a single entry for adding itself
        // (Also no messages are sent out beause there are no other nodes)
    }

    // Test startup with a single node (not specified as leader).

    // Test startup with an active leader, add a server

    // Test startup with an active leader, add a server that's
    // been running a while (and has become a leader)

    // Test startup with a leader and a follower, have the
    // follower redirect an AddServer request

    // Test startup with a follower (and a leader that is down)
    // have the follower redirect the AddServer (which will fail)
    // timeout and then try again
}
