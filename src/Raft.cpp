
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
    // Check that we've set the transId and term
    assert(this->transId);
    assert(this->term);
    write_raw<int>(ss, static_cast<int>(this->msgtype));
    write_raw<unsigned int>(ss, this->transId);
    write_raw<TERM>(ss, this->term);    
}

void Raft::Message::read(istringstream& is)
{
    this->msgtype = static_cast<MessageType>(read_raw<int>(is));
    this->transId = read_raw<unsigned int>(is);
    this->term = read_raw<TERM>(is);
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
    write_raw<INDEX>(ss, this->prevLogIndex);
    write_raw<TERM>(ss, this->prevLogTerm);
    write_raw<INDEX>(ss, this->leaderCommit);
    write_raw<unsigned int>(ss, this->entries.size());
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
    this->prevLogIndex = read_raw<INDEX>(is);
    this->prevLogTerm = read_raw<TERM>(is);
    this->leaderCommit = read_raw<INDEX>(is);

    size_t size = read_raw<unsigned int>(is);
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
    write_raw<INDEX>(ss, this->lastLogIndex);
    write_raw<TERM>(ss, this->lastLogTerm);

    return rawMessageFromStream(from, to, ss);
}

void RequestVoteMessage::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

    this->read(is);
    assert(this->msgtype == MessageType::REQUEST_VOTE);

    this->candidate = read_raw<Address>(is);
    this->lastLogIndex = read_raw<INDEX>(is);
    this->lastLogTerm = read_raw<TERM>(is);
}

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

unique_ptr<RawMessage> InstallSnapshotMessage::toRawMessage(const Address& from,
                                                            const Address& to)
{
    stringstream    ss;

    this->write(ss);
    write_raw<Address>(ss, this->leaderAddress);
    write_raw<INDEX>(ss, this->lastIndex);
    write_raw<TERM>(ss, this->lastTerm);
    write_raw<unsigned int>(ss, this->addresses.size());
    for (auto & addr : this->addresses) {
        write_raw<Address>(ss, addr);
    }
    write_raw<bool>(ss, this->done);

    return rawMessageFromStream(from, to, ss);
}
void InstallSnapshotMessage::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

    this->read(is);
    assert(this->msgtype == MessageType::INSTALL_SNAPSHOT);

    this->leaderAddress = read_raw<Address>(is);
    this->lastIndex = read_raw<INDEX>(is);
    this->lastTerm = read_raw<TERM>(is);

    size_t count = read_raw<unsigned int>(is);
    this->addresses.clear();
    for (size_t i=0; i<count; i++) {
        Address addr = read_raw<Address>(is);
        this->addresses.push_back(addr);
    }
    this->done = read_raw<bool>(is);
}

unique_ptr<RawMessage> InstallSnapshotReply::toRawMessage(const Address& from,
                                                          const Address& to)
{
    stringstream ss;

    this->write(ss);

    return rawMessageFromStream(from, to, ss);
}

void InstallSnapshotReply::load(const RawMessage *raw)
{
    istringstream is(std::string((const char *)raw->data.get(), raw->size));

    this->read(is);
    assert(this->msgtype == MessageType::INSTALL_SNAPSHOT_REPLY);
}

void Transaction::close()
{
    this->onCompleted = nullptr;
    this->parent = nullptr;
}

shared_ptr<NetworkNode> Transaction::getNetworkNode()
{
    return this->handler->node();
}

void ElectionTimeoutTransaction::start()
{
}

Transaction::RESULT ElectionTimeoutTransaction::onReply(const RawMessage *raw)
{
    // Should not get here since we do not send any messages directly
    throw AppException("should not be here");
    return Transaction::RESULT::KEEP;
}

Transaction::RESULT ElectionTimeoutTransaction::onTimeout()
{
    auto node = getNetworkNode();

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
            node->context.switchToCandidate();
            node->context.startElection(node->member);
        }
    }

    return Transaction::RESULT::KEEP;
}

