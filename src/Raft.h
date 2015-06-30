/*****
 * Raft.h
 *
 * See LICENSE for details.
 *
 * Holds the classes needed to receive and handle messages for the 
 * Membership protocol (using the Raft protocol).
 *
 *
 *****/


#ifndef NCLOUD_RAFT_H
#define NCLOUD_RAFT_H

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "json/json.h"
#include "NetworkNode.h"


class NetworkNode;


namespace Raft {

// ================
// Message types
// ================

enum MessageType {  
    MSGTYPE_NONE = 0,
    APPEND_ENTRIES,
    APPEND_ENTRIES_REPLY,
    REQUEST_VOTE,
    REQUEST_VOTE_REPLY
    ADD_SERVER,
    ADD_SERVER_REPLY,
    REMOVE_SERVER,
    REMOVE_SERVER_REPLY
};

// ================
// Enumeration for operations performed by the log entries.
//
// Since we are using the log to store operations on the cluster
// membership list.  All we are have are add server and remove server.
// (Note that operations are performed one server address at a time).
// ================
enum Command {
    CMD_NONE = 0,
    CMD_ADD_SERVER,
    CMD_REMOVE_SERVER
};


// ================
// System states (for the state table)
// ================

enum State {
    // This is the uninitialized state, the state has not
    // joined the cluster yet (hasn't send the ADD_SERVER message).
    NONE = 0,

    // We are trying to join the cluster, but have not received
    // a reply yet.
    JOINING = 1,

    // Normal RAFT states
    FOLLOWER = 2,
    CANDIDATE = 3,
    LEADER = 4
}

// Since we are using Raft only for the membership protocol,
// We are only storing only the membership information in the
// log.
// This are the contents of an individual log entry.
struct MemberLogEntry {
    // term when the entry was received by the leader
    int         termReceived;


    // Log entry data
    // ==============

    // Since we are only using the Raft protocol for cluster
    // membership, we are storing a delta of changes to the 
    // cluster membership. (nodes added and node deleted).
    //
    // We could store the entire list, but I think this makes
    // it more interesting to see how things work.
    //
    // 

    // The operation being performed.
    Command     command;

    // This is the address being operated on (either added or deleted).
    Address     address;

    MemberLogEntry() : termReceived(0)
    {}

};


// Implements the behavior needed for a Raft-based log.
// Note that the indexes are all 1-based!
class RaftLog
{
public:
    RaftLog() : prevIndex(0), prevTerm(0)
    {}

    // returns true if the entry at position pos matches term
    bool    contains(int pos, int term) const;
    
    void    append();
    size_t  size() const;
    bool    empty() const;

    // APIs for compaction?
protected:
    vector<MemberLogEntry>  entries;

    // Compaction?
    // index of last discarded entry (initialized to 0)
    int     prevIndex;

    // term of last discarded entry (initialized to 0)
    int     prevTerm;

    // latest cluster memberhsip configuration up through prevIndex
    vector<Adddress>    prevAddresses;
};


// Volatile state stored on a leader node
// (this should actally stored as a map<Address, LeaderStateEntry>,
// since we are storing information on each server).
// (Reinitialized after election)
struct LeaderStateEntry {
    // Index of the next log entry to send to this server
    // (initialized to leader last log index + 1)
    int         nextIndex;

    // Index of highest log entry known to be replicated on
    // server (initialized to 0, increases monotonically)
    int         matchIndex;

    LeaderState() : nextIndex(0), matchIndex(0)
    {}
};

// Volatile state stored on all servers
struct ServerState {
    // Index of highest log entry known to be committed
    // (initialized to 0, increases monotonically)
    int         commitIndex;

    // Index of highest log entry applied to state machine
    // (initialized to 0, increase monotonically)
    int         lastAppliedIndex;

    MemberState() : commitIndex(0), lastAppliedIndex(0)
    {}
};

// Persistent state for the system
// (Updated on stable storage before responding to RPCs)
struct SavedState {
    // Current leader address
    Address     leaderAddress;


    // The latest term that this server has seen (initialized to 0 on
    // first boot, increases monotonically).
    int         currentTerm;

    // Candidate that received a vote in the current term (or null if none)
    Address     candidateAddress;

    // Membership changes are communicated as log changes.  See Sect 3.4.
    // log entries, each log entry contains command for the state machine,
    // and term when entry was received by the leader (first index is 1).
    vector<MemberLogEntry>  membershipLog;

    SavedState() : currentTerm(0)
    {}

    // Serialization APIs
    Json::Value toJson();
    void load(const Json::Value& value);
};


// ==================
// Messages
// ==================

struct AppendEntriesMessage
{
    // Implied MessageType::APPEND_ENTRIES

