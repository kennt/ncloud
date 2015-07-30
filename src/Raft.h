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
    REQUEST_VOTE_REPLY
};

// ==================
// Messages
// ==================

class Message
{
public:
    MessageType     msgtype;
    int             transId;
    int             term;

    Message(MessageType mt) : msgtype(mt), transId(0), term(0) {}

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

    static MessageType getMessageType(const byte *data, size_t size);

    // Helper methods for writing/reading to binary
    void write(stringstream& ss);
    void read(istringstream& ss);
};

// Use this class to read in only the message header
// Used to simplify parts of the code that need to check the 
// header before taking action.
class HeaderOnlyMessage : public Message
{
public:
    HeaderOnlyMessage() : Message(MessageType::MSGTYPE_NONE) {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class AppendEntriesMessage : public Message
{
public:
    // Implied MessageType::APPEND_ENTRIES

    // leader's term
    // int         term;

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

    AppendEntriesMessage() : Message(APPEND_ENTRIES), prevLogIndex(0), prevLogTerm(0), leaderCommit(0)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class AppendEntriesReply : public Message
{
public:
    // Implied MessageType::APPEND_ENTRIES_REPLY

    // currentTerm, for the leader to update itself
    // int         term;

    // true if follower contained entry matching prevLogIndex and prevLogTerm
    bool        success;

    AppendEntriesReply() : Message(APPEND_ENTRIES_REPLY), success(false)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class RequestVoteMessage : public Message
{
public:
    // Implied MessageType::REQUEST_VOTE

    // candidate's term
    // int         term;

    // candidate requesting vote
    Address     candidate;

    // index of candidate's last log entry
    int         lastLogIndex;

    // term of candidate's last log entry
    int         lastLogTerm;

    
    RequestVoteMessage() : Message(REQUEST_VOTE), lastLogIndex(0), lastLogTerm(0)
    {}

    virtual void load(istringstream& ss) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};

class RequestVoteReply : public Message
{
public:
    // Implied MessageType::REQUEST_VOTE_REPLY

    // currentTerm, for candidate to update itself
    // int         term;

    // true means the candidate received the vote
    bool        voteGranted;

    RequestVoteReply() : Message(REQUEST_VOTE_REPLY), voteGranted(false)
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

    // So this transaction expires on timeSent + timeout
    int         timeSent;
    int         timeout;

    // The original message that was sent
    Address     toAddress;
    shared_ptr<Message> message;

    Transaction() : transId(0), timeSent(0), timeout(0) {}
    Transaction(int transid, int timesent, int timeout, const Address&to,
            shared_ptr<Message> message)
        : transId(transid), timeSent(timesent), timeout(timeout),
        toAddress(to), message(message)
    {}
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
        : log(log), par(par), store(store), netnode(netnode),
            connection_(connection), nextHeartbeat(0), nextMessageId(0)
    {
    };

    virtual ~RaftMessageHandler() {};

    // Initializes the MessageHandler, if needed. This will be called
    // before onMessageReceived() or onTimeout() will be called.
    //
    // For Raft, the address parameter is not used.
    virtual void start(const Address &unused) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

    // Handlers for the individual messages
    void onAppendEntries(const Address& from, istringstream& ss);
    void onRequestVote(const Address& from, istringstream& ss);

    // Replies are all handled here.  This is a dispatcher which
    // calls the separate onXXXReply() callbakcs and also cleans
    // up the transactions.
    void onReply(const Address& from, MessageType mt, istringstream& ss);

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
    // This will not add the entry to the log, it applies the
    // change to the "state machine"
    void applyLogEntry(Command command, const Address& address);
    void applyLogEntry(const RaftLogEntry& entry);

    // Returns true if the log is up-to-date
    // i.e. the last entry of the log is equal to term
    // or in this case log.size()-1 == index and log[index] == term
    // (subtract 1 from the size due to the 0th entry)
    bool isLogCurrent(int index, int term);

    void resetTimeouts(int timeouts);

    int getNextMessageId() { return ++nextMessageId; }
    Address address() { return connection_->address(); }
    shared_ptr<IConnection> connection() { return connection_; }

    // Broadcasts a message to all members
    // (skips self).
    void broadcast(Message *message);

protected:
    Log *                   log;
    Params *                par;
    ContextStoreInterface * store;
    weak_ptr<NetworkNode>   netnode;
    shared_ptr<IConnection> connection_;

    // This can be built from the log by executing all
    // of the log entries.
    vector<Address>         addresses;

    // The time to send out the next heartbeat, if set to 0
    // then this will never trigger.
    int                     nextHeartbeat;

    // List of currently open transactions
    //$ TODO: maybe keep sorted based on timeout?
    map<int, Transaction>   transactions;

    // Unique id used to track messages/transactions
    int                     nextMessageId;

    // Specific reply-handlers
    void onAppendEntriesReply(const Address& from, istringstream& ss);
    void onRequestVoteReply(const Address& from, istringstream& ss);
};

}

#endif /* NCLOUD_RAFT_H */