// Sends an AppendEntries message to all nodes.  Replies
// don't really matter here.  Updates are performed through
// a different mechanism.
//$ TODO: could this be used for failure detection?
void HeartbeatTimeoutTransaction::sendGroupHeartbeat()
{
    auto node = getNetworkNode();

    AppendEntriesMessage message;
    message.transId = this->handler->getNextMessageId();
    message.term = node->context.currentTerm;
    message.leaderAddress = this->handler->address();
    message.prevLogIndex = node->context.getLastLogIndex();
    message.prevLogTerm = node->context.getLastLogTerm();
    message.leaderCommit = node->context.commitIndex;

    this->handler->broadcast(&message);
}

void HeartbeatTimeoutTransaction::start()
{
    sendGroupHeartbeat();
}

Transaction::RESULT HeartbeatTimeoutTransaction::onReply(const RawMessage *raw)
{
    // Should not get here since we do not send any messages directly
    throw AppException("should not be here");
    return Transaction::RESULT::KEEP;
}

Transaction::RESULT HeartbeatTimeoutTransaction::onTimeout()
{
    sendGroupHeartbeat();
    return Transaction::RESULT::KEEP;
}

void ElectionTransaction::init(const MemberInfo& member)
{
    // This assumes that we are part of the memberlist
    assert(member.isMember(handler->address()));

    yesVotes = 1;   // vote for ourself
    noVotes = 0;
    totalVotes = static_cast<int>(member.memberList.size());
    voted.insert(handler->address());
    for (auto & elem: member.memberList) {
        recipients.push_back(elem.address);
    }
}

void ElectionTransaction::start()
{
    auto node = getNetworkNode();

    RequestVoteMessage request;
    request.transId = this->transId;
    request.term = node->context.currentTerm;
    request.candidate = this->handler->address();
    request.lastLogIndex = node->context.getLastLogIndex();
    request.lastLogTerm = node->context.getLastLogTerm();

    this->handler->broadcast(&request, recipients);
}

Transaction::RESULT ElectionTransaction::onReply(const RawMessage *raw)
{
    auto node = getNetworkNode();

    RequestVoteReply reply;
    reply.load(raw);

    // If we are no longer a candidate, then quit this election
    if (node->context.currentState != State::CANDIDATE)
        return completed(false);

    if (this->voted.count(raw->fromAddress) == 0) {
        yesVotes += (reply.voteGranted ? 1 : 0);
        noVotes += (reply.voteGranted ? 0 : 1);
        this->voted.insert(raw->fromAddress);
    }

    if (isMajority())
        return completed(yesVotes > noVotes);

    return Transaction::RESULT::KEEP;
}

Transaction::RESULT ElectionTransaction::onTimeout()
{
    // Need to check the degenerate case (if we are the only
    // node alive).
    if (isMajority())
        return completed(yesVotes > noVotes);

    return completed(false);
}

void UpdateTransaction::init(const Address& address, INDEX logIndex, TERM logTerm)
{
    this->recipient = address;
    this->lastLogIndex = logIndex;
    this->lastLogTerm = logTerm;
}

void UpdateTransaction::start()
{
    sendAppendEntriesRequest(this->lastLogIndex);
}

Transaction::RESULT UpdateTransaction::onReply(const RawMessage *raw)
{
    auto node = getNetworkNode();

    // If we have become a follower, shut down the update
    // (it no longer makes sense since we are not the leader)
    if (node->context.currentState != State::LEADER)
        return completed(false);

    AppendEntriesReply reply;
    reply.load(raw);

    if (reply.success) {
        node->context.followers[raw->fromAddress].matchIndex = lastSentLogIndex;
        node->context.followers[raw->fromAddress].nextIndex = lastSentLogIndex+1;

        node->context.checkCommitIndex(lastSentLogIndex);

        // Are we all caught up?
        if (lastLogIndex == lastSentLogIndex)
            return completed(true);
        sendAppendEntriesRequest(this->lastSentLogIndex+1);
    }
    else {
        // Resend, moving down the log
        if (this->lastSentLogIndex == 0)
            return completed(false);
        node->context.followers[raw->fromAddress].nextIndex -= 1;
        sendAppendEntriesRequest(this->lastSentLogIndex-1);
    }
    return Transaction::RESULT::KEEP;
}

Transaction::RESULT UpdateTransaction::onTimeout()
{
    auto node = getNetworkNode();

    if (isLifetime())
        return completed(false);

    // If we are no longer the leader, quit the update
    if (node->context.currentState != State::LEADER)
        return completed(false);

    // If this is a normal timeout, resend the last request
    sendAppendEntriesRequest(this->lastSentLogIndex);
    return Transaction::RESULT::KEEP;
}

