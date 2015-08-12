/*****
 * Context.h
 *
 * See LICENSE for details.
 *
 * Holds the classes needed to manage the state for Raft.
 *
 *
 *****/


#ifndef NCLOUD_CONTEXT_H
#define NCLOUD_CONTEXT_H

#include <functional>
#include <unordered_map>

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Network.h"
#include "Member.h"
#include "Util.h"

class ContextStoreInterface;
class NetworkNode;


namespace Raft {

class Transaction;
class ElectionTransaction;

// ================
// System states (for the state table)
// ================

enum State {
    // This is the uninitialized state, the state has not
    // joined the cluster yet (hasn't send the ADD_SERVER message).
    NONE = 0,

    // Normal RAFT states
    FOLLOWER,
    CANDIDATE,
    LEADER
};


// ================
// Enumeration for operations performed by the log entries.
//
// Since we are using the log to store operations on the cluster
// membership list.  All we are have are add server and remove server.
// (Note that operations are performed one server address at a time).
// ================
enum Command {
    CMD_NONE = 0,       // This should not appear in the log
    CMD_NOOP,
    CMD_ADD_SERVER,
    CMD_REMOVE_SERVER,

    //$ HACK: THis is a special command that is used for
    // clearing the membership list.  Only intended for
    // use by the Context class.
    CMD_CLEAR_LIST = 1000
};


// ================
// This is not the logging class, but a log file that keeps
// track of operations used by the system.
// ================

// Since we are using Raft only for the membership protocol,
// We are only storing only the membership information in the
// log.
// This are the contents of an individual log entry.
struct RaftLogEntry {
    // term when the entry was received by the leader
    int         termReceived;

    // Since we are only using the Raft protocol for cluster
    // membership, we are storing a delta of changes to the 
    // cluster membership. (nodes added and node deleted).
    //
    // We could store the entire list, but I think this makes
    // it more interesting to see how things work.
    // 

    // The operation being performed.
    Raft::Command     command;

    // This is the address being operated on (either added or deleted).
    Address     address;

    RaftLogEntry() : termReceived(0), command(Command::CMD_NOOP)
    {}

    RaftLogEntry(int term, Raft::Command command, const Address& addr)
        : termReceived(term), command(command), address(addr)
    {}

    // Used for load/save for messages
    void write(stringstream& ss);
    void read(istringstream& is);
};


// ==================
// This provides the access methods to the persistent data store
// for the context object. This assumes that JSON messages (strings
// are being stored).
//
// The entire context is read/written at once.
//
// ==================
class ContextStoreInterface
{
public:
    virtual ~ContextStoreInterface() {};

    // Reads in the entire persisted context
    virtual Json::Value read() = 0;

    // Writes out the entire context
    virtual void write(const Json::Value& value) = 0;

    // Tests the store to see if it is empty
    virtual bool empty() = 0;

    // Clears the storage and context
    // Calling read() after this will have no
    // effect on the values.
    virtual void reset() = 0;
};

// ==================
// Provides the basic interface to store the context
// to a file in the filesystem.  This will overwrite the 
// entirety of the file contents, it does not do an append.
//
//$ TODO: check, should this be doing a file append?  What
// happens if the file write fails?  Is it better to have
// the old context or an entirely blank context on startup?
//
// ==================
class FileBasedContextStore : public ContextStoreInterface
{
public:
    FileBasedContextStore(const char *filename);
    virtual ~FileBasedContextStore();

    virtual Json::Value read() override;
    virtual void write(const Json::Value& value) override;
    virtual bool empty() override;
    virtual void reset() override;
};

// ==================
// This is provided for debugging/testing purposes.
// It does NOT store the data anywhere.  However, this
// will provide a history of what was written to it.
//
// Accordingly, when this is created, there is NO
// persisted state associated with it by default. (although you
// could modify the data member manually).
// ==================
class MemoryBasedContextStore : public ContextStoreInterface
{
public:
    MemoryBasedContextStore(Params *par);
    virtual ~MemoryBasedContextStore() {}

    virtual Json::Value read() override;
    virtual void write(const Json::Value& value) override;
    virtual bool empty() override;
    virtual void reset() override;

    Params *par;

    // A list of the times the data has been written out
    // is stored, however only the data from the last write()
    // is stored.
    struct ContextEntry {
        int     timestamp;
        time_t  tm;

