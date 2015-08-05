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
using std::placeholders::_1;

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


void RaftMessageHandler::start(const Address &leader)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    node->member.inGroup = false;
    node->member.inited = true;

    node->member.memberList.clear();

    DEBUG_LOG(connection()->address(), "starting up...");
    node->context.init(this, store);

    // Setup the various timeout callbacks
    //  - electionTimeout
    //  - heartbeatTimeout
    initTimeoutTransactions();

    // Special case (for initial cluster startup)
    if (connection()->address() == leader) {
        if (!node->context.store->empty()) {
            // The log is not empty. This codepath
            // is meant for initial cluster startup (not
            // for an already existing cluster).
            throw AppException("log already exists, unsupported scenario");
        }

        node->member.inGroup = true;

        // IF we are the designated leader, then initialize
        // the log with our own membership.
        node->context.addMember(connection()->address());

        // commit the membership list change
        node->context.saveToStore();

        // Special case!  Have the node start an election
        // immediately!
        node->context.currentState = State::CANDIDATE;
        node->context.startElection(node->member, election.get());
    }

    // Start the election timeout
    election->start(par->getCurrtime(), par->electionTimeout);
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

    if (header.term > node->context.currentTerm) {
        //$ CHECK: Does this state change happen before
        // or after RPC handling?  Does this cause the
        // message handling to continue? or do we drop the msg?
        DEBUG_LOG(connection_->address(),
            "new term(%d->%d)! converting to follower",
            node->context.currentTerm, header.term);
        node->context.currentTerm = header.term;
        node->context.currentState = State::FOLLOWER;
        node->context.currentLeader = raw->fromAddress;

        // new term, clear the lastVotedFor
        node->context.votedFor.clear();

        // If there is one started, no need to keep it processing
        election->cancelElection();
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
            onReply(raw->fromAddress, header, ss);
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

    reply.term = node->context.currentTerm;

    if (message->term < node->context.currentTerm) {
        reply.success = false;
    }
    else if ((message->prevLogIndex > (node->context.logEntries.size()-1)) ||
              message->prevLogTerm != node->context.logEntries[message->prevLogIndex].termReceived) {
        reply.success = false;
    }
    else {
        // Add the new entries to the log
        // prevLogIndex is the index BEFORE the new entries, thus
        // actual new entry is at prevLogIndex+1
        node->context.addEntries(message->prevLogIndex+1, message->entries);

        if (message->leaderCommit > node->context.commitIndex) {
            node->context.commitIndex = 
                std::min(message->leaderCommit,
                         (int)(message->prevLogIndex + message->entries.size()));
            node->context.applyCommittedEntries();
        }

        // This node is up-to-date with the leader
        reply.success = true;
    }

    // if from current leader, reset timeout
    if (from == node->context.currentLeader)
        election->reset(par->getCurrtime());
    auto raw = reply.toRawMessage(address(), from);
    this->connection()->send(raw.get());
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
    else if ((!node->context.votedFor ||
              node->context.votedFor == request.candidate) &&
             isLogCurrent(request.lastLogIndex, request.lastLogTerm)) {
        reply.voteGranted = true;
    }

    auto raw = reply.toRawMessage(connection()->address(), from);
    connection()->send(raw.get());

    // If we have voted, reset the timeout
    election->reset(par->getCurrtime());
}