void UpdateTransaction::sendAppendEntriesRequest(INDEX index)
{
    auto node = getNetworkNode();

    // Pick the smaller of nextIndex and the target
    if (index > node->context.followers[recipient].nextIndex)
        index = node->context.followers[recipient].nextIndex;

    shared_ptr<Snapshot> snapshot;
    // If we are using a snapshot, use that, else use current context
    if (this->snapshot)
        snapshot = this->snapshot;
    else
        snapshot = node->context.currentSnapshot;

    if (snapshot && index < snapshot->prevIndex) {
        this->snapshot = snapshot;

        // We have a snapshot available
        InstallSnapshotMessage  install;
        install.transId = this->transId;
        install.term = node->context.currentTerm;
        install.leaderAddress = handler->address();
        install.lastIndex = snapshot->prevIndex;
        install.lastTerm = snapshot->prevTerm;

        std::copy_n(std::next(snapshot->prevMembers.begin(), this->offset),
                    par->maxSnapshotSize,
                    install.addresses.begin());

        install.offset = this->offset + install.addresses.size();
        install.done = (this->offset == snapshot->prevMembers.size());

        this->offset = install.offset;
        this->lastSentLogIndex = snapshot->prevIndex;
        this->lastSentLogTerm = snapshot->prevTerm;

        auto raw = install.toRawMessage(handler->address(), recipient);
        handler->connection()->send(raw.get());
    }
    else {
        AppendEntriesMessage append;
        append.transId = this->transId;
        append.term = node->context.currentTerm;
        append.leaderAddress = handler->address();
        append.prevLogIndex = index;
        append.prevLogTerm = node->context.logEntries[index].termReceived;
        append.leaderCommit = node->context.commitIndex;

        if (index < (node->context.logEntries.size()-1)) {
            //$ TODO: look into sending more than one entry at a time
            append.entries.push_back(node->context.logEntries[index+1]);
        }

        this->lastSentLogTerm = append.prevLogTerm;
        this->lastSentLogIndex = append.prevLogIndex;

        auto raw = append.toRawMessage(handler->address(), recipient);
        handler->connection()->send(raw.get());
    }
}

void GroupUpdateTransaction::init(const MemberInfo& member, INDEX lastIndex, TERM lastTerm)
{
    vector<Address> members;
    for (auto & elem : member.memberList)
        members.push_back(elem.address);
    this->init(members, lastIndex, lastTerm);
}

void GroupUpdateTransaction::init(const vector<Address>& members, INDEX lastIndex, TERM lastTerm)
{
    auto node = getNetworkNode();

    
    for (auto & address : members) {
        // Keep our own address in the recipients list. We use
        // the size for majority voting calculations later.
        if (address == this->handler->address() ||
            lastIndex >= node->context.followers[address].matchIndex) {
            recipients.push_back(address);
        }
    }
    this->lastLogIndex = lastIndex;
    this->lastLogTerm = lastTerm;
}

void GroupUpdateTransaction::start()
{
    auto node = getNetworkNode();
    
    totalVotes = static_cast<int>(recipients.size());
    successVotes = 1;       // for ourselves
    failureVotes = 0;

    // Start all of the individual updates
    for (auto & address : recipients) {
        if (address == this->handler->address())
            continue;
        auto update = make_shared<UpdateTransaction>(log, par, handler);
        update->term = this->term;
        update->transId = this->handler->getNextMessageId();
        update->onCompleted = std::bind(&GroupUpdateTransaction::onChildCompleted,
                                        this, _1, _2);
        update->parent = shared_from_this();
        update->init(address, this->lastLogIndex, this->lastLogTerm);

        this->handler->addTransaction(update);

        update->start();
        //$ TODO: what should this timeout be?
        update->startTimeout(this->handler->getElectionTimeout());;
    }

}

Transaction::RESULT GroupUpdateTransaction::onReply(const RawMessage *raw)
{
    // Should not get here, the GroupUpdate does not receive
    // any messages itself.
    throw AppException("should not be here");
    return Transaction::RESULT::KEEP;
}

