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

#include "TestRaftUtil.h"

using namespace Raft;

// In general, the approach is to do black-box testing of the RAFT protocol.
// Although, sometimes specific calls to look at the internals of the RAFT
// state may be needed.


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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);

        store.write(initializeStore(leaderAddr, nodes));

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   nodeAddr);
        auto netnode = std::get<0>(nettuple);
        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.electionTimeout);

        REQUIRE(netnode->context.currentState == State::CANDIDATE);
    }

    // Candidate (times out) -> new election
    SECTION("Candidate (times out) -> (new election)") {
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.emplace_back(0x64656667, 8200);

        store.write(initializeStore(leaderAddr, nodes));

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   nodeAddr);
        auto netnode = std::get<0>(nettuple);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.electionTimeout);

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  
        REQUIRE(netnode->context.currentTerm == 2);      

        // The election should timeout and move to a new term
        runMessageLoop(netnode, &params, params.electionTimeout);

        REQUIRE(netnode->context.currentState == State::CANDIDATE);  
        REQUIRE(netnode->context.currentTerm == 3);
    }

    // Candidate (receive majority vote) -> leader
    SECTION("Candiate (receives majority vote) -> leader") {
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.emplace_back(0x64656667, 8200);

        store.write(initializeStore(leaderAddr, nodes));
        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   nodeAddr);
        auto netnode = std::get<0>(nettuple);
        auto mockconn = network->findMockConnection(nodeAddr);
        network->create(nodes[1]);
        auto mockconn2 = network->findMockConnection(nodes[1]);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.electionTimeout);

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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.emplace_back(0x64656667, 8200);

        store.write(initializeStore(leaderAddr, nodes));

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   nodeAddr);
        auto netnode = std::get<0>(nettuple);
        auto mockconn = network->findMockConnection(nodeAddr);
        network->create(nodes[1]);
        auto mockconn2 = network->findMockConnection(nodes[1]);

        network->flush();
        runMessageLoop(netnode, &params, 0);

        REQUIRE(netnode->context.currentState == State::FOLLOWER);

        runMessageLoop(netnode, &params, params.electionTimeout);

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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   leaderAddr);
        auto netnode = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);
        auto conn = network->create(nodeAddr);

        params.resetCurrtime();
        network->reset();

        // Run through the election timeout
        runMessageLoop(netnode, &params, params.electionTimeout);
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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);

        params.resetCurrtime();

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   myAddr,
                                   myAddr);
        auto netnode = std::get<0>(nettuple);
        auto mockconn = network->findMockConnection(myAddr);

        REQUIRE(netnode->context.currentState == Raft::State::CANDIDATE);
        REQUIRE(netnode->member.inited == true);
        REQUIRE(netnode->member.inGroup == true);
        REQUIRE(netnode->member.memberList.size() == 1);
        REQUIRE(netnode->member.isMember(myAddr));

        // Since this became a candidate
        // (check for incremented term)
        REQUIRE(netnode->context.currentTerm == 2);
        // (check for vote for self)
        REQUIRE(myAddr == netnode->context.votedFor);
        // Should still be no leader
        REQUIRE(!netnode->context.currentLeader);
        // (check for RPCs), should be none since only one node
        REQUIRE(mockconn->messagesSent == 0);

        REQUIRE(store.entries.size() == 1);
        // Size is 2 because there is always one empty element
        REQUIRE(store.current["log"].size() == 2);
        REQUIRE(store.current["log"][0]["c"].asInt() == static_cast<int>(Raft::Command::CMD_NOOP));
        REQUIRE(store.current["log"][1]["a"].asString() == myAddr.toAddressString());
        REQUIRE(store.current["log"][1]["p"].asUInt() == myAddr.getPort());
        REQUIRE(store.current["log"][1]["c"].asInt() == static_cast<int>(Raft::Command::CMD_ADD_SERVER));

        // After timeout, it should check the results which would
        // indicate an election success.
        // Note that the check occurs on election timeout (we would
        // check during receiving a vote, but there are no other nodes,
        // thus no other votes).
        runMessageLoop(netnode, &params, params.electionTimeout);

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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);

        params.resetCurrtime();

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   myAddr);
        auto netnode = std::get<0>(nettuple);
        auto mockconn = network->findMockConnection(myAddr);
        auto leaderconn = network->create(leaderAddr);

        // According to our protocol, pass in a null address
        netnode->nodeStart(Address(), 10);

        // Check the startup state
        TERM term = netnode->context.currentTerm;
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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   leaderAddr);
        auto netnode = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);
        auto conn = network->create(myAddr);
        auto mockconn = network->findMockConnection(myAddr);

        params.resetCurrtime();
        network->reset();

        // Run through the election timeout
        runMessageLoop(netnode, &params, params.electionTimeout);
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
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 0);
        REQUIRE(append.prevLogIndex == 0);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].termReceived == 0);
        REQUIRE(append.entries[0].command == Command::CMD_ADD_SERVER);
        REQUIRE(append.entries[0].address == leaderAddr);
        REQUIRE(append.leaderCommit == 1);

        // Send a reply
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, true);
        runMessageLoop(netnode, &params, 0);

        // Should get another AppendEntries
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 0);
        REQUIRE(append.prevLogIndex == 1);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a reply
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, true);
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
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 2);
        REQUIRE(append.prevLogIndex == 2);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a failure reply (we are not fully up-to-date)
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, false);
        runMessageLoop(netnode, &params, 0);

        // Receive the next entry
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 0);
        REQUIRE(append.prevLogIndex == 1);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].termReceived == 2);
        REQUIRE(append.entries[0].command == Command::CMD_ADD_SERVER);
        REQUIRE(append.entries[0].address == myAddr);
        REQUIRE(append.leaderCommit == 1);

        // Send a success
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, true);
        runMessageLoop(netnode, &params, 0);

        // Should expect one more AppendEntries
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 2);
        REQUIRE(append.prevLogTerm == 2);
        REQUIRE(append.prevLogIndex == 2);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 1);

        // Send a success
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, true);
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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   myAddr);
        auto netnode = std::get<0>(nettuple);
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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        HeaderOnlyMessage header;
        AppendEntriesMessage append;
        AppendEntriesReply reply;

        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.push_back(node2Addr);

        store.write(initializeStore(leaderAddr, nodes));

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   leaderAddr);
        auto netnode = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);

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
        REQUIRE(append.term == 3);
        REQUIRE(append.prevLogTerm == 1);
        REQUIRE(append.prevLogIndex == 3);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 3);

        // The nodes are up-to-date with the previous config
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, 1, true);
        runMessageLoop(netnode, &params, 0);

        // Waiting for the update to the current config
        // Receive the next entry
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 3);
        REQUIRE(append.prevLogIndex == 4);
        REQUIRE(append.prevLogTerm == 3);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 3);

        // Send a failure, we don't have the last change
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, false);
        runMessageLoop(netnode, &params, 0);

        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 3);
        REQUIRE(append.prevLogIndex == 3);
        REQUIRE(append.prevLogTerm == 1);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].termReceived == 3);
        REQUIRE(append.entries[0].command == Command::CMD_REMOVE_SERVER);
        REQUIRE(append.entries[0].address == node2Addr);
        REQUIRE(append.leaderCommit == 3);

        // Send a success
        sendAppendEntriesReply(mockconn, leaderAddr,
            append.transId, append.term, true);
        runMessageLoop(netnode, &params, 0);

        // Should expect one more AppendEntries
        raw = mockconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 3);
        REQUIRE(append.prevLogTerm == 3);
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
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<Address> nodes;
        nodes.push_back(nodeAddr);
        nodes.push_back(node2Addr);
        store.write(initializeStore(leaderAddr, nodes));

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   nullptr,
                                   leaderAddr,
                                   nodeAddr);
        auto netnode = std::get<0>(nettuple);
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
    Address     node1Addr(0x64656667, 8100);
    Address     node2Addr(0x64656667, 8200);
    Address     adminAddr(0x64656667, 8999);

    vector<Address> addresses;
    addresses.push_back(node1Addr);
    addresses.push_back(node2Addr);

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    // Main node: 9000, 8100, 8200
    // Nodes: 9000 (leader), 8100, 8200
    // Action: start up 9000 (leader), 8100 and 8200 (followers)
    SECTION("3-node startup") {
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage leaderstore(&params);
        Raft::MemoryBasedStorage store(&params);
        shared_ptr<RaftHandler> leaderHandler;
        vector<shared_ptr<NetworkNode>> nodes;

        leaderstore.write(initializeStore(leaderAddr, vector<Address>()));
        network->installFilter([adminAddr](const MockMessage *mess)->bool
                                { return mess->to != adminAddr; });


        // Create the leader
        auto admin = network->create(adminAddr);
        auto nettuple = createNode(network, &params, &leaderstore, nullptr,
                                   leaderAddr, leaderAddr);
        nodes.push_back(std::get<0>(nettuple));
        leaderHandler = std::get<1>(nettuple);

        // Transition candidate to leader
        runMessageLoop(nodes[0], &params, params.electionTimeout);
        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->member.memberList.size() == 1);

        // Create the two follower nodes
        nettuple = createNode(network, &params, &store, nullptr,
                              leaderAddr, node1Addr);
        nodes.push_back(std::get<0>(nettuple));
        REQUIRE(nodes.back()->context.currentState == State::FOLLOWER);

        nettuple = createNode(network, &params, &store, nullptr,
                              leaderAddr, node2Addr);
        nodes.push_back(std::get<0>(nettuple));
        REQUIRE(nodes.back()->context.currentState == State::FOLLOWER);

        // Tell the leader to add the child nodes
        leaderHandler->onChangeServerCommand(nullptr,
                                             Command::CMD_ADD_SERVER,
                                             node1Addr);

        runMessageLoopUntilSilent(network, nodes, &params);

        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->member.memberList.size() == 2);

        leaderHandler->onChangeServerCommand(nullptr,
                                             Command::CMD_ADD_SERVER,
                                             node2Addr);

        // Let the system settle
        runMessageLoopUntilSilent(network, nodes, &params);

        // Verify the expected system state
        REQUIRE(nodes[0]->member.memberList.size() == 3);
        REQUIRE(nodes[0]->member.isMember(leaderAddr));
        REQUIRE(nodes[0]->member.isMember(node1Addr));
        REQUIRE(nodes[0]->member.isMember(node2Addr));
        REQUIRE(nodes[0]->context.followers.size() == 2);
        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->context.commitIndex == 3);

        // Go through a couple of election timeout cycles
        // Heartbeats should have been sent
        auto leaderconn = network->findMockConnection(leaderAddr);
        int sent = static_cast<int>(leaderconn->messagesSent);

        for (int i=0; i<2*params.idleTimeout; i++)
            runMessageLoop(nodes, &params, 1);

        REQUIRE(leaderconn->messagesSent == (sent+8));
        REQUIRE(nodes[0]->member.isMember(leaderAddr));
        REQUIRE(nodes[0]->member.isMember(node1Addr));
        REQUIRE(nodes[0]->member.isMember(node2Addr));
        REQUIRE(nodes[0]->context.followers.size() == 2);
        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->context.commitIndex == 3);
    }
}

