/*****
 * TestRaftUtil.cpp
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


void runMessageLoop(shared_ptr<NetworkNode> node, Params *par, int tm)
{
    par->addToCurrtime(tm);
    node->receiveMessages();
    node->processQueuedMessages();
}

void runMessageLoop(vector<shared_ptr<NetworkNode>>& nodes, Params *par, int tm)
{
    par->addToCurrtime(tm);
    for (auto & node : nodes) {
        runMessageLoop(node, par, 0);
    }
}

void runMessageLoopUntilSilent(shared_ptr<MockNetwork> network,
                               vector<shared_ptr<NetworkNode>>& nodes,
                               Params *par)
{
    while(network->messages.size() > 0) {
        runMessageLoop(nodes, par, 1);
    }
}

void runMessageLoopUntilActive(shared_ptr<MockNetwork> network,
                               vector<shared_ptr<NetworkNode>>& nodes,
                               Params *par)
{
    while (network->messages.size() == 0) {
        runMessageLoop(nodes, par, 1);
    }
}

void flushMessages(shared_ptr<IConnection> conn)
{
    while (conn->recv(0))
        ;
}

void sendAppendEntries(shared_ptr<IConnection> conn,
                       const Address& addr,
                       int transid,
                       TERM term,
                       const Address& leaderAddr,
                       INDEX lastIndex,
                       TERM lastTerm,
                       INDEX commitIndex,
                       Raft::RaftLogEntry *entry)
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

void sendAppendEntriesReply(shared_ptr<IConnection> conn,
                            const Address& addr,
                            int transid,
                            TERM term,
                            bool success)
{
    AppendEntriesReply reply;
    reply.transId = transid;
    reply.term = term;
    reply.success = success;

    auto raw = reply.toRawMessage(conn->address(), addr);
    conn->send(raw.get());
}

void sendInstallSnapshot(shared_ptr<IConnection> conn,
                         const Address& addr,
                         const Address& leader,
                         int transid,
                         TERM term,
                         INDEX lastIndex,
                         TERM lastTerm,
                         INDEX offset,
                         bool done,
                         const vector<Address>& addresses)
{
    InstallSnapshotMessage install;
    install.transId = transid;
    install.term = term;
    install.leaderAddress = leader;
    install.lastIndex = lastIndex;
    install.lastTerm = lastTerm;
    install.offset = offset;
    install.done = done;
    install.addresses = addresses;

    auto raw = install.toRawMessage(conn->address(), addr);
    conn->send(raw.get());
}

void sendInstallSnapshotReply(shared_ptr<IConnection> conn,
                              const Address& addr,
                              int transid,
                              TERM term)
{
    InstallSnapshotReply reply;
    reply.transId = transid;
    reply.term = term;

    auto raw = reply.toRawMessage(conn->address(), addr);
    conn->send(raw.get());
}

void append(Json::Value& value, TERM term, Command command, const Address& addr)
{
    Json::Value logEntry;
    
    logEntry["t"] = static_cast<Json::UInt>(term);
    logEntry["c"] = static_cast<int>(command);
    logEntry["a"] = addr.toAddressString();
    logEntry["p"] = addr.getPort();

    value.append(logEntry);
}

// Initializes the store with the data to start up
// a node with the specified cluster configuration.
// The leader gets added at term 0.  All other nodes
// are added at term 1 by default.
Json::Value initializeStore(const Address& leader,
                            const vector<Address>& nodes)
{   
    Json::Value root;
    Address     nullAddress;

    root["currentTerm"] = 1;
    root["votedFor"] = nullAddress.toAddressString();
    root["votedForPort"] = nullAddress.getPort();

    Json::Value log;

    append(log, 0, Command::CMD_NOOP, Address());
    append(log, 0, Command::CMD_ADD_SERVER, leader);

    for (auto & address: nodes)
        append(log, 1, Command::CMD_ADD_SERVER, address);

    root["log"] = log;

    return root;
}

// This will instantiate a single node with a MockNetwork.
// It will simulate the leaders and followers to get the
// instantiated node into the desired state.
// To start a node as a leader, let leaderAddr = nodeAddr;
// nodeAddr must ALWAYS be supplied.
//
// Parameters
//  network
//  par
//  store - the context store to use for the node
//      To setup the store for a mulit-node cluster,
//      use initializeStore().
//  leaderAddr - the address of the startup leader
//      may be the same as nodeAddr
//  nodeAddr - the main node that will be created
//
// Returns: a tuple
//  <0> : shared pointer to the NetworkNode
//  <1> : shared pointer to the RaftHandler
//
std::tuple<shared_ptr<NetworkNode>, shared_ptr<RaftHandler>> 
createNode(shared_ptr<MockNetwork> network,
           Params *par,
           Raft::StorageInterface *store,
           Raft::StorageInterface *snapshotStore,
           const Address& leaderAddr,
           const Address& nodeAddr,
           int timeoutModifier)
{
    string  name("mock");
    auto conn = network->create(nodeAddr);
    auto netnode = make_shared<NetworkNode>(name, nullptr, par, network);
    auto rafthandler = make_shared<Raft::RaftHandler>(nullptr, par, store, 
                            snapshotStore, netnode, conn);
    rafthandler->setElectionTimeoutModifier(timeoutModifier);
    netnode->registerHandler(ConnectType::MEMBER,
                             conn,
                             rafthandler);

    netnode->nodeStart(leaderAddr, par->idleTimeout);

    return std::make_tuple(netnode, rafthandler);
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
    runMessageLoop(netnode, par, par->electionTimeout);
}


void handleNodeUpdates(shared_ptr<MockNetwork> network,
                       Params *par,
                       shared_ptr<NetworkNode> netnode,
                       const Address& leaderAddr,
                       const vector<Address>& addresses)
{
    vector<TermNode> nodes;
    for (auto & addr : addresses) {
        nodes.emplace_back(addr);
    }
    handleNodeUpdates(network, par, netnode, leaderAddr, nodes);
}

// Simulates the child nodes accepting AppendEntries until
// they are "up-to-date".  This runs until there are no more
// messages for any of the nodes in the list.
// Does not do any validation of the messages.
void handleNodeUpdates(shared_ptr<MockNetwork> network,
                       Params *par,
                       shared_ptr<NetworkNode> netnode,
                       const Address& leaderAddr,
                       vector<TermNode>& nodes)
{
    // Have each node pull off data
    AppendEntriesMessage    append;
    AppendEntriesReply      reply;
    HeaderOnlyMessage       header;
    InstallSnapshotMessage  install;
    InstallSnapshotReply    installreply;
    bool                    messageFound = true;

    while (messageFound) {
        messageFound = false;

        for (auto & node : nodes) {
            auto conn = network->find(node.address);
            if (!conn)
                continue;
            auto raw = conn->recv(0);;

            for (; raw != nullptr; raw = conn->recv(0)) {
                messageFound = true;

                header.load(raw.get());
                if (header.msgtype == MessageType::INSTALL_SNAPSHOT) {
                    install.load(raw.get());
                    node.terms.resize(install.lastIndex+1);
                    node.terms[install.lastIndex] = install.lastTerm;
    
                    installreply.transId = install.transId;
                    installreply.term = install.term;
                    auto rawreply = installreply.toRawMessage(node.address, leaderAddr);
                    conn->send(rawreply.get());
                }
                else {
                    append.load(raw.get());
    
                    if (append.prevLogIndex > (node.terms.size()-1))
                        reply.success = false;
                    else if (node.terms[append.prevLogIndex] != append.prevLogTerm)
                        reply.success = false;
                    else {
                        node.terms.resize(append.prevLogIndex+1);
                        for (auto & entry : append.entries) {
                            node.terms.push_back(entry.termReceived);
                        }
                        reply.success = true;
                    }
                    reply.transId = append.transId;
                    reply.term = append.term;
                    auto rawreply = reply.toRawMessage(node.address, leaderAddr);
                    conn->send(rawreply.get());
                }
                
                runMessageLoop(netnode, par, 0);
            }
        }
    }
}

std::tuple<shared_ptr<NetworkNode>, shared_ptr<RaftHandler>> 
createCluster(shared_ptr<MockNetwork> network,
                   Params *par,
                   Raft::StorageInterface *store,
                   Raft::StorageInterface *snapshotStore,
                   const Address& leaderAddr,
                   vector<TermNode>& nodes)
{
    auto nettuple = createNode(network,
                               par,
                               store,
                               snapshotStore,
                               leaderAddr,
                               leaderAddr);
    auto netnode = std::get<0>(nettuple);
    auto rafthandler = std::get<1>(nettuple);

    runMessageLoop(netnode, par, 0);
    runMessageLoop(netnode, par, par->electionTimeout);

    for (auto & node : nodes) {
        if (network->find(node.address) == nullptr)
            network->create(node.address);
        rafthandler->onChangeServerCommand(nullptr,
                                           Command::CMD_ADD_SERVER,
                                           node.address);
        handleNodeUpdates(network, par, netnode, leaderAddr, nodes);
    }
    return nettuple;
}
