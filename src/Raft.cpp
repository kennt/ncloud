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
using std::placeholders::_2;

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

void HeaderOnlyMessage::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));
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

void AppendEntriesMessage::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

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

void AppendEntriesReply::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

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

void RequestVoteMessage::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

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

void RequestVoteReply::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

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

        // We're assuming that we're the only server at this point
        node->context.commitIndex = 1;

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

    HeaderOnlyMessage   header;
    header.load(raw);

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
        heartbeat->stop();
    }

    switch(header.msgtype) {
        case MessageType::APPEND_ENTRIES:
            onAppendEntries(raw->fromAddress, raw);
            break;
        case MessageType::REQUEST_VOTE:
            onRequestVote(raw->fromAddress, raw);
            break;
        case MessageType::APPEND_ENTRIES_REPLY:
        case MessageType::REQUEST_VOTE_REPLY:
            onReply(raw->fromAddress, header, raw);
            break;
        default:
            throw NetworkException(string_format("Unknown message type: %d", header.msgtype).c_str());
            break;
    }
}

void RaftMessageHandler::onAppendEntries(const Address& from, const RawMessage *raw)
{
    DEBUG_LOG(connection_->address(), "Append Entries received from %s",
              from.toString().c_str());

    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    shared_ptr<AppendEntriesMessage> message = make_shared<AppendEntriesMessage>();
    message->load(raw);

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
    auto rawReply = reply.toRawMessage(address(), from);
    this->connection()->send(rawReply.get());
}

void RaftMessageHandler::onRequestVote(const Address& from, const RawMessage *raw)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");

    DEBUG_LOG(connection_->address(), "Request Vote received from %s",
                from.toString().c_str());

    RequestVoteMessage    request;
    request.load(raw);

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

    auto rawReply = reply.toRawMessage(connection()->address(), from);
    connection()->send(rawReply.get());

    // If we have voted, reset the timeout
    election->reset(par->getCurrtime());
}

void RaftMessageHandler::onReply(const Address&from,
                                 const HeaderOnlyMessage& header,
                                 const RawMessage *raw)
{
    auto trans = findTransaction(header.transId);
    if (!trans)
        return;     // nothing to do here, cannot find transaction

    if (trans->onReceived == nullptr)
        return;     // no callback, nothing to do

    auto result = trans->onReceived(trans.get(), raw);
    if (result == Transaction::RESULT::DELETE)
        transactions.erase(header.transId);
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

    //$ TODO: check to see that we are not in the middle
    // of some other membership change.  Only one change allowed
    // at a time.

    if (node->context.currentState != State::LEADER) {
        reply = command->makeReply(false);
        reply->address = node->context.currentLeader;   // send back the leader
        reply->errmsg = "not the leader";
    }
    else if (this->addremove) {
        reply = command->makeReply(false);
        reply->address = node->context.currentLeader;
        reply->errmsg = "Add/RemoveServer in progress, this operation is not allowed";
    }
    else {
        // Step 1 (done here): Catch the new server up, start an update
        // transaction for that server
        // Step 2 : wait until previous config is commited (after step 1 completes)
        // Step 3 : append new entry, commit (after step 2 completes)
        // Step 4 : reply ok (after step 3 completes)
        //
        int transIdUpdate = this->getNextMessageId();
        auto trans = make_shared<UpdateTransaction>(log, par, node);

        trans->transId = transIdUpdate;
        // Catch the server up to this index/term
        trans->lastLogIndex = node->context.logEntries.size()-1;
        trans->lastLogTerm = node->context.logEntries[trans->lastLogIndex].termReceived;

        trans->onReceived = std::bind(&RaftMessageHandler::transOnUpdateMessage, this, _1, _2);
        trans->onTimeout = std::bind(&RaftMessageHandler::transOnUpdateTimeout, this, _1);
        trans->onCompleted = std::bind(&RaftMessageHandler::transChangeServerOnCompleted, this, _1);

        // Update the new server
        trans->recipients.push_back(address);
        addremove = trans;
        transactions[trans->transId] = trans;

        // Send out the initial appendEntries
        AppendEntriesMessage append;
        append.transId = transIdUpdate;
        append.term = node->context.currentTerm;
        append.leaderAddress = connection()->address();
        append.prevLogIndex = trans->lastLogIndex;
        append.prevLogTerm = trans->lastLogTerm;
        append.leaderCommit = node->context.commitIndex;

        auto raw = append.toRawMessage(connection()->address(), address);
        connection()->send(raw.get());
        // Append new config entry to log, commmit via majority voting
        // - force heartbeat and update
        //node->context.addMember(address);
    }

    if (reply) {
        auto raw = reply->toRawMessage(connection_->address(), command->from);
        connection_->send(raw.get());
    }
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
        reply->errmsg = "not the leader";
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
            elem.second->advanceTimeout();
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
            node->context.followers.emplace(address,
                                            Context::LeaderStateEntry());
            break;
        case CMD_REMOVE_SERVER:
            node->member.removeFromMemberList(
                address);
            node->context.followers.erase(address);
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
    auto trans = make_shared<ElectionTransaction>(log, par, node);
    trans->transId = Transaction::INDEX::ELECTION;
    trans->onTimeout = std::bind(&RaftMessageHandler::transOnElectionTimeout, this, _1);
    transactions[trans->transId] = trans;
    this->election = trans;

    // Create the transaction for the heartbeat timeout
    auto update = make_shared<Transaction>(log, par, node);
    update->transId = Transaction::INDEX::HEARTBEAT;
    update->onTimeout = std::bind(&RaftMessageHandler::transOnHeartbeatTimeout, this, _1);
    transactions[update->transId] = update;
    this->heartbeat = update;
}

