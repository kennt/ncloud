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


void RaftLogEntry::write(stringstream& ss)
{
    //$ TODO: Do I need to truncate the stream first?
    write_raw<int>(ss, this->termReceived);
    write_raw<int>(ss, static_cast<int>(this->command));
    write_raw<Address>(ss, this->address);
}

void RaftLogEntry::read(istringstream& is)
{
    this->termReceived = read_raw<int>(is);
    this->command = static_cast<Command>(read_raw<int>(is));
    this->address = read_raw<Address>(is);
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

void Context::init(RaftMessageHandler *handler,
                   ContextStoreInterface *store)
{
    Address     nullAddress;    // 0.0.0.0:0

    this->handler = handler;
    this->store = store;
    this->electionTimeout = 0;

    // Start up as a follower node
    this->currentState = State::FOLLOWER;
    this->commitIndex = 0;
    this->lastAppliedIndex = 0;

    this->leaderAddress = nullAddress;
    this->currentTerm = 0;
    this->votedFor = nullAddress;
    this->followers.clear();

    this->logEntries.clear();
    this->logEntries.emplace_back();

    this->votesReceived = 0;
    this->votesTotal = 0;

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

    // Read in leaderAddress, currentTerm, votedForAddress
    // and log entries
    unsigned short port;
    port = root.get("leaderPort", 0).asInt();
    this->leaderAddress.parse(
        root.get("leaderAddress", "0.0.0.0").asString().c_str(), port);

    this->currentTerm = root.get("currentTerm", 0).asInt();

    port = root.get("votedForPort", 0).asInt();
    this->votedFor.parse(
        root.get("votedFor", "0.0.0.0").asString().c_str(), port);

    Json::Value log = root["log"];
    for (int i=0; i<log.size(); i++) {
        RaftLogEntry    entry;
        entry.termReceived = log[i].get("term", 0).asInt();
        entry.command = static_cast<Command>(log[i].get("command", 0).asInt());
        entry.address.parse(
            log[i].get("address", "0.0.0.0").asString().c_str(),
            static_cast<unsigned short>(log[i].get("port", 0).asInt()));

        this->logEntries.push_back(entry);

        // Perform this action on the list of members
        handler->applyLogEntry(entry);
    }

    // apply the log entries
    // Update the context
    this->lastAppliedIndex = log.size() - 1;
}

// Persists the context to the ContextStore, then
// updates the context variables as needed.
void Context::saveToStore()
{
    if (!store)
        throw NetworkException("Raft::Context no store provided");

    Json::Value root;

    root["leaderAddress"] = this->leaderAddress.toString();
    root["currentTerm"] = this->currentTerm;
    root["votedFor"] = this->votedFor.toString();

    Json::Value log;
    for (auto & entry: this->logEntries) {
        Json::Value logEntry;
        logEntry["term"] = entry.termReceived;
        logEntry["command"] = static_cast<int>(entry.command);
        logEntry["address"] = entry.address.toString();
        log.append(logEntry);
    }
    root["log"] = log;

    store->write(root);

    // Update the context
    this->commitIndex = static_cast<int>(this->logEntries.size() - 1);
}

// Adds a member to the membership list.  This will also
// take care of all the necessary log-related activites.
void Context::addMember(const Address& address)
{
    assert(this->lastAppliedIndex == (this->logEntries.size()-1));

    // Update the context entries (note that the entry has not
    // been persisted yet).
    RaftLogEntry    entry;
    entry.termReceived = this->currentTerm;
    entry.command = CMD_ADD_SERVER;
    entry.address = address;

    this->logEntries.push_back(entry);
    this->handler->applyLogEntry(entry);
    this->lastAppliedIndex++;
    assert(this->lastAppliedIndex == (this->logEntries.size() - 1));
}

void Context::removeMember(const Address& address)
{
    // Update the context entries (note that the entry has not
    // been persisted yet).
    RaftLogEntry    entry;
    entry.termReceived = this->currentTerm;
    entry.command = CMD_REMOVE_SERVER;
    entry.address = address;

    this->logEntries.push_back(entry);
    this->handler->applyLogEntry(entry);
    this->lastAppliedIndex++;
    assert(this->lastAppliedIndex == (this->logEntries.size() - 1));
}

void Context::onTimeout()
{
    if (this->commitIndex > this->lastAppliedIndex) {
        for (int i=this->lastAppliedIndex; i<=this->commitIndex; i++) {
            // Apply log entries to match
            this->handler->applyLogEntry(this->logEntries[i]);
        }
        this->lastAppliedIndex = this->commitIndex;
    }
}

void Context::resetTimeout()
{
    this->electionTimeout = par->getCurrtime() + par->electionTimeout;
}

void Context::startElection(const MemberInfo& member)
{
    DEBUG_LOG(this->handler->address(),
        "Starting election : term %d", this->currentTerm+1);

    // Increment current term
    this->currentTerm++;

    // Vote for ourselves
    this->votesTotal = static_cast<int>(member.memberList.size());
    this->votesReceived = 1;

    // Reset election timer
    this->resetTimeout();

    // Send RequestVote RPCs to all other servers
    RequestVoteMessage  request;
    request.transId = this->handler->getNextMessageId();
    request.term = this->currentTerm;
    request.candidate = this->handler->address();
    request.lastLogIndex = static_cast<int>(this->logEntries.size()-1);
    request.lastLogTerm = this->logEntries.back().termReceived;

    this->handler->broadcast(&request);
}

