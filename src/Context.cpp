/*****
 * Context.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Context.h"
#include "NetworkNode.h"
#include "Raft.h"

using namespace Raft;
using std::placeholders::_1;
using std::placeholders::_2;


void RaftLogEntry::write(stringstream& ss)
{
    //$ TODO: Do I need to truncate the stream first?
    write_raw<TERM>(ss, this->termReceived);
    write_raw<int>(ss, static_cast<int>(this->command));
    write_raw<Address>(ss, this->address);
}

void RaftLogEntry::read(istringstream& is)
{
    this->termReceived = read_raw<TERM>(is);
    this->command = static_cast<Command>(read_raw<int>(is));
    this->address = read_raw<Address>(is);
}

FileBasedStorage::FileBasedStorage(const char *filename)
    : filename(filename)
{
    fs.open(filename, std::fstream::in |
                      std::fstream::out |
                      std::fstream::binary);
}

FileBasedStorage::~FileBasedStorage()
{
    fs.flush();
    fs.close();
}

Json::Value FileBasedStorage::read()
{
    Json::Value root;
    fs.seekg(0, fs.beg);
    fs >> root;
    return root;
}

void FileBasedStorage::write(const Json::Value& value)
{
    fs.seekp(0);    // seek to the beginning
    fs << value;
    fs.flush();
}

bool FileBasedStorage::empty()
{
    fs.seekg(0, fs.end);
    auto length = fs.tellg();
    return length == 0;
}

void FileBasedStorage::reset()
{
    fs.close();
    fs.open(filename, std::fstream::in |
                      std::fstream::out |
                      std::fstream::binary |
                      std::fstream::trunc);
}

MemoryBasedStorage::MemoryBasedStorage(Params *par)
    : par(par)
{}

Json::Value MemoryBasedStorage::read()
{
    return current;
}

void MemoryBasedStorage::write(const Json::Value& value)
{
    // This will do a deep-copy
    current = value;

    entries.emplace_back(par->getCurrtime(), time(NULL));
}

bool MemoryBasedStorage::empty()
{
    return !current;
}

void MemoryBasedStorage::reset()
{
    entries.clear();
    current.clear();
}

void Context::init(RaftHandler *handler,
                   StorageInterface *store,
                   StorageInterface *snapshotStore)
{
    this->handler = handler;
    this->store = store;
    this->snapshotStore = snapshotStore;

    // Start up as a follower node
    this->currentState = State::FOLLOWER;
    this->commitIndex = 0;
    this->lastAppliedIndex = 0;

    this->currentLeader.clear();
    this->currentTerm = 0;
    this->votedFor.clear();
    this->followers.clear();

    this->logEntries.clear();
    this->logEntries.emplace_back(); // add dummy first entry

    this->currentSnapshot = nullptr;
    this->workingSnapshot = nullptr;
    this->prevIndex = 0;
    this->prevTerm = 0;

    // Reload the persisted state
    loadFromStore();
}

// Loads the context from the Storage, then
// updates the context variables as needed.
void Context::loadFromStore()
{
    if (!store)
        throw NetworkException("Raft::Context no store provided");

    // Use the default values
    //$ TODO: what to do on exceptions?
    if (store->empty())
        return;

    Json::Value root = store->read();

    // Read in currentTerm, votedForAddress
    // and log entries
    unsigned short port;
    this->currentTerm = root.get("currentTerm", 0).asInt();

    port = root.get("votedForPort", 0).asInt();
    this->votedFor.parse(
        root.get("votedFor", "0.0.0.0").asString().c_str(), port);

    loadSnapshotFromStore();

    this->logEntries.clear();   // reset the log

    Json::Value log = root["log"];
    for (unsigned int i=0; i<log.size(); i++) {
        RaftLogEntry    entry;
        entry.termReceived = log[i].get("t", 0).asInt();
        entry.command = static_cast<Command>(log[i].get("c", 0).asInt());
        entry.address.parse(
            log[i].get("a", "0.0.0.0").asString().c_str(),
            log[i].get("p", 0).asUInt());

        this->logEntries.push_back(entry);
    }

    // apply the log entries
    // Update the context
    handler->applyLogEntries(this->logEntries);
    this->lastAppliedIndex = static_cast<int>(this->logEntries.size() - 1);
}

// Persists the context to the Storage, then
// updates the context variables as needed.
void Context::saveToStore()
{
    if (!store)
        throw NetworkException("Raft::Context no store provided");

    Json::Value root;

    root["currentTerm"] = static_cast<Json::UInt>(this->currentTerm);
    root["votedFor"] = this->votedFor.toAddressString();
    root["votedForPort"] = this->votedFor.getPort();

    Json::Value log;
    for (auto & entry: this->logEntries) {
        Json::Value logEntry;
        logEntry["t"] = static_cast<Json::UInt>(entry.termReceived);
        logEntry["c"] = static_cast<int>(entry.command);
        logEntry["a"] = entry.address.toAddressString();
        logEntry["p"] = entry.address.getPort();
        log.append(logEntry);
    }
    root["log"] = log;

    store->write(root);
    saveSnapshotToStore(this->currentSnapshot);
}

void Context::saveSnapshotToStore(shared_ptr<Snapshot> snapshot)
{
    if (!this->snapshotStore)
        return;

    snapshotStore->reset();

    if (!snapshot)
        return;

    Json::Value snapshotRoot;

    snapshotRoot["index"] = static_cast<Json::UInt>(snapshot->prevIndex);
    snapshotRoot["term"] = static_cast<Json::UInt>(snapshot->prevTerm);

    Json::Value memberRoot;
    for (auto & addr : snapshot->prevMembers) {
        Json::Value entry;
        entry["a"] = addr.toAddressString();
        entry["p"] = addr.getPort();
        memberRoot.append(entry);
    }
    snapshotRoot["members"] = memberRoot;

    snapshotStore->write(snapshotRoot);
}

void Context::loadSnapshotFromStore()
{
    if (!this->snapshotStore)
        return;
    if (this->snapshotStore->empty())
        return;

    Json::Value snapshotRoot = snapshotStore->read();
    auto snapshot = make_shared<Snapshot>();

    snapshot->prevIndex = snapshotRoot.get("index",0).asUInt();
    snapshot->prevTerm = snapshotRoot.get("term", 0).asUInt();

    for (unsigned int i=0; i<snapshotRoot["members"].size(); i++) {
        Json::Value elem = snapshotRoot["members"][i];
        Address addr;
        addr.parse(
            elem.get("a", "0.0.0.0").asString().c_str(),
            elem.get("p", 0).asUInt());
        snapshot->prevMembers.push_back(addr);
    }

    this->prevIndex = snapshot->prevIndex;
    this->prevTerm = snapshot->prevTerm;
}

void Context::changeMembership(Command cmd, const Address& address)
{
    assert(this->lastAppliedIndex == this->getLastLogIndex());

    auto node = this->handler->node();

    // Update the context entries (note that the entry has not
    // been persisted yet).
    RaftLogEntry    entry;
    entry.termReceived = this->currentTerm;
    entry.command = cmd;
    entry.address = address;

    this->logEntries.push_back(entry);
    this->handler->applyLogEntry(entry);
    this->lastAppliedIndex++;
    assert(this->lastAppliedIndex == this->getLastLogIndex());

    setLogChanged(true);
}

void Context::onTimeout()
{
    auto node = this->handler->node();

    // Special case, if we are the only node left
    // mark all entries as committed.  Allows nodes
    // to start up.
    if (node->member.memberList.size() == 1)
        node->context.commitIndex = node->context.getLastLogIndex();

    applyCommittedEntries();
}

void Context::startElection(const MemberInfo& member)
{
    DEBUG_LOG(this->handler->address(),
        "Starting election : term %d", this->currentTerm+1);

    this->currentTerm++;

    auto election = make_shared<ElectionTransaction>(this->log,
                                                     this->par,
                                                     this->handler);
    election->transId = this->handler->getNextMessageId();
    election->onCompleted = std::bind(&RaftHandler::onCompletedElection,
                                     this->handler, _1, _2);
    election->term = this->currentTerm;
    election->init(member);

    this->handler->addTransaction(election);

    election->start();
    this->votedFor = this->handler->address();

    election->startTimeout(this->handler->getElectionTimeout());
}

void Context::addEntries(INDEX startIndex, vector<RaftLogEntry> & entries)
{
    // At most we are appending all new entries
    if (startIndex > (this->getLastLogIndex() + 1)) {
        throw AppException("Context log index out-of-range");
    }

    setLogChanged(true);

    bool    rebuild = false;
    INDEX   index = 0;

    // Is there an overlap?
    if (startIndex <= getLastLogIndex()) {
        // If are possibly removing entries, need to rebuild
        // the state machine from scratch (or we could reverse
        // the entries but need to check that we don't delete
        // entries that weren't added, etc....).
        //
        // This is only true for log entries that deal with
        // cluster membership.

        // Look for entries that are in conflict
        for (index=0; index<entries.size(); index++) {
            // Stop if we go past the log size
            if (startIndex+index > getLastLogIndex())
                break;

            if (entries[index].termReceived != termAt(startIndex+index)) {
                // This location is different!
                // Delete this and all succeeding entries from the log

                // Do a sanity check to see that the log is not doing
                // weird things (like removing servers that haven't been
                // added or adding servers twice).
                if (DEBUG_) {
                    SanityTestLog     test;
                    test.init(this->currentSnapshot.get());
                    test.validateLogEntries(this->logEntries, 0, startIndex-prevIndex+index);
                    test.validateLogEntries(entries, index, entries.size()-index);
                }
                this->logEntries.resize(startIndex-prevIndex+index);

                // force rebuilding of the memberlist`
                rebuild = true;
                break;
            }
        }
    }

    if (rebuild) {
        this->handler->applyLogEntry(CMD_CLEAR_LIST, Address());
        // Reapply the snapshot
        if (this->currentSnapshot) {
            for (auto & addr : this->currentSnapshot->prevMembers) {
                this->handler->applyLogEntry(CMD_ADD_SERVER, addr);
            }
        }
        this->handler->applyLogEntries(this->logEntries);
        this->lastAppliedIndex = getLastLogIndex();
    }

    // Append on all other entries
    for (; index < entries.size(); index++) {
        this->logEntries.push_back(entries[index]);

        // we are using Raft for group membership
        // apply this entry to the state machine witout
        // waiting for commit
        this->handler->applyLogEntry(entries[index]);
        this->lastAppliedIndex++;
    }

    if (DEBUG_) {
        SanityTestLog test;
        test.init(this->currentSnapshot.get());
        test.validateLogEntries(this->logEntries, 0, this->logEntries.size());
    }

}

void Context::applyCommittedEntries()
{
    if (this->commitIndex > this->lastAppliedIndex) {
        for (INDEX i=this->lastAppliedIndex; i<=this->commitIndex; i++) {
            // Apply log entries to match
            this->handler->applyLogEntry(entryAt(i));
        }
        this->lastAppliedIndex = this->commitIndex;
        setLogChanged(true);
    }
}

void Context::switchToLeader()
{
    this->currentLeader = this->handler->address();
    this->currentState = State::LEADER;
}

void Context::switchToCandidate()
{
    this->currentState = State::CANDIDATE;
}

void Context::switchToFollower()
{
    this->currentState = State::FOLLOWER;
}

void Context::checkCommitIndex(INDEX sentLogIndex)
{
    if (this->commitIndex >= sentLogIndex)
        return;

    // Check to see if we have a possible new commit index
    // Start at 1 (don't forget ourselves!)
    unsigned int total = 1;
    for (const auto & elem : this->followers) { 
        if (elem.second.matchIndex >= sentLogIndex)
            total++;
    }
    if (2*total > this->followers.size()+1) {
        this->commitIndex = sentLogIndex;
    }
}

vector<Address> Context::runLogEntries(INDEX toIndex)
{
    if (toIndex < this->prevIndex)
        throw AppException("Cannot build the config, index before snapshot");

    vector<Address> addresses;

    if (this->currentSnapshot)
        addresses = this->currentSnapshot->prevMembers;

    for (INDEX i=this->prevIndex+1; i<=toIndex; i++) {
        auto entry = entryAt(i);
        switch (entry.command) {
            case Command::CMD_ADD_SERVER:
                addresses.push_back(entry.address);
                break;
            case Command::CMD_REMOVE_SERVER:
                addresses.erase(
                    std::remove(addresses.begin(), addresses.end(), entry.address),
                    addresses.end());
                break;
            default:
                break;
        }
    }

    return addresses;
}

void Context::takeSnapshot()
{
    auto node = this->handler->node();
    auto snapshot = make_shared<Snapshot>();

    snapshot->prevIndex = this->commitIndex;
    snapshot->prevTerm = termAt(snapshot->prevIndex);
    snapshot->prevMembers = runLogEntries(snapshot->prevIndex);

    //$ TODO: What to do with a failure to save?
    saveSnapshotToStore(snapshot);

    this->currentSnapshot = snapshot;

    // Truncate the log
    this->logEntries.erase(this->logEntries.begin(),
                           this->logEntries.begin() + realIndex(this->commitIndex) + 1);
    this->currentSnapshot = snapshot;
    this->prevIndex = snapshot->prevIndex;
    this->prevTerm = snapshot->prevTerm;
}

void SanityTestLog::init(Snapshot *snapshot)
{
    entries.clear();
    servers.clear();
    lastTermSeen = 0;

    if (snapshot) {
        for (auto & addr : snapshot->prevMembers)
            this->servers.insert(addr);
    }
}

void SanityTestLog::validateLogEntries(const vector<RaftLogEntry>& entries,
                                       INDEX start, INDEX count)
{
    for (INDEX i=0; i<count; i++) {
        auto & entry = entries[start+i];

        // terms are non-decreasing
        if (entry.termReceived < lastTermSeen)
            throw AppException("term not in increasing order");
        lastTermSeen = entry.termReceived;

        switch(entry.command) {
            case CMD_NOOP:
                break;
            case CMD_ADD_SERVER:
                {
                    // Check to see that the server is not already
                    // in the list of members
                    if (servers.count(entry.address) != 0)
                        throw AppException("address already in member list");
                    servers.insert(entry.address);
                }
                break;
            case CMD_REMOVE_SERVER:
                {
                    // Check to see that the server IS in the list
                    // of servers
                    if (servers.count(entry.address) == 0)
                        throw AppException("address not found in member list");
                    servers.erase(entry.address);
                }
                break;
            default:
                throw AppException("unknown command");
                break;
        }
    }
}