Transaction::RESULT RaftMessageHandler::transOnElectionTimeout(Transaction *trans)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    ElectionTransaction * elect = dynamic_cast<ElectionTransaction *>(trans);

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
            node->context.startElection(node->member, elect);
        }
    }
    else if (node->context.currentState == State::CANDIDATE) {
        // Check our vote totals
        if ((elect->total > 0) && (2*elect->successes > elect->total)) {
            // majority vote, we have become the leader
            node->context.currentLeader = connection()->address();
            node->context.currentState = State::LEADER;

            // Turn off the election processing
            elect->successes = elect->failures = elect->total = 0;

            // Send an initial heartbeat
            heartbeat->onTimeout(heartbeat.get());

            // we've become the leader, start the heartbeat
            heartbeat->start(par->getCurrtime(), par->idleTimeout);
        }
        else {
            // election timeout, start a new election
            node->context.startElection(node->member, elect);
        }
    }

    return Transaction::RESULT::KEEP;
}

Transaction::RESULT RaftMessageHandler::transOnHeartbeatTimeout(Transaction *trans)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("");

    // If leader, send heartbeats
    if (node->context.currentState == State::LEADER) {
        AppendEntriesMessage    message;
        message.transId = getNextMessageId();
        message.term = node->context.currentTerm;
        message.leaderAddress = connection()->address();
        message.prevLogIndex = static_cast<int>(node->context.logEntries.size()-1);
        message.prevLogTerm = node->context.logEntries[message.prevLogIndex].termReceived;
        message.leaderCommit = node->context.commitIndex;
            
        broadcast(&message);
    }
    return Transaction::RESULT::KEEP;
}

Transaction::RESULT RaftMessageHandler::transOnUpdateMessage(Transaction *trans, const RawMessage *raw)
{
    UpdateTransaction * update = dynamic_cast<UpdateTransaction *>(trans);
    if (!update)
        throw AppException("");

    AppendEntriesReply reply;
    reply.load(raw);

    // Is this in our recipient list? if not, skip
    if (std::count(update->recipients.begin(),
                   update->recipients.end(),
                   raw->fromAddress) == 0)
        return Transaction::RESULT::KEEP;

    if (reply.success) {
        // Update the followers data (nextIndex and matchIndex)
        // Send more data if necessary
        // Mark as complete if up-to-date
        // If complete, add to satisified list
    }
    else {
        // Failed, move further back in the log
        // Send another request
    }

    // Have we achieved a quorum?  Start the next operation
    // But keep this transaction around until all fully updated
    if (update->isCompleted()) {
        if (update->onCompleted) {
            update->onCompleted(trans);
            update->onCompleted = nullptr;
        }

        if (update->recipients.size() >= update->satisfied.size())
            return Transaction::RESULT::DELETE;
        else
            return Transaction::RESULT::KEEP;
    }
    return Transaction::RESULT::KEEP;
}

Transaction::RESULT RaftMessageHandler::transOnUpdateTimeout(Transaction *trans)
{
    // Look for updates that are not yet up-to-date
    // Resend as necessary
    // Keep on going until maximum time passes
    //
    // Check to see if a node is still in the membership list.  If not
    // remove from the satisified and recipients lists.
    // then check the completed condition.
    //
    throw NYIException("transOnUpdateTimeout", __FILE__, __LINE__);
    return Transaction::RESULT::KEEP;
}

void RaftMessageHandler::transChangeServerOnCompleted(Transaction *trans)
{
    // Start the next step in the process
    // Commit the previous configuration, update a quorum
    // of servers to the latest config
    throw NYIException("transOnCompleted", __FILE__, __LINE__);
}
