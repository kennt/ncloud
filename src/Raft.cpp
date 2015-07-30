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

using namespace Raft;

void Raft::Message::write(stringstream& ss)
{
    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<int>(ss, this->transId);
    write_raw<int>(ss, this->term);    
}

void Raft::Message::read(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<int>(is);
    this->term = read_raw<int>(is);
}

MessageType Raft::Message::getMessageType(const byte *data, size_t dataSize)
{
    istringstream ss(std::string((const char *)data, dataSize));
    return static_cast<MessageType>(read_raw<int>(ss));
}

unique_ptr<RawMessage> HeaderOnlyMessage::toRawMessage(const Address& from,
                                                       const Address& to)
{
    // Header-only messages should not be used to create a 
    // real message.
    throw AppException("Should not reach here");
}

void HeaderOnlyMessage::load(istringstream& is)
{
    this->read(is);
}

unique_ptr<RawMessage> AppendEntriesMessage::toRawMessage(const Address &from,
                                                          const Address& to)
{
    stringstream    ss;

    this->write(ss);

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
    this->read(is);
    assert(this->msgtype == MessageType::APPEND_ENTRIES);

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

    this->write(ss);

    write_raw<bool>(ss, this->success);

    return rawMessageFromStream(from, to, ss);
}

void AppendEntriesReply::load(istringstream& is)
{
    this->read(is);
    assert(this->msgtype == MessageType::APPEND_ENTRIES_REPLY);

    this->success = read_raw<bool>(is);
}

unique_ptr<RawMessage> RequestVoteMessage::toRawMessage(const Address &from,
                                                        const Address& to)
{
    stringstream    ss;

    this->write(ss);

    write_raw<Address>(ss, this->candidate);
    write_raw<int>(ss, this->lastLogIndex);
    write_raw<int>(ss, this->lastLogTerm);

    return rawMessageFromStream(from, to, ss);
}

void RequestVoteMessage::load(istringstream& is)
{
    this->read(is);
    assert(this->msgtype == MessageType::REQUEST_VOTE);

    this->candidate = read_raw<Address>(is);
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

    this->write(ss);

    write_raw<bool>(ss, this->voteGranted);

    return rawMessageFromStream(from, to, ss);
}

// Loads the streamed binary data from the istringstream into a
// RequestVoteReply.  It is assumed that the position of the stream is
// at the head of the message.
//
void RequestVoteReply::load(istringstream& is)
{
    this->read(is);
    assert(this->msgtype == MessageType::REQUEST_VOTE_REPLY);

    this->voteGranted = read_raw<bool>(is);
}


void RaftMessageHandler::start(const Address &unused)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    node->member.inGroup = false;
    node->member.inited = true;

    node->member.memberList.clear();

    DEBUG_LOG(connection_->address(), "starting up...");
    node->context.init(this, store);

    // start the election timeout
    node->context.resetTimeout();
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

    HeaderOnlyMessage   header;
    header.load(ss);
    ss.seekg(0, ss.beg);    // reset to start of the buffer

    // If we do not belong to a group, then do not handle any
    // client requests.
    if (!node->member.inGroup) {
        DEBUG_LOG(connection_->address(),
            "message dropped, not in group yet");
        return;
    }

    if (header.term > node->context.currentTerm) {
        //$ CHECK: Does this state change happen before
        // or after RPC handling?  Does this cause the
        // message handling to continue? or do we drop the msg?
        DEBUG_LOG(connection_->address(),
            "new term(%d->%d)! converting to follower",
            node->context.currentTerm, header.term);
        node->context.currentTerm = header.term;
        node->context.currentState = State::FOLLOWER;

        // new term, clear the lastVotedFor
        node->context.votedFor.clear();
    }

    switch(header.msgtype) {
        case MessageType::APPEND_ENTRIES:
            onAppendEntries(raw->fromAddress, ss);
            break;
        case MessageType::REQUEST_VOTE:
            onRequestVote(raw->fromAddress, ss);
            break;
        case MessageType::APPEND_ENTRIES_REPLY:
        case MessageType::REQUEST_VOTE_REPLY:
            onReply(raw->fromAddress, header.msgtype, ss);
            break;
        default:
            throw NetworkException(string_format("Unknown message type: %d", header.msgtype).c_str());
            break;
    }
}

void RaftMessageHandler::onAppendEntries(const Address& from, istringstream& ss)
{
    DEBUG_LOG(connection_->address(), "Append Entries received from %s",
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
    else if (node->context.logEntries[message->prevLogIndex].termReceived != message->prevLogTerm) {
        reply.success = false;
        //$ TODO: Do I need to reply with the current term?
        throw NYIException("Raft AppendEntries", __FILE__, __LINE__);
    }

    // if from current leader, reset timeout
    if (from == node->context.leaderAddress)
        node->context.resetTimeout();
}

void RaftMessageHandler::onRequestVote(const Address& from, istringstream& ss)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    DEBUG_LOG(connection_->address(), "Request Vote received from %s",
                from.toString().c_str());

    RequestVoteMessage    request;
    request.load(ss);

    RequestVoteReply    reply;
    reply.transId = request.transId;
    reply.term = node->context.currentTerm;
    reply.voteGranted = false;

    //$ TODO: refactor?  just following the psuedo-code
    if (request.term < node->context.currentTerm) {
        reply.voteGranted = false;
    }
    else if ((node->context.votedFor.isZero() ||
              node->context.votedFor == request.candidate) &&
             isLogCurrent(request.lastLogIndex, request.lastLogTerm)) {
        reply.voteGranted = true;
    }

    auto raw = reply.toRawMessage(connection()->address(), from);
    connection()->send(raw.get());

    // If we have voted, reset the timeout
    node->context.resetTimeout();
}

