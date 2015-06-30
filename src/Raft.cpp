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

const int TREMOVE 	= 20;
const int TFAIL 	= 5;
const int JOIN_TIMEOUT = 10;

using namespace Raft;

unique_ptr<RawMessage> AppendEntriesMessage::toRawMessage(const Address &from,
                                                          const Address& to)
{
    return nullptr;
}

void AppendEntriesMessage::load(istringstream& ss)
{
}

unique_ptr<RawMessage> AppendEntriesReply::toRawMessage(const Address &from,
                                                        const Address& to)
{
    return nullptr;
}

void AppendEntriesReply::load(istringstream& ss)
{
}

unique_ptr<RawMessage> RequestVoteMessage::toRawMessage(const Address &from,
                                                        const Address& to)
{
    return nullptr;
}

void RequestVoteMessage::load(istringstream& ss)
{
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

    write_raw<int>(ss, static_cast<int>(MessageType::REQUEST_VOTE_REPLY));
    write_raw<int>(ss, this->term);
    write_raw<int>(ss, this->voteGranted ? 1 : 0);

    return rawMessageFromStream(from, to, ss);
}

// Loads the streamed binary data from the istringstream into a
// RequestVoteReply.  It is assumed that the position of the stream is
// at the head of the message.
//
void RequestVoteReply::load(istringstream& ss)
{
    int     msgtype = read_raw<int>(ss);
    if (msgtype != MessageType::REQUEST_VOTE_REPLY)
        throw NetworkException("incorrect message type");

    int term = read_raw<int>(ss);
    int voteGranted = read_raw<int>(ss);

    this->term = term;
    this->voteGranted = (voteGranted != 0);
}


// Initializes the message handler.  Call this before any calls to
// onMessageReceived() or onTimeout().  Or if the connection has been reset.
//
void RaftMessageHandler::start(const Address &joinAddress)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    node->member.inGroup = false;
    node->member.inited = true;

    node->member.memberList.clear();

    // Nodes always start in uninitialized
    // They need to be joined to the group first
    // In our model, nodes add themselves by sending an
    // ADD_SERVER message to the leader.
    currentState = State::NONE;
    joinTimeout = 0;

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

    MEMBER_MSGTYPE msgtype = static_cast<MessageType>(read_raw<int>(ss));
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
    DEBUG_LOG(log, raw->toAddress, "Append Entries received from %s",
              raw->fromAddress.toString().c_str());

    AppendEntriesMessage    message;
    message.load(ss);

    AppendEntriesReply  reply;

    if (message.term < savedState.currentTerm) {
        reply.success = false;
        reply.term = savedState.currentTerm;
    }
    else if (!logContains(message.prevLogIndex, message.prevLogTerm)) {
        reply.success = false;
        //$ TODO: Do I need to reply with the current term?
    }
}

void RaftMessageHandler::onRequestVote(const Address& from, istringstream& ss)
{
    DEBUG_LOG(log, raw->toAddress, "Request Vote received from %s",
                raw->fromAddress.toString().c_str());

    RequestVoteMessage    message;
    message.load(ss);

    RequestVoteReply    reply;
}

void RaftMessageHandler::OnAddServer(const Address& from, istringstream& ss)
{
    AddServerMessage    request;
    AddServerReply      reply;

    request.load(ss);
    if (currentState != State::LEADER) {
        reply.status = false;
        reply.leaderHint = savedState.leaderAddress;
    }
}

void RaftMessageHandler::OnAddServerReply(const Address& from, istringstream& ss)
{
    // This should only occur if we are trying to join
    // a group and have not been initialized yet.
    assert(currentState == State::NONE);

    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    AddServerReply  reply;
    reply.load(ss);

    if (reply.status) {
        // Success! we are part of the cluster
        savedState.leaderAddress = from;
        node->member.inGroup = true;
        stateChange(State::FOLLOWER);

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

void RaftMessageHandler::stateChange(State newState)
{
    if (this->currentState == newState)
        return;

    switch(newState) {
        case FOLLOWER:
            nextHeartbeat = 0;      // disable heartbeat
            break;
        case CANDIDATE:
            nextHeartbeat = 0;      // disable heartbeat
            break;
        case LEADER:
            // Send heartbeat to all followers
            //$ TODO: implement

            // steup for future heartbeats
            nextHeartbeat = par->getCurrtime() + timeout;
            break;
    }

    currentState = newState;
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

    if (currentState == State::JOINING && joinTimeout > par->getCurrtime()) {
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
    if (currentState == State::LEADER && nextHeartbeat > par->getCurrtime()) {
        //$ TODO: send heartbeat to all followers
        nextHeartbeat = par->getCurrtime() + timeout;
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

    if (connection->address() == joinAddress) {
        // we are the first process to join the group
        // Make ourselves the leader, initialize the log

        DEBUG_LOG(log, connection->address(), "Starting up group...");
        node->member.inGroup = true;
        node->member.addToMemberList(connection->address(),
                                     par->getCurrtime(),
                                     node->member.heartbeat);

        //$ TODO: initialie the system
        //$ TODO: intiialize the log entries for the system

        currentState = State::LEADER;
    }
    else {
        AddServerMessage    request;

        request.newServer = connection->address();
        auto raw = request.toRawMessage(connection->address(), joinAddress);

        DEBUG_LOG(log, connection->address(), "Trying to join...");

        stateChange(State::JOINING);
        joinTimeout = par->getCurrtime() + JOIN_TIMEOUT;

        connection->send(raw.get());
    }
}