        ContextEntry(int timestamp, time_t tm) : timestamp(timestamp), tm(tm) {}
    };
    vector<ContextEntry> entries;
    Json::Value     current;
};


// ==================
// Encapsulates all the state needed by the Raft algorithm.
// This is purely a way to aggregate all of the state
// information into one place, instead of members in the
// RaftHandler.
// ==================
class RaftHandler;

struct Context
{
    Context(Log *log, Params *par) : par(par), log(log),
        handler(nullptr), store(nullptr), 
        currentState(State::NONE),
        commitIndex(0), lastAppliedIndex(0), currentTerm(0)
    {}

    void init(RaftHandler *handler,
              ContextStoreInterface *store);

    // ==================
    // Implementation variables
    // ==================

    Params *                par;
    Log *                   log;

    // Pointer to the handler that contains this context
    // The lifetime of the context is bound to the handler
    // (so there is no need to release/delete the pointer).
    RaftHandler *    handler;

    // The store interface to used when loading/saving
    // the persisted context data.
    //
    // it is assumed that this lifetime is longer than the
    // lifetime of the context object (thus just a pointer to
    // the store interface is held).
    ContextStoreInterface * store;

    // ==================
    // Volatile state stored on all servers
    // ==================

    State       currentState;

    // Index of highest log entry known to be committed
    // (initialized to 0, increases monotonically)
    int         commitIndex;

    // Index of highest log entry applied
    // (initialized to 0, increase monotonically)
    int         lastAppliedIndex;

    // ==================
    // Persistent state for the system
    // (Updated on stable storage before responding to RPCs)
    // ==================
    // Current leader address
    Address     currentLeader;

    // The latest term that this server has seen (initialized to 0 on
    // first boot, increases monotonically).
    int         currentTerm;

    // Candidate that received a vote in the current term (or null if none)
    Address     votedFor;

    // Membership changes are communicated as log changes.  See Sect 3.4.
    // log entries, each log entry contains command for the state machine,
    // and term when entry was received by the leader (first index is 1).
    //
    // Note that the log starts out with a dummy entry at position 0.
    // This is due to Raft starting its log index at 1 (this is to avoid
    // confusion).    
    vector<RaftLogEntry>    logEntries;

    // Serialization APIs
    void saveToStore();
    void loadFromStore();

    // ==================
    // Volatile state stored on a leader node for all
    // follower nodes.
    // (Reinitialized after election)
    // ==================
    struct LeaderStateEntry {
        // Index of the next log entry to send to this server
        // (initialized to leader last log index + 1)
        int         nextIndex;

        // Index of highest log entry known to be replicated on
        // server (initialized to 0, increases monotonically)
        int         matchIndex;

        LeaderStateEntry() : nextIndex(0), matchIndex(0)
        {}
    };

    std::unordered_map<Address, LeaderStateEntry> followers;

    // APIs for context manipulation
    void addMember(const Address& address);
    void removeMember(const Address& address);

    // Perform any actions/checking of the timeout
    void onTimeout();

    void startElection(const MemberInfo& member);

    // Add new entries to the log, if an old entry conflicts they will
    // remove the conflict and all succeeding entries. Appends new
    // entries to the log after that.
    void addEntries(int startIndex, vector<RaftLogEntry> & entries);

    // Applies committed but not-yet-applied entries
    void applyCommittedEntries();

    // APIs for switching states
    void switchToLeader();
    void switchToCandidate();
    void switchToFollower();

    int getLastLogIndex()   { return static_cast<int>(this->logEntries.size() - 1); }
    int getLastLogTerm()    { return this->logEntries.back().termReceived; }
};


// Use this class to test the validity of a set of RaftLogEntry
// operations.  This checks to for invalid states: thus you can't
// remove a server that's not there or add a server twice (without
// having removed it).
class SanityTestLog
{
public:
    vector<RaftLogEntry>    entries;
    set<Address>    servers;
    int             lastTermSeen;

    SanityTestLog() : lastTermSeen(-1) {}

    // Performs basically the following:
    // for (int i=0; i<count; i++)
    //      validate entries[start+i]
    //
    void validateLogEntries(const vector<RaftLogEntry> &entries,
                            int start, int count);
};


}

#endif /* NCLOUD_CONTEXT_H */

