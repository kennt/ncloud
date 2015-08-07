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
    virtual void load(const RawMessage *raw) = 0;

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

    virtual void load(const RawMessage *raw) override;
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
    // Thus we are overwriting/appending
    //  for (int i=0; i<entries.size(); i++)
    //      log[prevLogIndex+1+i] = entries[i]
    int         prevLogIndex;

    // term of prevLogIndex entry
    int         prevLogTerm;

    // Log entries to store (empty for heartbeat, may send
    // more than one for efficiency)
    vector<RaftLogEntry>  entries;

    // leader's commit index
    int         leaderCommit;

    AppendEntriesMessage() : Message(APPEND_ENTRIES),
        prevLogIndex(0), prevLogTerm(0), leaderCommit(0)
    {}

    virtual void load(const RawMessage *raw) override;
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

    virtual void load(const RawMessage *raw) override;
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

    virtual void load(const RawMessage *raw) override;
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

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from, const Address& to) override;
};


// Transactions are used to track RPCs and their replies.  This are
// started and then checked periodically within the onTimeout() call.
//
// In Raft, RPCs are long-lived and can stay alive indefinitely.
//
// These transactions may also be used for timer functions.
//
// The onTimeout() callback is only called when the timeout is non-zero.
//
class Transaction
{
public:
    enum class RESULT { KEEP=0, DELETE };
    enum INDEX { ELECTION = -10, HEARTBEAT = -9 };

    Log *       log;

    // This is ususally the message transId, but may be
    // set to a negative value (for non-message transactions).
    int         transId;
    int         term;

    // The original message and command that caused this message
    // Note that both may be set (a client command that causes
    // a message to be sent).  Either (or both) may be set to nullptr.
    shared_ptr<Message> original;
    shared_ptr<Command> command;

    // Recipient addresses (for non-response). This collection
    // may be modified from a callback. One use case is for 
    // RPCs that need to resend requests
    vector<Address> recipients;

    weak_ptr<NetworkNode>   netnode;

    Transaction(Log *log, Params *par, shared_ptr<NetworkNode> node) 
        : log(log), transId(0), term(0), netnode(node),
        timeStart(-1), timeout(0), nTimeouts(1)
    {
        timeStart = par->getCurrtime();
    }

    virtual ~Transaction() {}

    // Starts the timeout handling
    void start(int start, int interval)
        { timeStart = start; timeout = interval; nTimeouts = 1; }

    // Advance to the next timeout
    void advanceTimeout()
        { nTimeouts++; }

    void reset(int newStart)
        { timeStart = newStart; nTimeouts = 1; }

    // Stops the timeout handling (the callbacks will not be invoked).
    void stop() { timeStart = -1; }

    bool isTimedOut(int currtime)
    {
        if (timeStart == -1)
            return false;
        return currtime >= (timeStart + (nTimeouts * timeout));
    }

    // Returns RESULT::DELETE to remove this transaction
    // Returns RESULT::KEEP otherwise
    std::function<RESULT (Transaction *thisTrans)>   onTimeout;
    std::function<RESULT (Transaction *thisTrans, const RawMessage *raw)>   onReceived;

protected:
    // The timeout callback will be called when
    //      current_time >= (timeStart + timeout*nTimeouts)
    // Note that this will be called whenever this condition
    // is true.  To disable, set timeStart to -1.
    int         timeStart;
    int         timeout;
    // The multiplier used for timeouts (starts at 1).
    // Incremented each time a timeout occurs.
    int         nTimeouts;

};

// Use this for keeping track of election results.
// This transaction completes when a majority result is determined.
// Note that the voting list is copied into the transaction, so 
// later changes do not affect the voting result.
class ElectionTransaction : public Transaction
{
public:
    // Used for calculating majority voting
    // Setting total to 0 disables election tracking
    // (and preventing the node from moving from candidate to leader)
    int         total;
    int         successes;
    int         failures;
    void        cancelElection()
        { total = 0; }

    ElectionTransaction(Log *log, Params *par, shared_ptr<NetworkNode> node)
        : Transaction(log, par, node),
        total(0), successes(0), failures(0)
    {}
};

