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

FileBasedContextStore::FileBasedContextStore(const char *filename)
    : filename(filename)
{
    fs.open(filename, std::fstream::in |
                      std::fstream::out |
                      std::fstream::binary);
}

FileBasedContextStore::~FileBasedContextStore()
{
    fs.flush();
    fs.close();
}

Json::Value FileBasedContextStore::read()
{
    Json::Value root;
    fs.seekg(0, fs.beg);
    fs >> root;
    return root;
}

void FileBasedContextStore::write(const Json::Value& value)
{
    fs.seekp(0);    // seek to the beginning
    fs << value;
    fs.flush();
}

bool FileBasedContextStore::empty()
{
    fs.seekg(0, fs.end);
    auto length = fs.tellg();
    return length == 0;
}

void FileBasedContextStore::reset()
{
    fs.close();
    fs.open(filename, std::fstream::in |
                      std::fstream::out |
                      std::fstream::binary |
                      std::fstream::trunc);
}

MemoryBasedContextStore::MemoryBasedContextStore(Params *par)
    : par(par)
{}

Json::Value MemoryBasedContextStore::read()
{
    return current;
}

void MemoryBasedContextStore::write(const Json::Value& value)
{
    // This will do a deep-copy
    current = value;

    entries.emplace_back(par->getCurrtime(), time(NULL));
}

bool MemoryBasedContextStore::empty()
{
    return !current;
}

void MemoryBasedContextStore::reset()
{
    entries.clear();
    current.clear();
}

void Context::init(RaftHandler *handler,
                   ContextStoreInterface *store)
{
    this->handler = handler;
    this->store = store;

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

    // Reload the persisted state
    this->loadFromStore();
}

// Loads the context from the ContextStore, then
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

    this->logEntries.clear();   // reset the log

    Json::Value log = root["log"];
    for (int i=0; i<log.size(); i++) {
        RaftLogEntry    entry;
        entry.termReceived = log[i].get("term", 0).asInt();
        entry.command = static_cast<Command>(log[i].get("command", 0).asInt());
        entry.address.parse(
            log[i].get("address", "0.0.0.0").asString().c_str(),
            static_cast<unsigned short>(log[i].get("port", 0).asInt()));

        this->logEntries.push_back(entry);
    }

    // apply the log entries
    // Update the context
    handler->applyLogEntries(this->logEntries);
    this->lastAppliedIndex = static_cast<int>(this->logEntries.size() - 1);
}

// Persists the context to the ContextStore, then
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
        logEntry["term"] = static_cast<Json::UInt>(entry.termReceived);
        logEntry["command"] = static_cast<int>(entry.command);
        logEntry["address"] = entry.address.toAddressString();
        logEntry["port"] = entry.address.getPort();
        log.append(logEntry);
    }
    root["log"] = log;

    store->write(root);
}

void Context::changeMembership(Command cmd, const Address& address)
{
    assert(this->lastAppliedIndex == (this->logEntries.size()-1));

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
    assert(this->lastAppliedIndex == (this->logEntries.size() - 1));

    setLogChanged(true);
}

void Context::onTimeout()
{
    this->applyCommittedEntries();
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
    if (startIndex > this->logEntries.size()) {
        throw AppException("Context log index out-of-range");
    }

    this->setLogChanged(true);

    bool    rebuild = false;
    int     index = 0;

    if (startIndex < this->logEntries.size()) {
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
            if (startIndex+index > this->logEntries.size()-1)
                break;

            if (entries[index].termReceived != this->logEntries[startIndex+index].termReceived) {
                // This location is different!
                // Delete this and all succeeding entries from the log

                // Do a sanity check to see that the log is not doing
                // weird things (like removing servers that haven't been
                // added or adding servers twice).
                if (DEBUG_) {
                    SanityTestLog     test;
                    test.validateLogEntries(this->logEntries, 0, startIndex+index);
                    test.validateLogEntries(entries, index, static_cast<int>(entries.size()-index));
                }
                this->logEntries.resize(startIndex+index);

                // force rebuilding of the memberlist
                rebuild = true;
                break;
            }
        }
    }

    if (rebuild) {
        this->handler->applyLogEntry(CMD_CLEAR_LIST, Address());
        this->handler->applyLogEntries(this->logEntries);
        this->lastAppliedIndex = static_cast<int>(this->logEntries.size() - 1);
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
        test.validateLogEntries(this->logEntries, 0, this->lastAppliedIndex+1);
    }

}

void Context::applyCommittedEntries()
{
    if (this->commitIndex > this->lastAppliedIndex) {
        for (INDEX i=this->lastAppliedIndex; i<=this->commitIndex; i++) {
            // Apply log entries to match
            this->handler->applyLogEntry(this->logEntries[i]);
        }
        this->lastAppliedIndex = this->commitIndex;
        this->setLogChanged(true);
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
    int total = 0;
    for (const auto & elem : this->followers) { 
        if (elem.second.matchIndex >= sentLogIndex)
            total++;
    }
    if (2*total > this->followers.size()) {
        this->commitIndex = sentLogIndex;
    }
}

void SanityTestLog::validateLogEntries(const vector<RaftLogEntry>& entries,
                                       INDEX start, INDEX count)
{
    for (int i=0; i<count; i++) {
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