Transaction::RESULT GroupUpdateTransaction::onTimeout()
{
    auto node = getNetworkNode();

    // If we are no longer the leader, quit the update
    if (node->context.currentState != State::LEADER)
        return completed(false);

    if (isLifetime())
        return completed(false);

    if (isMajority())
        return completed(successVotes > failureVotes);

    return Transaction::RESULT::KEEP;
}

void GroupUpdateTransaction::onChildCompleted(Transaction *trans, bool success)
{
    auto update = dynamic_cast<UpdateTransaction *>(trans);
    assert(update);

    if (this->replied.count(update->address()) != 0)
        return;
    this->replied.insert(update->address());

    successVotes += (success ? 1 : 0);
    failureVotes += (success ? 0 : 1);

    if (isMajority() && this->onCompleted) {
        // Notify of success!
        completed(successVotes > failureVotes);
    }

    this->handler->removeTransaction(trans->transId);
}

void MemberChangeTransaction::init(const MemberInfo& member)
{
    // Pull off the member addresses
    for (const auto & elem : member.memberList) {
        recipients.push_back(elem.address);
    }
}

void MemberChangeTransaction::start()
{
    // The first step is to catch the new server up 
    // to the previous config.
    // Start an individual UpdateTransaction to manage this.
    if (this->command == CMD_ADD_SERVER) {
        auto trans = make_shared<UpdateTransaction>(log, par, handler);

        trans->transId = this->handler->getNextMessageId();
        trans->term = this->term;

        trans->init(this->server, this->lastLogIndex, this->lastLogTerm);

        trans->onCompleted = std::bind(&MemberChangeTransaction::onServerUpdateCompleted,
                                       this, _1, _2);
        trans->parent = shared_from_this();

        this->currentTrans = trans;
        this->handler->addTransaction(trans);

        trans->start();
        //$ TODO: what should this timeout be?
        trans->startTimeout(5*par->idleTimeout);
    }
    else if (this->command == CMD_REMOVE_SERVER) {
        // skip this step, move to the next step
        // there's no need to update the node to be removed
        this->onServerUpdateCompleted(this, true);
    }
    else {
        throw AppException("Unexpected comamnd, should not be here");
    }
}

void MemberChangeTransaction::close()
{
    Transaction::close();
    auto current = this->currentTrans.lock();
    if (current)
        current->close();
    this->currentTrans.reset();
}

Transaction::RESULT MemberChangeTransaction::onReply(const RawMessage *raw)
{
    // Should not be here as we should not be getting a reply
    // with this transId.  We should not be sending out any
    // messages with this transId.
    throw AppException("should not be here");
    return Transaction::RESULT::DELETE;
}

Transaction::RESULT MemberChangeTransaction::onTimeout()
{
    // We've timed out!
    // Reply with a failure
    return completed(false);
}

void MemberChangeTransaction::onServerUpdateCompleted(Transaction *trans,
                                                      bool success)
{
    auto current = this->currentTrans.lock();
    if (current) {
        current->close();
        this->handler->removeTransaction(current->transId);
    }
    this->currentTrans.reset();

    auto node = getNetworkNode();

    if (success) {
        // move onto the next step

        // Add the current server to the config
        // (Note that we are still updating only up to the
        // previous configuration). 
        
        // Shortcut: If there are no recipients go straight to the 
        // next step in the operation.
        if ((recipients.size() == 0) ||
            (recipients.size() == 1 && recipients.front() == this->handler->address()))
            onPrevConfigCompleted(trans, true);
        else {
            // Update a majority of the servers to the configuration
            // without the current config change.
            auto update = make_shared<GroupUpdateTransaction>(log, par, handler);
            update->term = node->context.currentTerm;
            update->transId = this->handler->getNextMessageId();
            update->init(this->recipients, this->lastLogIndex, this->lastLogTerm);
    
            update->onCompleted = std::bind(&MemberChangeTransaction::onPrevConfigCompleted,
                                            this, _1, _2);
            update->parent = shared_from_this();
    
            this->currentTrans = update;
            this->handler->addTransaction(update);
        
            update->start();

            update->setLifetime(5*this->handler->getElectionTimeout());
        }
    }
    else {
        // Step one failed, end this transaction
        completed(false);
    }
}