// Use this transaction for bringing followers up-to-date with
// the leader. This transaction is not normally used for heartbeats.
class UpdateTransaction : public Transaction
{
public:
    UpdateTransaction(Log *log, Params *par, shared_ptr<NetworkNode> node)
        : Transaction(log, par, node),
        lastLogTerm(0), lastLogIndex(0)
    {}

    // The set of addresses that have caught up to lastLogIndex.
    set<Address>        satisfied;

    // For each recipient, this is the index where we match up.
    // The goal is to get a majority to be up to lastLogIndex
    map<Address, int>   matchIndexes;

    // We are trying to catch up the logs to these values.
    int     lastLogTerm;
    int     lastLogIndex;

    // Returns true if a majority of nodes caught up to lastLogIndex.
    // This requires that satisfied.size()*2 > recipients.size()
    bool    isCompleted()
        { return 2*satisfied.size() > recipients.size(); }

    // Called when the update has been committed (have to match up
    // to lastLogTerm and lastLogIndex).  This will be called only
    // once per transaction (unless manually set again).
    std::function<void (Transaction *thisTrans)>   onCompleted;
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
            connection_(connection), nextMessageId(0)
    {
    };

    virtual ~RaftMessageHandler() {};

    // Initializes the MessageHandler, if needed. This will be called
    // before onMessageReceived() or onTimeout() will be called.
    //
    // For Raft, the address parameter is the "initial" leader.
    // The machine with this address will initialize its log
    // and start an election immediately, all other servers will
    // follow the normal startup path.
    //
    // Setting the address to the leader is meant for initial
    // cluster startup.  After that, a zero address should be
    // passed in.
    virtual void start(const Address &leader) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

    // Handlers for the individual messages
    void onAppendEntries(const Address& from, const RawMessage *raw);
    void onRequestVote(const Address& from, const RawMessage *raw);

    // Replies are all handled here.  This is a dispatcher which
    // calls the separate onXXXReply() callbakcs and also cleans
    // up the transactions.
    void onReply(const Address& from,
                 const HeaderOnlyMessage& header,
                 const RawMessage *raw);

    // Handlers for command messages
    // i.e. those messages received via JSON command messages
    void onAddServerCommand(shared_ptr<CommandMessage> command, const Address& address);
    void onRemoveServerCommand(shared_ptr<CommandMessage> command, const Address& address);

    // Transaction support
    shared_ptr<Transaction> findTransaction(int transid);    

    // Create the timeout transactions, these are initially 
    // deactivated (timeout == 0). To start processing,
    // set the timeStart and timeout values.
    void initTimeoutTransactions();
    Transaction::RESULT transOnElectionTimeout(Transaction *trans);
    Transaction::RESULT transOnHeartbeatTimeout(Transaction *trans);

    Transaction::RESULT transOnUpdateMessage(Transaction *trans, const RawMessage *raw);
    Transaction::RESULT transOnUpdateTimeout(Transaction *trans);

    // The OnCompleted function for Add/RemoverServer functions.
    // This will go onto the next step of the Add/RemoveServer process.
    void transChangeServerOnCompleted(Transaction *trans);

    
    // State manipulation APIs
    // This will not add the entry to the log, it applies the
    // change to the "state machine".  To maintain consistency,
    // all changes in the state machine (the membership list)
    // should go through here.
    void applyLogEntry(Command command, const Address& address);
    void applyLogEntry(const RaftLogEntry& entry);
    void applyLogEntries(const vector<RaftLogEntry>& entries);

    // Returns true if the log is up-to-date
    // i.e. the last entry of the log is equal to term
    // or in this case log.size()-1 == index and log[index] == term
    // (subtract 1 from the size due to the 0th entry)
    bool isLogCurrent(int index, int term);

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

    // List of currently open transactions
    map<int, shared_ptr<Transaction>>   transactions;

    // Unique id used to track messages/transactions
    int                     nextMessageId;

    // Hwlpful pointers to common transactions
    shared_ptr<ElectionTransaction> election;
    shared_ptr<Transaction> heartbeat;

    // If this is not nullptr, then there is an Add/RemoveServer
    // operation going on (only one is allowed at a time).
    shared_ptr<Transaction>  addremove;

    // Returns true if there is an UpdateTransaction in the
    // list of transactions.
    bool isUpdateTransactionInProgress();
};

}

#endif /* NCLOUD_RAFT_H */