void RaftMessageHandler::onReply(const Address&from, MessageType mt, istringstream& ss)
{
    switch (mt) {
        case APPEND_ENTRIES_REPLY:
            onAppendEntriesReply(from, ss);
            break;
        case REQUEST_VOTE_REPLY:
            onRequestVoteReply(from, ss);
            break;
        default:
            throw NYIException("reply handler", __FILE__, __LINE__);
            break;
    }
}

void RaftMessageHandler::onAppendEntriesReply(const Address& from, istringstream& ss)
{
    throw NYIException("onAppendEntriesReply", __FILE__, __LINE__);
}

void RaftMessageHandler::onRequestVoteReply(const Address& from, istringstream& ss)
{
    throw NYIException("RaftMessageHandler::onRequestVoteReply", __FILE__, __LINE__);
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
        reply = command->makeReply(true);
        node->context.addMember(address);
    }

    // Send the reply
    auto raw = reply->toRawMessage(connection_->address(), command->from);
    connection_->send(raw.get());
}

void RaftMessageHandler::onRemoveServerCommand(shared_ptr<CommandMessage> command, const Address& address)
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
        reply = command->makeReply(true);
        node->context.removeMember(address);
    }

    // Send the reply
    auto raw = reply->toRawMessage(connection_->address(), command->from);
    connection_->send(raw.get());
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

    // Wait until you're in the group
    //if (!node->member.inGroup)
    //    return;

    node->context.onTimeout();

    // If leader, send heartbeats
    if (node->context.currentState == State::LEADER) {
        if (nextHeartbeat > par->getCurrtime()) {
            //$ TODO: send heartbeat to all followers
            node->context.resetTimeout();
        }
    }
    else if (node->context.currentState == State::FOLLOWER) {
        // If it times out and we haven't voted for anyone this
        // term, then convert to candidate
        if (node->context.electionTimeout >= par->getCurrtime() &&
            node->context.votedFor.isZero()) {
            // Election timeout!
            node->context.currentState = State::CANDIDATE;
            node->context.startElection(node->member);
        }
    }
    else if (node->context.currentState == State::CANDIDATE) {
        // Check our vote totals
        if ((2 * node->context.votesReceived) > node->context.votesTotal) {
            // majority vote, we have become the leader
            node->context.leaderAddress = connection()->address();
            node->context.currentState = State::LEADER;

            // broadcast heartbeats
            AppendEntriesMessage    message;
            message.transId = getNextMessageId();
            message.term = node->context.currentTerm;
            message.leaderAddress = connection()->address();
            message.prevLogIndex = node->context.logEntries.size()-1;
            message.prevLogTerm = node->context.logEntries[message.prevLogIndex].termReceived;
            message.leaderCommit = node->context.commitIndex;
            
            broadcast(&message);
        }
        else if (node->context.electionTimeout >= par->getCurrtime()) {
            // election timeout, start new election
            node->context.startElection(node->member);
        }
    }
}

void RaftMessageHandler::openTransaction(int transId,
                                         int timeout,
                                         const Address& to,
                                         shared_ptr<Message> message)
{
    Transaction     trans(transId, par->getCurrtime(), timeout,
                          to, message);
    transactions[transId] = trans;
}

void RaftMessageHandler::closeTransaction(int transId)
{
    auto it = transactions.find(transId);
    if (it != transactions.end()) {
        transactions.erase(it);
    }
}

// The actual Raft state machine (for our application) is
// actually the membership list.  This is kept separate from
// the leader-kept follower list.
//
void RaftMessageHandler::applyLogEntry(const RaftLogEntry& entry)
{
    applyLogEntry(entry.command, entry.address);
}

void RaftMessageHandler::applyLogEntry(Command command,
                                       const Address& address)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    switch(command) {
        case CMD_NONE:
            // Dummy command, just ignore
            break;
        case CMD_ADD_SERVER:
            node->member.addToMemberList(
                address,
                par->getCurrtime(),
                0);
            break;
        case CMD_REMOVE_SERVER:
            node->member.removeFromMemberList(
                address);
            break;
        default:
            throw NetworkException("Unknown log entry command!");
            break;
    }
}

bool RaftMessageHandler::isLogCurrent(int index, int term)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    if (index != (node->context.logEntries.size()-1))
        return false;
    return term == node->context.logEntries[index].termReceived;
}

void RaftMessageHandler::broadcast(Message *message)
{
    assert(message);

    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    for (const auto & elem: node->member.memberList) {
        if (elem.address == address())
            continue;

        auto raw = message->toRawMessage(address(), elem.address);
        connection()->send(raw.get());
    }
}