    // leader's term
    int         term;

    // so follower can redirect clients
    Address     leaderAddress;

    // index of log entry immediately preceding new ones
    int         prevLogIndex;

    // term of prevLogIndex entry
    int         prevLogTerm;

    // Log entries to store (empty for heartbeat, may send
    // more than one for efficiency)
    vector<MemberLogEntry>  entries;

    // leader's commit index
    int         leaderCommit;

    AppendEntriesMessage() : term(0), prevLogIndex(0), prevLogTerm(0), leaderCommit(0)
    {}

    // Loads the data from the stringstream into this object.
    void load(istringstream& ss);

    // Creates a RawMessage and serializes the data for this object
    // into the RawMessage data.  Ownership of the RawMessage is passed
    // to the caller.
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct AppendEntriesReply
{
    // Implied MessageType::APPEND_ENTRIES_REPLY

    // currentTerm, for the leader to update itself
    int         term;

    // true if follower contained entry matching prevLogIndex and prevLogTerm
    bool        success;

    AppendEntriesReply() : term(0), success(false)
    {}

    // Loads the data from the stringstream into this object.
    void load(istringstream& ss);

    // Creates a RawMessage and serializes the data for this object
    // into the RawMessage data.  Ownership of the RawMessage is passed
    // to the caller.
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct RequestVoteMessage {
    // Implied MessageType::REQUEST_VOTE

    // candidate's term
    int         term;

    // candidate requesting vote
    Address     candidateAddress;

    // index of candidate's last log entry
    int         lastLogIndex;

    // term of candidate's last log entry
    int         lastLogTerm;

    
    RequestVoteMessage() : term(0), lastLogIndex(0), lastLogTerm(0)
    {}

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);        
};

struct RequestVoteReply {
    // Implied MessageType::REQUEST_VOTE_REPLY

    // currentTerm, for candidate to update itself
    int         term;

    // true means the candidate received the vote
    bool        voteGranted;

    RequestVoteReply() : term(0), voteGranted(false)
    {}

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);            
};

struct AddServerMessage {
    // Address of server to add to configuration
    Address     newServer;

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);            
};

struct AddServerReply {
    // true if server was added succesfully
    bool        status;

    // Address of recent leader, if known
    Address     leaderHint;

    AddServerReply() : status(false) 
    {}

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);            
};


struct RemoveServerMessage {
    // Address of server to remove from configutation
    Address     oldServer;

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);            
};


struct RemoveServerReply {
    // true if server was removed successfully
    bool        status;

    // Address of recent leader, if known
    Address     leaderHint;

    RemoveServerReply() : status(false) 
    {}

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);            
};


class RaftMessageHandler: public IMessageHandler
{
public:
    RaftMessageHandler(Log *log, 
                      Params *par,
                      shared_ptr<NetworkNode> netnode,
                      shared_ptr<IConnection> connection)
        : log(log), par(par), netnode(netnode), connection(connection), timeout(0),
        joinTimeout(0), nextHeartbeat(0)
    {
    }

    virtual ~RaftMessageHandler() {}

    // Initializes the MessageHandler, if needed. This will be called
    // before onMessageReceived() or onTimeout() will be called.
    virtual void start(const Address &address) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

    // Performs the action to join a group (sending a JoinRequest message).
    void joinGroup(const Address& address);

    // Change the state and perform any actions needed
    void stateChange(State newState);

    // Handlers for the individual messages
    void onAppendEntries(const Address& from, istringstream& ss);
    void onRequstVote(const Address& from, istringstream& ss);
    void onAddServer(const Address& from, istringstream& ss);
    void onAddServerReply(const Address& from, istringstream& ss);

protected:
    Log *                   log;
    Params *                par;
    weak_ptr<NetworkNode>   netnode;
    shared_ptr<IConnection> connection;
    int                     timeout;

    State                   currentState;

    // Volatile state (on all servers)
    ServerState             serverState;

    // Persistent state (on all servers)
    SavedState              savedState;

    // Volatile state (only on leaders)
    map<Address, LeaderStateEntry>  servers;

    // This is what is maintained by raft
    // The logEntries dictate the changes made to the address list
    RaftLog                 logEntries;
    vector<Address>         addresses;

    // This timeout is used upon joining.  If we do not receive
    // a reply by this timeout, then we cannot contact a leader
    // If this is 0, ignore
    int                     joinTimeout;

    // The time to send out the next heartbeat, if set to 0
    // then this will never trigger.
    int                     nextHeartbeat;
};

}

#endif /* NCLOUD_RAFT_H */