void MemberChangeTransaction::onPrevConfigCompleted(Transaction *trans,
                                                    bool success)
{
    auto current = this->currentTrans.lock();
    if (current) {
        current->close();
        this->handler->removeTransaction(current->transId);
    }
    this->currentTrans.reset();

    auto node = getNetworkNode();

    if (success) {
        // next step, commit the current change (either add/remove)
        node->context.changeMembership(this->command, this->server);

        // Since we are the one's updating the log, no need to 
        // have the timeout mechanism do it.
        node->context.setLogChanged(false);

        // Commit using the new config
        if (this->command == Command::CMD_ADD_SERVER) {
            this->recipients.push_back(this->server);
        }
        else {
            this->recipients.erase(
                std::remove(recipients.begin(), recipients.end(), this->server),
                recipients.end());
        }
        
        // make the change
        auto update = make_shared<GroupUpdateTransaction>(log, par, handler);
        update->transId = this->handler->getNextMessageId();
        update->term = node->context.currentTerm;
        update->init(this->recipients,
                     node->context.getLastLogIndex(),
                     node->context.getLastLogTerm());

        update->onCompleted = std::bind(&MemberChangeTransaction::onCommandUpdateCompleted,
                                        this, _1, _2);
        update->parent = shared_from_this();

        this->currentTrans = update;
        this->handler->addTransaction(update);

        update->start();        

        update->setLifetime(5*this->handler->getElectionTimeout());
    }
    else {
        completed(false);
    }
}

void MemberChangeTransaction::onCommandUpdateCompleted(Transaction *trans,
                                                       bool success)
{
    auto current = this->currentTrans.lock();
    if (current) {
        this->handler->removeTransaction(current->transId);
    }
    this->currentTrans.reset();

    auto node = getNetworkNode();

    // We're done!  Success or failure
    // Notify the command client of the result

    if (this->command) {
        auto reply = this->commandMessage->makeReply(success);
        reply->address = node->context.currentLeader;

        auto raw = reply->toRawMessage(this->handler->address(),
                                       this->commandMessage->from);
        this->handler->connection()->send(raw.get());
    }

    completed(success);
}

void RaftHandler::start(const Address &leader)
{
    auto node = getNetworkNode();

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
        if (node->context.store->empty()) {
            // IF we are the designated leader, then initialize
            // the log with our own membership.
            node->context.changeMembership(Command::CMD_ADD_SERVER,
                                           connection()->address());

            // commit the membership list change
            node->context.saveToStore();

            // We're assuming that we're the only server at this point
            node->context.commitIndex = 1;
        }

        node->member.inGroup = true;

        // Special case!  Have the node start an election
        // immediately!
        // Just for this, start with an extra term
        node->context.currentTerm++;
        node->context.switchToCandidate();
        node->context.startElection(node->member);
    }

    // Start the election timeout
    this->election->startTimeout(this->getElectionTimeout());
}

// This is a callback and is called when the connection has received
// a message.
//
// The RawMessage will not be changed.
//
void RaftHandler::onMessageReceived(const RawMessage *raw)
{
    auto node = getNetworkNode();

    HeaderOnlyMessage   header;
    header.load(raw);

    if (header.term > node->context.currentTerm) {
        DEBUG_LOG(connection_->address(),
            "new term(%d->%d)! converting to follower",
            node->context.currentTerm, header.term);
        node->context.currentTerm = header.term;
        node->context.switchToFollower();
        node->context.currentLeader = raw->fromAddress;

        // new term, clear the lastVotedFor
        node->context.votedFor.clear();

        // If there is one started, no need to keep it processing
        heartbeat->stopTimeout();
    }

    switch(header.msgtype) {
        case MessageType::APPEND_ENTRIES:
            onAppendEntries(raw->fromAddress, raw);
            break;
        case MessageType::REQUEST_VOTE:
            onRequestVote(raw->fromAddress, raw);
            break;
        case MessageType::INSTALL_SNAPSHOT:
            onInstallSnapshot(raw->fromAddress, raw);
            break;
        case MessageType::APPEND_ENTRIES_REPLY:
        case MessageType::REQUEST_VOTE_REPLY:
        case MessageType::INSTALL_SNAPSHOT_REPLY:
            onReply(raw->fromAddress, header, raw);
            break;
        default:
            throw NetworkException(string_format("Unknown message type: %d", header.msgtype).c_str());
            break;
    }
}

