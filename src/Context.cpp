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

bool RaftLog::contains(int pos, int term) const
{
    assert(pos > 0);

    // Determine the real index (in the face of compaction)
    int     realIndex = pos - 1;

    //$ TODO: What to do with queries that are further back in 
    // the past?

    if (this->entries.size() >= realIndex)
        return false;

    return this->entries[realIndex].termReceived == term;
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

    // Start up as a follower node
    this->currentState = State::FOLLOWER;
    this->commitIndex = 0;
    this->lastAppliedIndex = 0;

    this->leaderAddress = nullAddress;
    this->currentTerm = 0;
    this->candidateAddress = nullAddress;
    this->raftLog.clear();
    this->followers.clear();

    // Reload the persisted state
    this->loadFromStore();
}

// Loads the context from the ContextStore, then
// updates the context variables as needed.
void Context::loadFromStore()
{
    if (!store)
        throw NetworkException("Raft::Context no store provided");

    Json::Value root = store->read();

    // Read in leaderAddress, currentTerm, candidateAddress
    // and log entries
    unsigned short port;
    port = root.get("leaderPort", 0).asInt();
    this->leaderAddress.parse(
        root.get("leaderAddress", "0.0.0.0").asString().c_str(), port);

    this->currentTerm = root.get("currentTerm", 0).asInt();

    port = root.get("candidatePort", 0).asInt();
    this->candidateAddress.parse(
        root.get("candidateAddress", "0.0.0.0").asString().c_str(), port);

    Json::Value log = root["log"];
    for (int i=0; i<log.size(); i++) {
        RaftLogEntry    entry;
        entry.termReceived = log[i].get("term", 0).asInt();
        entry.command = static_cast<Command>(log[i].get("command", 0).asInt());
        entry.address.parse(
            log[i].get("address", "0.0.0.0").asString().c_str(),
            static_cast<unsigned short>(log[i].get("port", 0).asInt()));

        this->raftLog.entries.push_back(entry);

        // Perform this action on the list of members
        handler->applyLogEntry(entry);
    }

    // apply the log entries
    // Update the context
    this->lastAppliedIndex = log.size();
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
    root["candidateAddress"] = this->candidateAddress.toString();

    Json::Value log;
    for (auto & entry: this->raftLog.entries) {
        Json::Value logEntry;
        logEntry["term"] = entry.termReceived;
        logEntry["command"] = static_cast<int>(entry.command);
        logEntry["address"] = entry.address.toString();
        log.append(logEntry);
    }
    root["log"] = log;

    store->write(root);

    // Update the context
    this->commitIndex = static_cast<int>(this->raftLog.entries.size());
}

// Adds a member to the membership list.  This will also
// take care of all the necessary log-related activites.
void Context::addMember(const Address& address)
{
    // Add an entry into the log
    throw NYIException("implement Context::addServer", __FILE__, __LINE__);
}

void Context::removeMember(const Address& address)
{
    throw NYIException("implment remove server", __FILE__, __LINE__);
}