// Test cases for various election scenarios
TEST_CASE("Raft elections", "[raft][election][system]")
{
    Params      params;
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    Address     node1Addr(0x64656667, 8100);
    Address     node2Addr(0x64656667, 8200);
    Address     node3Addr(0x64656667, 8300);
    Address     node4Addr(0x64656667, 8400);

    vector<Address> addresses;
    addresses.push_back(node1Addr);
    addresses.push_back(node2Addr);
    addresses.push_back(node3Addr);

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;

    // election timeout, does the node become a leader and
    // start an election?
    // Main node: 9000, 8100, 8200, 8300
    // Nodes: 9000 (leader), 8100, 8200, 8300
    // Action: start up 9000 (leader), 8100, 8200, 8300 (followers)
    //         9000 fails, 8100 will become leader
    SECTION("4-node startup with leader failure") {
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage leaderstore(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<shared_ptr<NetworkNode>> nodes;

        leaderstore.write(initializeStore(leaderAddr, addresses));

        // Create the leader
        auto nettuple = createNode(network, &params, &leaderstore, nullptr,
                                   leaderAddr, leaderAddr);
        nodes.push_back(std::get<0>(nettuple));

        // create follower nodes
        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node1Addr, -2);
        nodes.push_back(std::get<0>(nettuple));

        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node2Addr);
        nodes.push_back(std::get<0>(nettuple));

        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node3Addr);
        nodes.push_back(std::get<0>(nettuple));

        // Transition candidate to leader
        runMessageLoopUntilSilent(network, nodes, &params);

        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->member.memberList.size() == 4);
        REQUIRE(nodes[0]->context.commitIndex == 0);

        // Need to go through a heartbeat cycle to catch
        // the follower nodes up
        runMessageLoop(nodes, &params, params.idleTimeout);
        runMessageLoopUntilSilent(network, nodes, &params);

        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->member.memberList.size() == 4);
        REQUIRE(nodes[1]->member.memberList.size() == 4);
        REQUIRE(nodes[2]->member.memberList.size() == 4);
        REQUIRE(nodes[3]->member.memberList.size() == 4);
        REQUIRE(nodes[0]->context.commitIndex == 4);
        REQUIRE(nodes[1]->context.commitIndex == 4);
        REQUIRE(nodes[2]->context.commitIndex == 4);
        REQUIRE(nodes[3]->context.commitIndex == 4);

        // Now fail the leader node
        nodes[0]->fail();

        // run until we see some network traffic
        // (should have triggered an election cycle)
        // Node1 has a shorter electionTimeout so it should
        // become a candidate first.
        runMessageLoopUntilActive(network, nodes, &params);

        REQUIRE(nodes[1]->context.currentState == State::CANDIDATE);
        REQUIRE(nodes[2]->context.currentState == State::FOLLOWER);
        REQUIRE(nodes[3]->context.currentState == State::FOLLOWER);

        // The other nodes should elect node[1] as the new leader
        for (int i=0; i<5; i++) {
            runMessageLoop(nodes, &params, 1);
        }

        REQUIRE(nodes[1]->context.currentState == State::LEADER);
        // Even though we have a failed node, it doesn't get
        // removed from the membership list until an admin
        // has told us to remove it.
        REQUIRE(nodes[1]->member.memberList.size() == 4);
        REQUIRE(nodes[2]->context.currentState == State::FOLLOWER);
        REQUIRE(nodes[3]->context.currentState == State::FOLLOWER);
    }

    // split-vote election, backoff test
    // (to partition the network, add a filter).
    // Main node: 9000, 8100, 8200, 8300, 8400
    // Nodes: 9000 (leader), 8100, 8200, 8300, 8400
    // Action: start up 9000 (leader), 8100, 8200, 8300, 8400 (followers)
    //         fail 9000
    //         partition into 2 networks (8100,8200) and (8300,8400)
    //         should get two nodes as candidates 8100 and 8300
    //         rejoin the paritions (8100 should win)
    SECTION("split-vote election") {
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage leaderstore(&params);
        Raft::MemoryBasedStorage store(&params);
        vector<shared_ptr<NetworkNode>> nodes;

        addresses.clear();
        addresses.push_back(node1Addr);
        addresses.push_back(node2Addr);
        addresses.push_back(node3Addr);
        addresses.push_back(node4Addr);

        leaderstore.write(initializeStore(leaderAddr, addresses));

        // Create the leader
        auto nettuple = createNode(network, &params, &leaderstore, nullptr,
                                   leaderAddr, leaderAddr);
        nodes.push_back(std::get<0>(nettuple));

        // create follower nodes
        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node1Addr, -4);
        nodes.push_back(std::get<0>(nettuple));

        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node2Addr);
        nodes.push_back(std::get<0>(nettuple));

        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node3Addr, -2);
        nodes.push_back(std::get<0>(nettuple));

        nettuple = createNode(network, &params, &store, nullptr, leaderAddr, node4Addr);
        nodes.push_back(std::get<0>(nettuple));

        // Transition candidate to leader
        runMessageLoopUntilSilent(network, nodes, &params);

        REQUIRE(nodes[0]->context.currentState == State::LEADER);

        // Need to go through a heartbeat cycle to catch
        // the follower nodes up
        runMessageLoop(nodes, &params, params.idleTimeout);
        runMessageLoopUntilSilent(network, nodes, &params);

        // Partition the network
        map<const Address, int>   partition;
        partition[leaderAddr] = 0;
        partition[node1Addr] = 1;
        partition[node2Addr] = 1;
        partition[node3Addr] = 2;
        partition[node4Addr] = 2;

        nodes[0]->fail();

        network->installFilter([partition](const MockMessage *mess) -> bool
            { return partition.at(mess->from) == partition.at(mess->to); });

        // Let the nodes run through their election
        runMessageLoop(nodes, &params, params.electionTimeout);
        runMessageLoopUntilSilent(network, nodes, &params);

        // neither will have enough votes to win
        REQUIRE(nodes[1]->context.currentState == State::CANDIDATE);
        REQUIRE(nodes[2]->context.currentState == State::FOLLOWER);
        REQUIRE(nodes[3]->context.currentState == State::CANDIDATE);
        REQUIRE(nodes[4]->context.currentState == State::FOLLOWER);

        // restore the network
        network->installFilter(nullptr);
        runMessageLoopUntilActive(network, nodes, &params);
        runMessageLoopUntilSilent(network, nodes, &params);

        // now one node should win
        REQUIRE(nodes[1]->context.currentState == State::LEADER);
        REQUIRE(nodes[2]->context.currentState == State::FOLLOWER);
        REQUIRE(nodes[3]->context.currentState == State::FOLLOWER);
        REQUIRE(nodes[4]->context.currentState == State::FOLLOWER);        
    }
}

