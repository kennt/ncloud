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


namespace Raft {

class RaftHandler;

using INDEX = unsigned long;
using TERM = unsigned long;

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
// We are only storing only the membership information in the log.
// These are the contents of an individual log entry.
struct RaftLogEntry {
    // term when the entry was received by the leader
    TERM            termReceived;

    // Since we are only using the Raft protocol for cluster
    // membership, we are storing a delta of changes to the 
    // cluster membership. (nodes added and node deleted).
    //

    // The operation being performed.
    Raft::Command     command;

    // This is the address being operated on (either added or deleted).
    Address     address;

    RaftLogEntry() : termReceived(0), command(Command::CMD_NOOP)
    {}

    RaftLogEntry(TERM term, Raft::Command command, const Address& addr)
        : termReceived(term), command(command), address(addr)
    {}

    void write(stringstream& ss);
    void read(istringstream& is);
};


// ==================
// This provides the access methods to the persistent data store
// for the context object. This assumes that JSON messages (strings
// are being stored).
//
// The entire context is read/written at once.
// ==================
class StorageInterface
{
public:
    virtual ~StorageInterface() {};

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
// to a file in the filesystem.
// ==================
class FileBasedStorage : public StorageInterface
{
public:
    FileBasedStorage(const char *filename);
    virtual ~FileBasedStorage();

    virtual Json::Value read() override;
    virtual void write(const Json::Value& value) override;
    virtual bool empty() override;
    virtual void reset() override;

protected:
    std::string     filename;
    std::fstream    fs;
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
class MemoryBasedStorage : public StorageInterface
{
public:
    MemoryBasedStorage(Params *par);
    virtual ~MemoryBasedStorage() {}

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
// Stores a snapshot of the "state machine".
// This is stored separately so that each transaction that
// sends a snapshot will keep a shared_ptr to this object.
// (Allows me to generate a snapshot without interrupting
// ongoing snapshot trnasactions).
// ==================
struct Snapshot
{
    vector<Address> prevMembers;

    // Location information
    INDEX           prevIndex;
    TERM            prevTerm;

    Snapshot() : prevIndex(0), prevTerm(0)
    {}
    Snapshot(INDEX index, TERM term, const vector<Address>& addresses)
        : prevIndex(index), prevTerm(term)
    {
        this->prevMembers = addresses;
    }
};


// ==================
// Encapsulates all the state needed by the Raft algorithm.
// This is purely a way to aggregate all of the state
// information into one place, instead of members in the
// RaftHandler.
// ==================
struct Context
{
    Context(Log *log, Params *par) : par(par), log(log),
        handler(nullptr), store(nullptr), 
        currentState(State::NONE),
        commitIndex(0), lastAppliedIndex(0), currentTerm(0),
        logChanged_(false), prevIndex(0), prevTerm(0)
    {}

    void init(RaftHandler *handler,
              StorageInterface *store,
              StorageInterface *snapshotStore);

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
    StorageInterface * store;
    StorageInterface * snapshotStore;

    // ==================
    // Volatile state stored on all servers
    // ==================
    State       currentState;

    // Index of highest log entry known to be committed
    // (initialized to 0, increases monotonically)
    INDEX       commitIndex;

    // Index of highest log entry applied
    // (initialized to 0, increase monotonically)
    INDEX       lastAppliedIndex;

    // Current leader address
    Address     currentLeader;

    // ==================
    // Persistent state for the system
    // (Updated on stable storage before responding to RPCs)
    // ==================
    // The latest term that this server has seen (initialized to 0 on
    // first boot, increases monotonically).
    TERM        currentTerm;

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
    bool                    logChanged_;

    // Snapshot information
    // This contains information about the current snapshot
    // if one has been taken (leader) or has been applied (follower).
    // The currentSnapshot is persisted.
    shared_ptr<Snapshot>    currentSnapshot;

    // This is kept by the follower to accumulate the state
    // information. Thus the information here may not be up-to-date.
    shared_ptr<Snapshot>    workingSnapshot;

    // If the current setup has been snapshotted, this is the
    // location/term of the last entry that has been applied
    // to the snapshot. In other words, we do not have logEntries
    // for anything <= prevIndex;
    //$ TODO: redundant, this information is in currentSnapshot
    INDEX                   prevIndex;
    TERM                    prevTerm;