void RaftHandler::onAppendEntries(const Address& from, const RawMessage *raw)
{
    DEBUG_LOG(connection_->address(), "Append Entries received from %s",
              from.toString().c_str());

    auto node = getNetworkNode();

    shared_ptr<AppendEntriesMessage> message = make_shared<AppendEntriesMessage>();
    message->load(raw);

    AppendEntriesReply  reply;
    reply.transId = message->transId;
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
                         (message->prevLogIndex + message->entries.size()));
            node->context.applyCommittedEntries();
        }

        // This node is up-to-date with the leader
        reply.success = true;
    }

    // if from current leader, reset timeout
    if (from == node->context.currentLeader)
        election->resetTimeout(par->getCurrtime());
    auto rawReply = reply.toRawMessage(address(), from);
    this->connection()->send(rawReply.get());
}

void RaftHandler::onRequestVote(const Address& from, const RawMessage *raw)
{
    auto node = getNetworkNode();

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
    election->resetTimeout(par->getCurrtime());
}

void RaftHandler::onInstallSnapshot(const Address& from,
                                    const RawMessage *raw)
{
    throw NYIException("onInstallSnapshot", __FILE__, __LINE__);
}

void RaftHandler::onReply(const Address&from,
                          const HeaderOnlyMessage& header,
                          const RawMessage *raw)
{
    auto trans = findTransaction(header.transId);
    if (!trans)
        return;     // nothing to do here, cannot find transaction

    auto node = getNetworkNode();
    node->context.saveToStore();

    auto result = trans->onReply(raw);
    if (result == Transaction::RESULT::DELETE) {
        trans->close();
        transactions.erase(header.transId);
    }
}

// This implements the AddServer and RemoveServer command messages.
//
// We do not forward the request, just return a failed notification,
// with the leader address in the address portion.
//
void RaftHandler::onChangeServerCommand(shared_ptr<CommandMessage> message,
                                        Raft::Command command,
                                        const Address& address)
{
    auto node = getNetworkNode();

    shared_ptr<CommandMessage>  reply;

    if (node->context.currentState != State::LEADER) {
        reply = message->makeReply(false);
        reply->address = node->context.currentLeader;   // send back the leader
        reply->errmsg = "not the leader";
    }
    else if (this->memberchange) {
        // Only one member change operation allowed at a time
        reply = message->makeReply(false);
        reply->address = node->context.currentLeader;
        reply->errmsg = "Add/RemoveServer in progress, this operation is not allowed";
    }
    else {
        int transIdUpdate = this->getNextMessageId();
        auto trans = make_shared<MemberChangeTransaction>(log, par, this);
        trans->transId = transIdUpdate;
        trans->term = node->context.currentTerm;
        trans->init(node->member);

        trans->onCompleted = std::bind(&RaftHandler::onCompletedMemberChange,
                                       this, _1, _2);

        trans->lastLogIndex = node->context.getLastLogIndex();
        trans->lastLogTerm = node->context.getLastLogTerm();
        trans->server = address;
        trans->command = command;
        trans->commandMessage = message;

        this->memberchange = trans;
        this->addTransaction(trans);

        // Get this started!
        trans->start();
        //$ TODO: what should the overall timeout be here?
        trans->startTimeout(5*this->getElectionTimeout());
    }

    if (reply) {
        auto raw = reply->toRawMessage(connection_->address(), message->from);
        connection_->send(raw.get());
    }
}