void RaftMessageHandler::onReply(const Address&from, const HeaderOnlyMessage& header, istringstream& is)
{
    auto trans = findTransaction(header.transId);
    if (!trans)
        return;     // nothing to do here, cannot find transaction

    if (trans->onReceived == nullptr)
        return;     // no callback, nothing to do

    auto result = trans->onReceived(trans.get(), is);
    if (result == Transaction::RESULT::DELETE)
        transactions.erase(header.transId);
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
        reply->address = node->context.currentLeader;
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
        reply->address = node->context.currentLeader;
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

    // Process transaction timeouts
    vector<int> idsToRemove;
    int currtime = par->getCurrtime();
    for (auto & elem : transactions) {
        if (elem.second->isTimedOut(currtime)) {
            elem.second->nTimeouts++;
            if (elem.second->onTimeout == nullptr)
                continue;
            if (elem.second->onTimeout(elem.second.get()) == Transaction::RESULT::DELETE) {
                // remove this transaction
                idsToRemove.push_back(elem.first);
            }
        }
    }
    for (auto id : idsToRemove) {
        transactions.erase(id);
    }

    // Take care of any internal context actions
    // (usually not time-based but are triggered by
    // changes within the context).
    node->context.onTimeout();

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
        case CMD_NOOP:
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
        case CMD_CLEAR_LIST:
            node->member.memberList.clear();
            break;
        default:
            throw NetworkException("Unknown log entry command!");
            break;
    }
}

void RaftMessageHandler::applyLogEntries(const vector<RaftLogEntry> &entries)
{
    for (const auto & elem : entries) {
        this->applyLogEntry(elem);
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

shared_ptr<Transaction> RaftMessageHandler::findTransaction(int transid)
{
    auto it = transactions.find(transid);
    if (it == transactions.end())
        return nullptr;
    return it->second;
}

void RaftMessageHandler::initTimeoutTransactions()
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    // Create the transaction for the election timeout
    auto trans = make_shared<Transaction>(log, par, node);
    trans->transId = Transaction::INDEX::ELECTION;
    trans->onTimeout = std::bind(&RaftMessageHandler::onElectionTimeout, this, _1);
    transactions[trans->transId] = trans;
    this->election = trans;

    // Create the transaction for the heartbeat timeout
    trans = make_shared<Transaction>(log, par, node);
    trans->transId = Transaction::INDEX::HEARTBEAT;
    trans->onTimeout = std::bind(&RaftMessageHandler::onHeartbeatTimeout, this, _1);
    transactions[trans->transId] = trans;
    this->heartbeat = trans;
}

Transaction::RESULT RaftMessageHandler::onElectionTimeout(Transaction *trans)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    if (node->context.currentState == State::FOLLOWER) {
        // If it times out and we haven't voted for anyone this
        // term, then convert to candidate
        //
        // Since we reset the timeout when we receive an append_entries
        // or reply to a request_vote this only triggers when we haven't
        // seen either for a while
        //
        // Can't start an election if there are no members
        if (!node->context.votedFor &&
            !node->member.memberList.empty()) {
            // Election timeout!
            node->context.currentState = State::CANDIDATE;
            node->context.startElection(node->member, trans);
        }
    }
    else if (node->context.currentState == State::CANDIDATE) {
        // Check our vote totals
        if ((trans->total > 0) && (2*trans->successes > trans->total)) {
            // majority vote, we have become the leader
            node->context.currentLeader = connection()->address();
            node->context.currentState = State::LEADER;

            // Turn off the election processing
            trans->successes = trans->failures = trans->total = 0;

            // broadcast heartbeats
            AppendEntriesMessage    message;
            message.transId = getNextMessageId();
            message.term = node->context.currentTerm;
            message.leaderAddress = connection()->address();
            message.prevLogIndex = static_cast<int>(node->context.logEntries.size()-1);
            message.prevLogTerm = node->context.logEntries[message.prevLogIndex].termReceived;
            message.leaderCommit = node->context.commitIndex;
            
            broadcast(&message);
        }
        else {
            // election timeout, start a new election
            node->context.startElection(node->member, trans);
        }
    }

    return Transaction::RESULT::KEEP;
}

Transaction::RESULT RaftMessageHandler::onHeartbeatTimeout(Transaction *trans)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    // If leader, send heartbeats
    if (node->context.currentState == State::LEADER) {
        if (nextHeartbeat > par->getCurrtime()) {
            //$ TODO: send heartbeat to all followers
            throw NYIException("broadcast heartbeats", __FILE__, __LINE__);
        }
    }
    return Transaction::RESULT::KEEP;
}
