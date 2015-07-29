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

MessageType Raft::Message::getMessageType(const byte *data, size_t dataSize)
{
    istringstream ss(std::string((const char *)data, dataSize));
    return static_cast<MessageType>(read_raw<int>(ss));
}

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


void RaftMessageHandler::start(const Address &unused)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    node->member.inGroup = false;
    node->member.inited = true;

    node->member.memberList.clear();

    DEBUG_LOG(connection->address(), "starting up...");
    node->context.init(this, store);

    // start the timeouts up
    // If the timeouts are triggered then events happen
    node->context.electionTimeout = par->getCurrtime() + par->electionTimeout;
    node->context.heartbeatTimeout = par->getCurrtime() + par->heartbeatTimeout;
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

    // If we do not belong to a group, then do not handle any
    // client requests.
    if (!node->member.inGroup) {
        DEBUG_LOG(connection->address(), "message dropped, not in group yet");
        return;
    }

    switch(msgtype) {
        case MessageType::APPEND_ENTRIES:
            onAppendEntries(raw->fromAddress, ss);
            break;
        case MessageType::REQUEST_VOTE:
            onRequestVote(raw->fromAddress, ss);
            break;
        case MessageType::APPEND_ENTRIES_REPLY:
        case MessageType::REQUEST_VOTE_REPLY:
            onReply(raw->fromAddress, msgtype, ss);
            break;
        default:
            throw NetworkException(string_format("Unknown message type: %d", msgtype).c_str());
            break;
    }
}

void RaftMessageHandler::onAppendEntries(const Address& from, istringstream& ss)
{
    DEBUG_LOG(connection->address(), "Append Entries received from %s",
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
}

void RaftMessageHandler::onRequestVote(const Address& from, istringstream& ss)
{
    DEBUG_LOG(connection->address(), "Request Vote received from %s",
                from.toString().c_str());

    RequestVoteMessage    message;
    message.load(ss);

    RequestVoteReply    reply;
}

void RaftMessageHandler::onReply(const Address&from, MessageType mt, istringstream& ss)
{
    // Read in the message type and transid again
    // skip past the msgtype
    read_raw<int>(ss);
    int     transid = read_raw<int>(ss);
    ss.seekg(0, ss.beg);    // reset to start of the buffer

    if (transactions.count(transid) == 0) {
        // Cannot find the transaction, nothing to
        // do except to exit (it may have timeout)
        return;
    }

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

    closeTransaction(transid);
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
        //$ TODO: would it be better to throw an exception?
        DEBUG_LOG(connection->address(), "Timeout on joining the cluster");
        node->quit();
        return;
    }

    // Wait until you're in the group
    if (!node->member.inGroup)
        return;

    node->context.onTimeout();

    // If leader, send heartbeats
    if (node->context.currentState == State::LEADER && nextHeartbeat > par->getCurrtime()) {
        //$ TODO: send heartbeat to all followers
        nextHeartbeat = par->getCurrtime() + par->heartbeatTimeout;
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