// Test cases for log compaction
TEST_CASE("Raft log compaction", "[raft][snapshot]") {
    Params      params;
    Address     node1Addr(0x64656667, 8100); // 100.101.102.103:8100
    Address     node2Addr(0x64656667, 8200); // 100.101.102.103:8200
    Address     node3Addr(0x64656667, 8300); // 100.101.102.103:8300
    Address     adminAddr(0x64656667, 8999); // 100.101.102.103:8999
    Address     leaderAddr(0x64656667, 9000); // 100.101.102.103:9000
    vector<Address> nodes;
    nodes.push_back(node1Addr);
    nodes.push_back(node2Addr);

    // Setup the timeouts
    params.electionTimeout = 10;
    params.idleTimeout = 5;   // not used by the mock network
    params.rpcTimeout = 5;
    params.logCompactionThreshold = 2;
    params.maxSnapshotSize = 1;

    // test snapshot sending
    // Main node: 9000
    // Nodes: 9000 (leader), 8100
    // Action: start up 9000 (leader), test updating via snapshot
    SECTION("basic snapshot update - leader") {
        auto network = MockNetwork::createNetwork(&params);
        auto conn1 = network->create(node1Addr);
        auto conn2 = network->create(node2Addr);
        Raft::MemoryBasedStorage store(&params);
        Raft::MemoryBasedStorage snapshotStore(&params);        
        set<Address>    allowed;

        allowed.insert(leaderAddr);
        allowed.insert(node1Addr);

        network->installFilter([&allowed](const MockMessage *mess)->bool
                { return allowed.count(mess->to) != 0; });

        auto root = initializeStore(leaderAddr, vector<Address>());
        append(root["log"], 2, Command::CMD_ADD_SERVER, node1Addr);
        append(root["log"], 2, Command::CMD_REMOVE_SERVER, node1Addr);
        append(root["log"], 2, Command::CMD_ADD_SERVER, node2Addr);
        append(root["log"], 2, Command::CMD_REMOVE_SERVER, node2Addr);
        root["currentTerm"] = 2;
        store.write(root);

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   &snapshotStore,
                                   leaderAddr,
                                   leaderAddr);
        auto node = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);

        runMessageLoop(node, &params, 0);
        runMessageLoop(node, &params, params.electionTimeout);

        REQUIRE(node->context.currentSnapshot != nullptr);
        REQUIRE(node->context.prevIndex == 5);
        REQUIRE(node->context.logEntries.size() == 0);
        REQUIRE(!node->context.snapshotStore->empty());

        // Now Add a server and look at the updates coming through
        rafthandler->onChangeServerCommand(nullptr,
                                           Command::CMD_ADD_SERVER,
                                           node1Addr);

        // Should update the new server to current config
        // There should be a message to the new server

        // Should attempt to update to previous config
        // Start with an appendEntries
        // Receive empty appendEntries on mockconn
        auto raw = conn1->recv(0);
        REQUIRE(raw != nullptr);
        HeaderOnlyMessage       header;
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT);

        InstallSnapshotMessage install;
        install.load(raw.get());
        REQUIRE(install.term == 4);
        REQUIRE(install.lastIndex == 5);
        REQUIRE(install.lastTerm == 2);
        REQUIRE(install.offset == 0);
        REQUIRE(install.addresses.size() == 1);
        REQUIRE(install.done == true);

        // Send a reply
        sendInstallSnapshotReply(conn1, leaderAddr,
            install.transId, install.term);
        runMessageLoop(node, &params, 0);

        raw = conn1->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);

        AppendEntriesMessage append;
        append.load(raw.get());
        REQUIRE(append.term == 4);
        REQUIRE(append.prevLogIndex == 6);
        REQUIRE(append.prevLogTerm == 4);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 5);

        sendAppendEntriesReply(conn1, leaderAddr,
            append.transId, append.term, false);
        runMessageLoop(node, &params, 0);

        raw = conn1->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);

        append.load(raw.get());
        REQUIRE(append.term == 4);
        REQUIRE(append.prevLogIndex == 5);
        REQUIRE(append.prevLogTerm == 2);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.leaderCommit == 5);

        sendAppendEntriesReply(conn1, leaderAddr,
            append.transId, append.term, true);
        runMessageLoop(node, &params, 0);

        raw = conn1->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        
        append.load(raw.get());
        REQUIRE(append.term == 4);
        REQUIRE(append.prevLogIndex == 6);
        REQUIRE(append.prevLogTerm == 4);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 5);

        sendAppendEntriesReply(conn1, leaderAddr,
                               append.transId, append.term, true);
        runMessageLoop(node, &params, 0);
    }

    // test snapshot sending
    // Main node: 9000
    // Nodes: 9000 (leader), 8100, 8200
    // Action: start up 9000 (leader), test updating via snapshot
    // should be sending multiple InstallSnapshot messages
    SECTION("basic snapshot update multiple messages - leader") {
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage store(&params);
        Raft::MemoryBasedStorage snapshotStore(&params);     
        vector<TermNode> termNodes;
        set<Address>    allowed;

        allowed.insert(leaderAddr);
        allowed.insert(node1Addr);
        allowed.insert(node2Addr);
        allowed.insert(node3Addr);

        termNodes.emplace_back(node1Addr);
        termNodes.emplace_back(node2Addr);

        network->installFilter([&allowed](const MockMessage *mess)->bool
                { return allowed.count(mess->to) != 0; });

        auto root = initializeStore(leaderAddr, vector<Address>());
        root["currentTerm"] = 2;
        store.write(root);

        auto nettuple = createCluster(network, &params, &store, &snapshotStore,
                                      leaderAddr, termNodes);
        auto node = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);
        auto conn3 = network->create(node3Addr);

        REQUIRE(node->member.memberList.size() == 3);
        REQUIRE(node->context.currentSnapshot != nullptr);
        REQUIRE(node->context.prevIndex == 3);
        REQUIRE(node->context.commitIndex == 3);

        rafthandler->onChangeServerCommand(nullptr,
                                           Command::CMD_ADD_SERVER,
                                           node3Addr);
        runMessageLoop(node, &params, 1);

        REQUIRE(node->context.currentSnapshot != nullptr);
        REQUIRE(node->context.prevIndex == 3);

        // Should see the app try to update node3
        auto raw = conn3->recv(0);
        REQUIRE(raw != nullptr);
        HeaderOnlyMessage       header;
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT);

        InstallSnapshotMessage install;
        install.load(raw.get());
        REQUIRE(install.term == 4);
        REQUIRE(install.lastIndex == 3);
        REQUIRE(install.lastTerm == 4);
        REQUIRE(install.offset == 0);
        REQUIRE(install.addresses.size() == 1);
        REQUIRE(install.done == false);

        sendInstallSnapshotReply(conn3, leaderAddr,
            install.transId, install.term);
        runMessageLoop(node, &params, 0);

        raw = conn3->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT);
        install.load(raw.get());
        REQUIRE(install.term == 4);
        REQUIRE(install.lastIndex == 3);
        REQUIRE(install.lastTerm == 4);
        REQUIRE(install.offset == 1);
        REQUIRE(install.addresses.size() == 1);
        REQUIRE(install.done == false);

        sendInstallSnapshotReply(conn3, leaderAddr,
            install.transId, install.term);
        runMessageLoop(node, &params, 0);

        raw = conn3->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT);
        install.load(raw.get());
        REQUIRE(install.term == 4);
        REQUIRE(install.lastIndex == 3);
        REQUIRE(install.lastTerm == 4);
        REQUIRE(install.offset == 2);
        REQUIRE(install.addresses.size() == 1);
        REQUIRE(install.done == true);

        sendInstallSnapshotReply(conn3, leaderAddr,
            install.transId, install.term);
        runMessageLoop(node, &params, 0);
        handleNodeUpdates(network, &params, node, leaderAddr, termNodes);

        // Ensure that we've gone past the snapshot phase
        raw = conn3->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        AppendEntriesMessage append;
        append.load(raw.get());
        REQUIRE(append.term == 4);
        REQUIRE(append.prevLogIndex == 4);
        REQUIRE(append.prevLogTerm == 4);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 3);

        sendAppendEntriesReply(conn3, leaderAddr,
            append.transId, append.term, false);
        runMessageLoop(node, &params, 0);

        raw = conn3->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 4);
        REQUIRE(append.prevLogIndex == 3);
        REQUIRE(append.prevLogTerm == 4);
        REQUIRE(append.entries.size() == 1);
        REQUIRE(append.entries[0].address == node3Addr);
        REQUIRE(append.leaderCommit == 4);

        sendAppendEntriesReply(conn3, leaderAddr,
            append.transId, append.term, true);
        runMessageLoop(node, &params, 0);

        raw = conn3->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES);
        append.load(raw.get());
        REQUIRE(append.term == 4);
        REQUIRE(append.prevLogIndex == 4);
        REQUIRE(append.prevLogTerm == 4);
        REQUIRE(append.entries.size() == 0);
        REQUIRE(append.leaderCommit == 4);
    }

    // test snapshot sending
    // Main node: 8100
    // Nodes: 9000 (leader), 8100
    // Action: start up 8100, test updating via snapshot
    SECTION("basic snapshot update - follower") {
        auto network = MockNetwork::createNetwork(&params);
        auto leaderconn = network->create(leaderAddr);
        Raft::MemoryBasedStorage store(&params);
        Raft::MemoryBasedStorage snapshotStore(&params);        

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   &snapshotStore,
                                   leaderAddr,
                                   node1Addr);
        auto node = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);
        auto conn1 = network->find(node1Addr);

        runMessageLoop(node, &params, 0);

        REQUIRE(node->context.currentSnapshot == nullptr);
        REQUIRE(node->context.prevIndex == 0);

        // Now assume that we're a new server that is
        // updating the follower
        vector<Address> data;
        data.push_back(leaderAddr);

        sendInstallSnapshot(leaderconn, node1Addr, leaderAddr,
                            1 /*transid*/, 4 /*term*/,
                            5 /*lastIndex*/, 2 /*lastTerm*/,
                            0 /*offset*/, true /*done*/, data);
        runMessageLoop(node, &params, 0);

        auto raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        HeaderOnlyMessage header;
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT_REPLY);
        InstallSnapshotReply  ireply;
        ireply.load(raw.get());

        REQUIRE(node->member.memberList.size() == 1);
        REQUIRE(node->context.prevIndex == 5);
        REQUIRE(node->context.currentSnapshot != nullptr);
        REQUIRE(node->context.commitIndex == 5);
        REQUIRE(node->context.lastAppliedIndex == 5);
        REQUIRE(node->context.logEntries.size() == 0);

        // Send an append entries
        sendAppendEntries(leaderconn, node1Addr,
                          2 /*transid*/, 4 /*term */,
                          leaderAddr, 5 /* lastIndex*/, 2 /*lastTerm*/,
                          5 /*commitIndex*/);
        runMessageLoop(node, &params, 0);

        raw = leaderconn->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        AppendEntriesReply reply;
        reply.load(raw.get());
        REQUIRE(reply.success == true);

        //$ TODO : move this test to an error section test
        // Try it with a bad lastTerm
        sendAppendEntries(leaderconn, node1Addr,
                          2 /*transid*/, 4 /*term */,
                          leaderAddr, 5 /* lastIndex*/, 22 /*lastTerm*/,
                          5 /*commitIndex*/);
        runMessageLoop(node, &params, 0);

        raw = leaderconn->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        REQUIRE(reply.success == false);

        //$ TODO: move this to an error section test
        // Try it with an index past the end
        sendAppendEntries(leaderconn, node1Addr,
                          2 /*transid*/, 4 /*term */,
                          leaderAddr, 55 /* lastIndex*/, 2 /*lastTerm*/,
                          5 /*commitIndex*/);
        runMessageLoop(node, &params, 0);

        raw = leaderconn->recv(0);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::APPEND_ENTRIES_REPLY);
        reply.load(raw.get());
        REQUIRE(reply.success == false);
    }

    SECTION("basic snapshot multi message update - follower") {
        // Create the leader
        auto network = MockNetwork::createNetwork(&params);
        auto leaderconn = network->create(leaderAddr);
        Raft::MemoryBasedStorage store(&params);
        Raft::MemoryBasedStorage snapshotStore(&params);        

        auto nettuple = createNode(network,
                                   &params,
                                   &store,
                                   &snapshotStore,
                                   leaderAddr,
                                   node1Addr);
        auto node = std::get<0>(nettuple);
        auto rafthandler = std::get<1>(nettuple);
        auto conn1 = network->find(node1Addr);

        runMessageLoop(node, &params, 0);

        REQUIRE(node->context.currentSnapshot == nullptr);
        REQUIRE(node->context.prevIndex == 0);

        // Now assume that we're a new server that is
        // updating the follower
        vector<Address> data;
        data.push_back(leaderAddr);

        sendInstallSnapshot(leaderconn, node1Addr, leaderAddr,
                            1 /*transid*/, 4 /*term*/,
                            5 /*lastIndex*/, 2 /*lastTerm*/,
                            0 /*offset*/, false /*done*/, data);
        runMessageLoop(node, &params, 0);

        REQUIRE(node->member.memberList.size() == 0);
        REQUIRE(node->context.prevIndex == 0);
        REQUIRE(node->context.currentSnapshot == nullptr);
        REQUIRE(node->context.commitIndex == 0);
        REQUIRE(node->context.lastAppliedIndex == 0);
        REQUIRE(node->context.logEntries.size() == 1);

        auto raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        HeaderOnlyMessage header;
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT_REPLY);

        data.clear();
        data.push_back(node2Addr);

        sendInstallSnapshot(leaderconn, node1Addr, leaderAddr,
                            1 /*transid*/, 4 /*term*/,
                            5 /*lastIndex*/, 2 /*lastTerm*/,
                            1 /*offset*/, true /*done*/, data);
        runMessageLoop(node, &params, 0);

        REQUIRE(node->member.memberList.size() == 2);
        REQUIRE(node->context.prevIndex == 5);
        REQUIRE(node->context.currentSnapshot != nullptr);
        REQUIRE(node->context.commitIndex == 5);
        REQUIRE(node->context.lastAppliedIndex == 5);
        REQUIRE(node->context.logEntries.size() == 0);

        raw = leaderconn->recv(0);
        REQUIRE(raw != nullptr);
        header.load(raw.get());
        REQUIRE(header.msgtype == MessageType::INSTALL_SNAPSHOT_REPLY);
    }

    SECTION("basic snapshot update - system test") {
        // Create the leader
        // Create child nodes, let them run
        // Check the state of the nodes
        auto network = MockNetwork::createNetwork(&params);
        Raft::MemoryBasedStorage leaderstore(&params);
        Raft::MemoryBasedStorage store(&params);
        Raft::MemoryBasedStorage snapshotStore(&params);
        shared_ptr<RaftHandler> leaderHandler;
        vector<shared_ptr<NetworkNode>> nodes;

        auto root = initializeStore(leaderAddr, vector<Address>());
        append(root["log"], 2, Command::CMD_ADD_SERVER, node1Addr);
        append(root["log"], 2, Command::CMD_REMOVE_SERVER, node1Addr);
        append(root["log"], 2, Command::CMD_ADD_SERVER, node2Addr);
        append(root["log"], 2, Command::CMD_REMOVE_SERVER, node2Addr);
        root["currentTerm"] = 2;
        leaderstore.write(root);

        // Create the leader
        auto nettuple = createNode(network, &params, &leaderstore, &snapshotStore,
                                   leaderAddr, leaderAddr);
        nodes.push_back(std::get<0>(nettuple));
        leaderHandler = std::get<1>(nettuple);

        // Transition candidate to leader
        runMessageLoop(nodes[0], &params, params.electionTimeout);
        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->member.memberList.size() == 1);

        // Create the two follower nodes
        nettuple = createNode(network, &params, &store, &snapshotStore,
                              leaderAddr, node1Addr);
        nodes.push_back(std::get<0>(nettuple));
        REQUIRE(nodes.back()->context.currentState == State::FOLLOWER);

        nettuple = createNode(network, &params, &store, &snapshotStore,
                              leaderAddr, node2Addr);
        nodes.push_back(std::get<0>(nettuple));
        REQUIRE(nodes.back()->context.currentState == State::FOLLOWER);

        // Tell the leader to add the child nodes
        leaderHandler->onChangeServerCommand(nullptr,
                                             Command::CMD_ADD_SERVER,
                                             node1Addr);
        runMessageLoopUntilSilent(network, nodes, &params);

        leaderHandler->onChangeServerCommand(nullptr,
                                             Command::CMD_ADD_SERVER,
                                             node2Addr);
        runMessageLoopUntilSilent(network, nodes, &params);

        REQUIRE(nodes[0]->context.currentState == State::LEADER);
        REQUIRE(nodes[0]->member.memberList.size() == 3);
        REQUIRE(nodes[0]->context.currentSnapshot != nullptr);
        REQUIRE(nodes[0]->context.prevIndex == 7);
        REQUIRE(nodes[0]->context.commitIndex == 7);
        REQUIRE(nodes[1]->context.currentSnapshot != nullptr);
        REQUIRE(nodes[1]->context.prevIndex == 5);
        REQUIRE(nodes[1]->context.commitIndex == 6);
        REQUIRE(nodes[2]->context.currentSnapshot != nullptr);
        REQUIRE(nodes[2]->context.prevIndex == 5);
        REQUIRE(nodes[2]->context.commitIndex == 6);

        // Need one more heartbeat for the commits to catch up
        runMessageLoop(nodes[0], &params, params.electionTimeout);
        runMessageLoopUntilSilent(network, nodes, &params);
        REQUIRE(nodes[0]->context.commitIndex == 7);
        REQUIRE(nodes[1]->context.commitIndex == 7);
        REQUIRE(nodes[2]->context.commitIndex == 7);
    }
}

// Test cases for log operations
// Test cases for failover scenarios
TEST_CASE("Raft failover", "[raft][failover]") {
    // Test startup with a follower (and a leader that is down)
    // have the follower redirect the AddServer (which will fail)
    // timeout and then try again
}