// This is called when there are no messages available (usually on a
// connection timeout).  Thus perform any actions that should be done
// on idle here.
//
void RaftHandler::onTimeout()
{
    // run the node maintenance loop
    auto node = getNetworkNode();

    // Process transaction timeouts
    vector<int> idsToRemove;
    int currtime = par->getCurrtime();
    for (auto & elem : transactions) {
        if (elem.second->isTimedOut(currtime)) {
            elem.second->advanceTimeout();
            if (elem.second->onTimeout() == Transaction::RESULT::DELETE) {
                // remove this transaction
                elem.second->close();
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

    if (node->context.currentState == State::LEADER) {
        // Check to see if we have to send out log updates
        if (node->context.logChanged() || currtime > (this->lastUpdate + par->idleTimeout)) {
            // Start up a group update
            auto update = make_shared<GroupUpdateTransaction>(
                            this->log, this->par, this);
            update->transId = this->getNextMessageId();
            update->term = node->context.currentTerm;
            update->init(node->member,
                         node->context.getLastLogIndex(),
                         node->context.getLastLogTerm());
            this->addTransaction(update);

            update->start();

            update->setLifetime(5*this->getElectionTimeout());
            update->startTimeout(par->idleTimeout);

            node->context.setLogChanged(false);

            lastUpdate = par->getCurrtime();
        }
    }
}

// The actual Raft state machine (for our application) is
// actually the membership list.  This is kept separate from
// the leader-kept follower list.
//
void RaftHandler::applyLogEntry(const RaftLogEntry& entry)
{
    applyLogEntry(entry.command, entry.address);
}

void RaftHandler::applyLogEntry(Command command,
                                const Address& address)
{
    auto node = getNetworkNode();

    switch(command) {
        case CMD_NONE:
        case CMD_NOOP:
            // Dummy command, just ignore
            break;
        case CMD_ADD_SERVER:
            node->member.addToMemberList(address, par->getCurrtime(), 0);
            if (address != this->address())
                node->context.followers[address] = 
                    Context::LeaderStateEntry(node->context.getLastLogIndex()+1, 0);
            break;
        case CMD_REMOVE_SERVER:
            node->member.removeFromMemberList(address);
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

void RaftHandler::applyLogEntries(const vector<RaftLogEntry> &entries)
{
    for (const auto & elem : entries) {
        this->applyLogEntry(elem);
    }
}

// Compare according to 3.6.1
// Compare the last terms, larger term is more current
// If terms are equal, compare the length
bool RaftHandler::isLogCurrent(INDEX index, TERM term)
{
    auto node = getNetworkNode();

    if (term == node->context.getLastLogTerm())
        return index >= node->context.getLastLogIndex();
    else
        return term > node->context.getLastLogTerm();
}

void RaftHandler::broadcast(Message *message)
{
    assert(message);

    auto node = getNetworkNode();

    for (const auto & elem: node->member.memberList) {
        if (elem.address == address())
            continue;

        auto raw = message->toRawMessage(address(), elem.address);
        connection()->send(raw.get());
    }
}

void RaftHandler::broadcast(Message *message, vector<Address> &recips)
{
    for (const auto & elem: recips) {
        if (elem == address())
            continue;

        auto raw = message->toRawMessage(address(), elem);
        connection()->send(raw.get());
    }
}

shared_ptr<Transaction> RaftHandler::findTransaction(int transid)
{
    auto it = transactions.find(transid);
    if (it == transactions.end())
        return nullptr;
    return it->second;
}

void RaftHandler::addTransaction(shared_ptr<Transaction> trans)
{
    // Note, only one transaction with a given transid is allowed
    // at one time
    if (transactions.count(trans->transId) > 0)
        throw AppException("duplicate transid");
    transactions[trans->transId] = trans;
}

void RaftHandler::removeTransaction(int transid)
{
    transactions.erase(transid);
}

void RaftHandler::initTimeoutTransactions()
{
    auto node = getNetworkNode();

    // Create the transaction for the election timeout
    auto trans = make_shared<ElectionTimeoutTransaction>(this->log, this->par, this);
    trans->transId = Transaction::SPECIALINDEX::ELECTION;
    transactions[trans->transId] = trans;
    this->election = trans;

    // Create the transaction for the heartbeat timeout
    auto heartbeat = make_shared<HeartbeatTimeoutTransaction>(this->log, this->par, this);
    heartbeat->transId = Transaction::SPECIALINDEX::HEARTBEAT;
    transactions[heartbeat->transId] = heartbeat;
    this->heartbeat = heartbeat;
}

void RaftHandler::onCompletedElection(Transaction *trans, bool success)
{
    auto node = getNetworkNode();

    if (node->context.currentState != State::CANDIDATE)
        return;

    if (success) {
        node->context.switchToLeader();

        heartbeat->start();
        heartbeat->startTimeout(par->idleTimeout);
    }
    else {
        // try again
        node->context.startElection(node->member);
    }
}

void RaftHandler::onCompletedMemberChange(Transaction *trans, bool success)
{
    if (this->memberchange) {
        this->memberchange->close();
        this->removeTransaction(this->memberchange->transId);
    }
    this->memberchange = nullptr;
}
