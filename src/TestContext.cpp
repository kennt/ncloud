/*****
 * TestContext.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "stdincludes.h"
#include "catch.hpp"

#include "Context.h"
#include "MockNetwork.h"
#include "NetworkNode.h"
#include "Raft.h"

using namespace Raft;

// Test the Context class.  The Context class (generally) is
// responsible for maintaining the Raft state.
//
// There is a bit of interaction between the RaftHandler and
// the Context. That tests are left up to the functional tests in
// TestRaft.cpp.
//
// This is similar to MVC.  In this case Context=M RaftHandler=C
// (and no V).

TEST_CASE("Context", "[context]")
{
    string  name("mockleader");

    // The context gets created as part of a RaftHandler
    // so just test the one that's a part of the message handler
    //
    // Thus we don't go through the MessageHandler startup.

    Params *    par = new Params();
    Address     myAddr(0x64656667, 8000); // 100.101.102.103:8000
    auto network = MockNetwork::createNetwork(par);
    auto conn = network->create(myAddr);
    auto mockconn = network->findMockConnection(myAddr);

    NetworkNode node(name, nullptr, par, network);
    auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);

    // Setup the timeouts
    par->electionTimeout = 10;
    par->idleTimeout = 5;   // not used by the mock network
    par->rpcTimeout = 5;

    SECTION("initialization") {
        Raft::MemoryBasedContextStore store(par);
        auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, &store, netnode, conn);        

        par->resetCurrtime();
        network->reset();

        netnode->context.init(rafthandler.get(), &store);

        // verify initial values
        REQUIRE(netnode->context.currentTerm == 0);
        REQUIRE(!netnode->context.votedFor);
        REQUIRE(!netnode->context.currentLeader);
        REQUIRE(netnode->context.logEntries.size() == 1);
        REQUIRE(netnode->context.commitIndex == 0);
        REQUIRE(netnode->context.lastAppliedIndex == 0);
        REQUIRE(netnode->context.followers.size() == 0);
    }

    SECTION("load/save from the store") {
        // Test to see that only the relevant portions are restored
    }

    // Test Member APIs
    // Test RaftLogEntry APIs
    SECTION("addEntries") {
        Raft::MemoryBasedContextStore store(par);
        auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, &store, netnode, conn);        

        par->resetCurrtime();
        network->reset();

        netnode->member.memberList.clear();
        netnode->context.init(rafthandler.get(), &store);

        REQUIRE(netnode->context.logEntries.size() == 1);

        vector<RaftLogEntry>    newEntries;
        Address addr1(0x64656667, 8000); // 100.101.102.103:8000
        Address addr2(0x64656667, 8001); // 100.101.102.103:8001
        Address addr3(0x64656667, 8002); // 100.101.102.103:8002
        Address addr4(0x64656667, 8003); // 100.101.102.103:8003

        newEntries.emplace_back(1, Command::CMD_ADD_SERVER, addr1);
        newEntries.emplace_back(1, Command::CMD_REMOVE_SERVER, addr1);
        newEntries.emplace_back(1, Command::CMD_ADD_SERVER, addr2);

        vector<RaftLogEntry>    newEntries2;
        newEntries2.emplace_back(2, Command::CMD_REMOVE_SERVER, addr2);
        newEntries2.emplace_back(2, Command::CMD_ADD_SERVER, addr2);

        vector<RaftLogEntry>    newEntries3;
        newEntries3.emplace_back(2, Command::CMD_ADD_SERVER, addr3);
        newEntries3.emplace_back(2, Command::CMD_ADD_SERVER, addr4);

        // Invalid index values
        netnode->member.memberList.clear();
        netnode->context.init(rafthandler.get(), &store);
        REQUIRE_THROWS(netnode->context.addEntries(-1, newEntries));
        REQUIRE_THROWS(netnode->context.addEntries(2, newEntries));
        REQUIRE_THROWS(netnode->context.addEntries(1000, newEntries));

        // normal ops
        netnode->member.memberList.clear();
        netnode->context.init(rafthandler.get(), &store);
        netnode->context.addEntries(1, newEntries);
        REQUIRE(netnode->context.logEntries.size() == 4);
        REQUIRE(netnode->member.memberList.size() == 1);
        REQUIRE(netnode->member.memberList.front().address == addr2);

        // should overwrite with no change
        netnode->member.memberList.clear();
        netnode->context.init(rafthandler.get(), &store);
        netnode->context.addEntries(1, newEntries);
        netnode->context.addEntries(1, newEntries);
        REQUIRE(netnode->context.logEntries.size() == 4);
        REQUIRE(netnode->member.memberList.size() == 1);
        REQUIRE(netnode->member.memberList.front().address == addr2);

        //$ TODO: Add checks to verify that the follower list and the
        // membership lists are in sync

        // Check overwriting
        netnode->member.memberList.clear();
        netnode->context.init(rafthandler.get(), &store);
        netnode->context.addEntries(1, newEntries);
        REQUIRE(netnode->member.memberList.size() == 1);
        netnode->context.addEntries(3, newEntries3);
        REQUIRE(netnode->member.memberList.size() == 2);
        netnode->context.addEntries(2, newEntries3);
        REQUIRE(netnode->member.memberList.size() == 3);

        // addition of same address twice
        if (DEBUG_) {
            netnode->member.memberList.clear();
            netnode->context.init(rafthandler.get(), &store);
            netnode->context.addEntries(1, newEntries);
            REQUIRE_THROWS(netnode->context.addEntries(2, newEntries));
        }

        // Test remove of non-existent node
        if (DEBUG_) {
            netnode->member.memberList.clear();
            netnode->context.init(rafthandler.get(), &store);
            REQUIRE_THROWS(netnode->context.addEntries(1, newEntries2));
        }

        // Term mismatch
        if (DEBUG_) {
            netnode->member.memberList.clear();
            netnode->context.init(rafthandler.get(), &store);
            netnode->context.addEntries(1, newEntries);
            netnode->context.addEntries(4, newEntries2);
            REQUIRE_THROWS(netnode->context.addEntries(6, newEntries));
        }

    }
}
