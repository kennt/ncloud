/*****
 * Raft.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Raft.h"
#include "NetworkNode.h"
#include "json/json.h"

// Timeout for the join operation
const int JOIN_TIMEOUT = 10;

using namespace Raft;

unique_ptr<RawMessage> AppendEntriesMessage::toRawMessage(const Address &from,
                                                          const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<int>(ss, this->term);
    write_raw<Address>(ss, this->leaderAddress);
    write_raw<int>(ss, this->prevLogIndex);
    write_raw<int>(ss, this->prevLogTerm);
    write_raw<int>(ss, this->leaderCommit);
    write_raw<int>(ss, static_cast<int>(this->entries.size()));
    for (auto & logEntry : entries) {
        logEntry.write(ss);
    }

    return rawMessageFromStream(from, to, ss);
}

void AppendEntriesMessage::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::APPEND_ENTRIES);

    this->term = read_raw<int>(is);
    this->leaderAddress = read_raw<Address>(is);
    this->prevLogIndex = read_raw<int>(is);
    this->prevLogTerm = read_raw<int>(is);
    this->leaderCommit = read_raw<int>(is);

    size_t size = static_cast<size_t>(read_raw<int>(is));
    this->entries.clear();
    for (size_t i=0; i<size; i++) {
        RaftLogEntry    logEntry;
        logEntry.read(is);
        this->entries.push_back(logEntry);
    }
}

unique_ptr<RawMessage> AppendEntriesReply::toRawMessage(const Address &from,
                                                        const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<int>(ss, this->term);
    write_raw<bool>(ss, this->success);

    return rawMessageFromStream(from, to, ss);
}

void AppendEntriesReply::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::APPEND_ENTRIES_REPLY);

    this->term = read_raw<int>(is);
    this->success = read_raw<bool>(is);
}

unique_ptr<RawMessage> RequestVoteMessage::toRawMessage(const Address &from,
                                                        const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<int>(ss, this->term);
    write_raw<Address>(ss, this->candidateAddress);
    write_raw<int>(ss, this->lastLogIndex);
    write_raw<int>(ss, this->lastLogTerm);

    return rawMessageFromStream(from, to, ss);
}

void RequestVoteMessage::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::REQUEST_VOTE);

    this->term = read_raw<int>(is);
    this->candidateAddress = read_raw<Address>(is);
    this->lastLogIndex = read_raw<int>(is);
    this->lastLogTerm = read_raw<int>(is);
}

// Converts the data in a RequestVoteReply structure into a RawMessage (which
// is ready to be sent over the wire).
//
// The pointer returned is a unique_ptr<> and should be freed by the caller.
//
unique_ptr<RawMessage> RequestVoteReply::toRawMessage(const Address &from,
                                                      const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<int>(ss, this->term);
    write_raw<bool>(ss, this->voteGranted);

    return rawMessageFromStream(from, to, ss);
}

// Loads the streamed binary data from the istringstream into a
// RequestVoteReply.  It is assumed that the position of the stream is
// at the head of the message.
//
void RequestVoteReply::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::REQUEST_VOTE_REPLY);

    this->term = read_raw<int>(is);
    this->voteGranted = read_raw<bool>(is);
}


unique_ptr<RawMessage> AddServerMessage::toRawMessage(const Address &from,
                                                      const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<Address>(ss, this->newServer);

    return rawMessageFromStream(from, to, ss);
}

void AddServerMessage::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::ADD_SERVER);

    this->newServer = read_raw<Address>(is);
}

unique_ptr<RawMessage> AddServerReply::toRawMessage(const Address &from,
                                                    const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<bool>(ss, this->status);
    write_raw<Address>(ss, this->leaderHint);

    return rawMessageFromStream(from, to, ss);
}

void AddServerReply::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::ADD_SERVER_REPLY);

    this->status = read_raw<bool>(is);
    this->leaderHint = read_raw<Address>(is);
}

unique_ptr<RawMessage> RemoveServerMessage::toRawMessage(const Address &from,
                                                         const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<Address>(ss, this->oldServer);

    return rawMessageFromStream(from, to, ss);
}

void RemoveServerMessage::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::REMOVE_SERVER);

    this->oldServer = read_raw<Address>(is);
}

unique_ptr<RawMessage> RemoveServerReply::toRawMessage(const Address& from,
                                                       const Address& to)
{
    stringstream    ss;

    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);

    write_raw<bool>(ss, this->status);
    write_raw<Address>(ss, this->leaderHint);

    return rawMessageFromStream(from, to, ss);
}

void RemoveServerReply::load(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    assert(this->msgtype == MessageType::REMOVE_SERVER_REPLY);

    this->status = read_raw<bool>(is);
    this->leaderHint = read_raw<Address>(is);
}


void RaftMessageHandler::start(const Address &joinAddress)
{
    if (joinAddress.isZero())
        throw AppException("zero join address is not allowed");

    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    node->member.inGroup = false;
    node->member.inited = true;

    node->member.memberList.clear();

    joinGroup(joinAddress);
}

// This is a callback and is called when the connection has received
// a message.
//
// The RawMessage will not be changed.
//
void RaftMessageHandler::onMessageReceived(const RawMessage *raw)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    istringstream ss(std::string((const char *)raw->data.get(), raw->size));

    MessageType msgtype = static_cast<MessageType>(read_raw<int>(ss));
    ss.seekg(0, ss.beg);    // reset to start of the buffer

    switch(msgtype) {
        case MessageType::APPEND_ENTRIES:
            onAppendEntries(raw->fromAddress, ss);
            break;
        case MessageType::REQUEST_VOTE:
            onRequestVote(raw->fromAddress, ss);
            break;
        case MessageType::ADD_SERVER:
            onAddServer(raw->fromAddress, ss);
            break;
        case MessageType::ADD_SERVER_REPLY:
            onAddServerReply(raw->fromAddress, ss);
            break;
        default:
            throw NetworkException(string_format("Unknown message type: %d", msgtype).c_str());
            break;
    }
}

void RaftMessageHandler::onAppendEntries(const Address& from, istringstream& ss)
{
    DEBUG_LOG(log, connection->address(), "Append Entries received from %s",
              from.toString().c_str());

    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    shared_ptr<AppendEntriesMessage> message = make_shared<AppendEntriesMessage>();
    message->load(ss);

    AppendEntriesReply  reply;

    if (message->term < node->context.currentTerm) {
        reply.success = false;
        reply.term = node->context.currentTerm;
    }
    else if (!node->context.raftLog.contains(message->prevLogIndex, message->prevLogTerm)) {
        reply.success = false;
        //$ TODO: Do I need to reply with the current term?
        throw NYIException("Raft AppendEntries", __FILE__, __LINE__);
    }
}

void RaftMessageHandler::onRequestVote(const Address& from, istringstream& ss)
{
    DEBUG_LOG(log, connection->address(), "Request Vote received from %s",
                from.toString().c_str());

    RequestVoteMessage    message;
    message.load(ss);

    RequestVoteReply    reply;
}

void RaftMessageHandler::onAddServer(const Address& from, istringstream& ss)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    AddServerMessage    request;
    AddServerReply      reply;

    request.load(ss);
    if (node->context.currentState != State::LEADER) {
        reply.status = false;
        reply.leaderHint = node->context.leaderAddress;
    }
    else {
        // We are the leader, so go ahead and add the member
        node->context.addMember(request.newServer);
        
        // Send a reply
        reply.status = true;
    }

    // Send the reply
    auto raw = request.toRawMessage(connection->address(), from);
    connection->send(raw.get());
}

void RaftMessageHandler::onAddServerReply(const Address& from, istringstream& ss)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    // This should only occur if we are trying to join
    // a group and have not been initialized yet.
    assert(node->context.currentState == State::NONE);

    AddServerReply  reply;
    reply.load(ss);

    if (reply.status) {
        // Success! we are part of the cluster
        node->context.leaderAddress = from;
        node->member.inGroup = true;

        node->context.init(this, store);

        // The membership list will get updated when we receive
        // APPEND_ENTRIES messages        
    }
    else {
        // Node join failed!
        if (reply.leaderHint.isZero()) {
            // There is nowhere to go, log an error and quit
            DEBUG_LOG(log, connection->address(), "Failed to join, no leader hint");
            node->quit();
            return;
        }

        // Try to join the cluster again, but with a different
        // leader address
        joinGroup(reply.leaderHint);
    }
}

// This has to implement the same functionality as the regular
// AddServer() but with the added responsibility of sending
// a reply to the CommandMessage.
//
// We do not forward the request, just return a failed notification,
// with the leader address in the address portion.
//
void RaftMessageHandler::onAddServerCommand(shared_ptr<CommandMessage> command, const Address& address)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    shared_ptr<CommandMessage>  reply;

    if (node->context.currentState != State::LEADER) {
        reply = command->makeReply(false);

        // Send back what we think is the leader
        reply->address = node->context.leaderAddress;
    }
    else {
        // Send a reply
        reply = command->makeReply(true);

        // If we are the leader
        node->context.addMember(address);
    }

    // Send the reply
    auto raw = reply->toRawMessage(connection->address(), command->from);
    connection->send(raw.get());
}

void RaftMessageHandler::onRemoveServerCommand(shared_ptr<CommandMessage> command, const Address& address)
{
}


// This is called when there are no messages available (usually on a
// connection timeout).  Thus perform any actions that should be done
// on idle here.
//
void RaftMessageHandler::onTimeout()
{
    // run the node maintenance loop
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    if (node->context.currentState == State::JOINING) {
        // Check the transaction list for a add server request
        // if this has failed, then quit

        // We have timed out on joining the cluster.
        // Nothing to do but to quit
        DEBUG_LOG(log, connection->address(), "Timeout on joining the cluster");
        node->quit();
        return;
    }

    // Wait until you're in the group
    if (!node->member.inGroup)
        return;

    // If leader, send heartbeats
    if (node->context.currentState == State::LEADER && nextHeartbeat > par->getCurrtime()) {
        //$ TODO: send heartbeat to all followers
        nextHeartbeat = par->getCurrtime() + par->heartbeatTimeout;
    }
}

// Joins the node to the current group.  The action performed is
// different depending on whether or not this node is the coordinator
// node or not.
//
void RaftMessageHandler::joinGroup(const Address & joinAddress)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    if (joinAddress == connection->address() || joinAddress.isZero()) {
        // This should only be called when we are the first process to
        // startup.
        // If joinAddress is all zeros, then we are either the first
        // node or we have to wait until we are contacted by a leader.
        // In either case, the startup process is the same.
        //
        DEBUG_LOG(log, connection->address(), "Starting up group...");
        node->context.init(this, store);

        // add ourselves as a member to the group
        node->context.addMember(connection->address());

        // Setup the timeout callbacks
        this->electionTimeout = par->getCurrtime() + par->electionTimeout;
        this->heartbeatTimeout = par->getCurrtime() + par->heartbeatTimeout;
    }
    else {
        assert(!joinAddress.isZero());

        AddServerMessage    request;

        request.newServer = connection->address();
        auto raw = request.toRawMessage(connection->address(), joinAddress);

        DEBUG_LOG(log, connection->address(), "Trying to join...");

        //context.change(State::JOINING);

        // Create a transaction, however, the timeout on this transaction
        // will cause the server to shutdown.

        //joinTimeout = par->getCurrtime() + JOIN_TIMEOUT;
        connection->send(raw.get());
    }
}

// The actual Raft state machine (for our application) is
// actually the membership list.  This is kept separate from
// the leader-kept follower list.
//
void RaftMessageHandler::applyLogEntry(const RaftLogEntry& entry)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    switch(entry.command) {
        case CMD_ADD_SERVER:
            node->member.addToMemberList(
                entry.address,
                par->getCurrtime(),
                0);
            break;
        case CMD_REMOVE_SERVER:
            node->member.removeFromMemberList(
                entry.address);
            break;
        default:
            throw NetworkException("Unknown log entry command!");
            break;
    }
}

