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

#include <functional>
#include <unordered_map>

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "json/json.h"
#include "Network.h"
#include "NetworkNode.h"
#include "Context.h"
#include "Command.h"
#include "Util.h"

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
    REQUEST_VOTE_REPLY,
    ADD_SERVER,
    ADD_SERVER_REPLY,
    REMOVE_SERVER,
    REMOVE_SERVER_REPLY
};

// ==================
// Messages
// ==================

class Message
{
public:
    MessageType     msgtype;
    int             transId;

    Message(MessageType mt) : msgtype(mt), transId(0) {}

    virtual ~Message() = default;
    Message(Message &&) = default;
    Message& operator=(Message&&) = default;
    Message(const Message&) = default;
    Message& operator=(const Message&) = default;

    // Loads the data from the stringstream into this object.
    virtual void load(istringstream& ss) = 0;

    // Creates a RawMessage and serializes the data for this object
    // into the RawMessage data.  Ownership of the RawMessage is passed
    // to the caller.
    virtual unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to) = 0; 
};

class AppendEntriesMessage : public Message
{
public:
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
    vector<RaftLogEntry>  entries;

    // leader's commit index
    int         leaderCommit;

    AppendEntriesMessage() : Message(APPEND_ENTRIES), term(0), prevLogIndex(0), prevLogTerm(0), leaderCommit(0)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class AppendEntriesReply : public Message
{
public:
    // Implied MessageType::APPEND_ENTRIES_REPLY

    // currentTerm, for the leader to update itself
    int         term;

    // true if follower contained entry matching prevLogIndex and prevLogTerm
    bool        success;

    AppendEntriesReply() : Message(APPEND_ENTRIES_REPLY), term(0), success(false)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class RequestVoteMessage : public Message
{
public:
    // Implied MessageType::REQUEST_VOTE

    // candidate's term
    int         term;

    // candidate requesting vote
    Address     candidateAddress;

    // index of candidate's last log entry
    int         lastLogIndex;

    // term of candidate's last log entry
    int         lastLogTerm;

    
    RequestVoteMessage() : Message(REQUEST_VOTE), term(0), lastLogIndex(0), lastLogTerm(0)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class RequestVoteReply : public Message
{
public:
    // Implied MessageType::REQUEST_VOTE_REPLY

    // currentTerm, for candidate to update itself
    int         term;

    // true means the candidate received the vote
    bool        voteGranted;

    RequestVoteReply() : Message(REQUEST_VOTE_REPLY), term(0), voteGranted(false)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class AddServerMessage : public Message
{
public:
    // Implied MessageType::ADD_SERVER

    // Address of server to add to configuration
    Address     newServer;

    AddServerMessage() : Message(ADD_SERVER) {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class AddServerReply : public Message
{
public:
    // Implied MessageType::ADD_SERVER_REPLY

    // true if server was added succesfully
    bool        status;

    // Address of recent leader, if known
    Address     leaderHint;

    AddServerReply() : Message(ADD_SERVER_REPLY), status(false) 
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};


class RemoveServerMessage : public Message
{
public:
    // Implied MessageType::REMOVE_SERVER

    // Address of server to remove from configutation
    Address     oldServer;

    RemoveServerMessage() : Message(REMOVE_SERVER) {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};


class RemoveServerReply : public Message
{
public:
    // Implied MessageType::REMOVE_SERVER_REPLY

    // true if server was removed successfully
    bool        status;

    // Address of recent leader, if known
    Address     leaderHint;

    RemoveServerReply() : Message(REMOVE_SERVER_REPLY), status(false) 
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};


// The RaftMessageHandler uses message IDs as a way to distringuish RPC replies.
// Due to the network, replies may be received out-of-order.  Thus we use
// this map to keep track of our outstanding transactions.
//
// Also, messages that are received after the timeout has passed are dropped
// with no further notificqtion.  The callbacks are optional.  Only one of the
// onTimeout() on onReply() will be called.
//
struct Transaction
{
    int         transId;

    int         timeSent;
    int         timeout;

    // The original message that was sent
    Address     toAddress;
    shared_ptr<Message> message;

    // Optional function to call on timeout
    std::function<void (Transaction *trans)> onTimeout;

    // Optional function to call when a reply is received
    std::function<void (Transaction *trans, shared_ptr<Message> reply)> onReply;
};


// The MessageHandler for the Raft protocol.  This replaces the normal
// message-handling protocol for Group Membership.
//
class RaftMessageHandler: public IMessageHandler
{
public:
    // The log, par, and store are direct pointers, and the
    // objects are expected to live beyond the lifetime
    // of the MessageHandlers.
    // Since these belong to the system, this is a reliable
    // assumption.
    //
    RaftMessageHandler(Log *log, 
                      Params *par,
                      ContextStoreInterface *store,
                      shared_ptr<NetworkNode> netnode,
                      shared_ptr<IConnection> connection)
        : log(log), par(par), store(store), netnode(netnode), connection(connection),
        electionTimeout(0), heartbeatTimeout(0), nextHeartbeat(0)
    {
    };

    virtual ~RaftMessageHandler() {};

    // Initializes the MessageHandler, if needed. This will be called
    // before onMessageReceived() or onTimeout() will be called.
    //
    // Call this with a zero'ed address (0.0.0.0:0) for the first
    // node in the cluster to startup.  Otherwise call with an address
    // of any node within the cluster.
    // (otherwise we don't know how to find the cluster).
    virtual void start(const Address &address) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

    // Performs the action to join a group (sending a JoinRequest message).
    void joinGroup(const Address& address);

    // Handlers for the individual messages
    void onAppendEntries(const Address& from, istringstream& ss);
    void onRequestVote(const Address& from, istringstream& ss);
    void onAddServer(const Address& from, istringstream& ss);
    void onAddServerReply(const Address& from, istringstream& ss);

    // Handlers for command messages
    // i.e. those messages received via JSON command messages
    void onAddServerCommand(shared_ptr<CommandMessage> command, const Address& address);
    void onRemoveServerCommand(shared_ptr<CommandMessage> command, const Address& address);

    // Transaction apis
    void openTransaction(int transId,
                         int timeout,
                         const Address& to,
                         shared_ptr<Message> message);
    void closeTransaction(int transId);

    // State manipulation APIs
    void applyLogEntry(const RaftLogEntry& entry);

protected:
    Log *                   log;
    Params *                par;
    ContextStoreInterface * store;
    weak_ptr<NetworkNode>   netnode;
    shared_ptr<IConnection> connection;
    int                     electionTimeout;
    int                     heartbeatTimeout;

    // This can be built from the log by executing all
    // of the log entries.
    vector<Address>         addresses;

    // The time to send out the next heartbeat, if set to 0
    // then this will never trigger.
    int                     nextHeartbeat;

    // List of currently open transactions
    //$ TODO: maybe keep sorted based on timeout?
    map<int, Transaction>   transaction;
};

}

#endif /* NCLOUD_RAFT_H */