    // Some APIs to help index calculations (to account for the snapshot)
    // Returns the term at the given index (offset by the snapshot)
    TERM    termAt(INDEX index)
    { 
        if (this->prevIndex && (this->prevIndex == index))
            return this->prevTerm;
        return entryAt(index).termReceived;
    }

    // Returns the entry at the given index (offset by the snapshot)
    RaftLogEntry entryAt(INDEX index)
    {
        return logEntries[realIndex(index)];
    }

    // Returns the actual position of the index (offset by the snapshot)
    INDEX realIndex(INDEX index)
    {
        // The calculations get weird because of the behavior when prevIndex = 0.
        // A prevIndex > 0 indicates a snapshot including prevIndex
        // however prevIndex == 0, indicates no snapshot
        //$ TODO: all of this would be unnessary if prevIndex was -1
        // if no snapshot existed, however an index is an unsigned value.
        if (index > getLastLogIndex())
            throw AppException("index above bounds");

        if (prevIndex == 0)
            return index;

        if (index <= prevIndex)
            throw AppException("index below bounds");
        return index - prevIndex - 1;
    }

    // Serialization APIs
    void saveToStore();
    void loadFromStore();
    void saveSnapshotToStore(shared_ptr<Snapshot> snapshot);
    void loadSnapshotFromStore();

    // ==================
    // Volatile state stored on a leader node for all
    // follower nodes.
    // (Reinitialized after election)
    // ==================
    struct LeaderStateEntry {
        // Index of the next log entry to send to this server
        // (initialized to leader last log index + 1)
        INDEX       nextIndex;

        // Index of highest log entry known to be replicated on
        // server (initialized to 0, increases monotonically)
        INDEX       matchIndex;

        LeaderStateEntry() : nextIndex(0), matchIndex(0)
        {}
        LeaderStateEntry(INDEX next, INDEX match)
            : nextIndex(next), matchIndex(match)
        {}
    };

    std::unordered_map<Address, LeaderStateEntry> followers;


    // APIs for context manipulation
    void changeMembership(Command cmd, const Address& address);

    // Perform any actions/checking of the timeout
    void onTimeout();

    // Call this when switching to a candidate
    void startElection(const MemberInfo& member);

    // Add new entries to the log, if an old entry conflicts they will
    // remove the conflict and all succeeding entries. Appends new
    // entries to the log after that.
    void addEntries(INDEX startIndex, vector<RaftLogEntry> & entries);

    // Applies committed but not-yet-applied entries
    void applyCommittedEntries();

    // Returns true if we need to take a snapshot (due to log growth)
    bool isSnapshotNeeded(INDEX threshold)
    {
        return ((prevIndex == 0 && commitIndex+1 >= threshold) ||
                (prevIndex != 0 && commitIndex- prevIndex >= threshold));
    }
    // Creates a snapshot of the current config and saves it
    void takeSnapshot();

    // Runs through the log entries to build the configuration
    // (used for snapshotting).  This will run up and including toIndex.
    vector<Address> runLogEntries(INDEX toIndex);

    // APIs for switching states
    void switchToLeader();
    void switchToCandidate();
    void switchToFollower();

    INDEX getLastLogIndex()
    { 
        if (this->prevIndex)
            return this->prevIndex + this->logEntries.size();
        else
            return this->logEntries.size() - 1;
    }
    TERM getLastLogTerm()    { return termAt(getLastLogIndex()); }

    // Use this to trigger sending of log updates
    bool logChanged() const { return logChanged_; }
    void setLogChanged(bool val) { logChanged_ = val; }

    // Check to see if the commitIndex needs to be updated.
    // Call this whenever a change is made to any of the matchIndex
    // values.  This will update the commitInde if needed.
    void checkCommitIndex(INDEX sentLogIndex);
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
    TERM            lastTermSeen;

    SanityTestLog() : lastTermSeen(0) {}

    // Initializes the state from the snapshot
    // nullptr may be passed in to initialize to an empty state.
    void init(Snapshot *snapshot);

    // Performs basically the following:
    // for (int i=0; i<count; i++)
    //      validate entries[start+i]
    //
    void validateLogEntries(const vector<RaftLogEntry> &entries,
                            INDEX start, INDEX count);
};


}

#endif /* NCLOUD_CONTEXT_H */

