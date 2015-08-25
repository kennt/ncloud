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
    INSTALL_SNAPSHOT,
    INSTALL_SNAPSHOT_REPLY
};

// ==================
// Messages
// ==================

class Message
{
public:
    MessageType     msgtype;
    int             transId;
    TERM            term;

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
    virtual unique_ptr<RawMessage> toRawMessage(const Address &from,
                                                const Address &to) = 0; 

    static MessageType getMessageType(const byte *data, size_t size);

    // Helper methods for writing/reading to binary
    void write(stringstream& ss);
    void read(istringstream& ss);
};

// Use this class to read in only the message header
// Used to simplify parts of the code that need to check only the 
// header before taking action.
class HeaderOnlyMessage : public Message
{
public:
    HeaderOnlyMessage() : Message(MessageType::MSGTYPE_NONE) {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;
};

class AppendEntriesMessage : public Message
{
public:
    // Implied MessageType::APPEND_ENTRIES

    // so follower can redirect clients
    Address     leaderAddress;

    // index of log entry immediately preceding new ones
    INDEX       prevLogIndex;

    // term of prevLogIndex entry
    TERM        prevLogTerm;

    // Log entries to store (empty for heartbeat, may send
    // more than one for efficiency)
    vector<RaftLogEntry>  entries;

    // leader's commit index
    INDEX       leaderCommit;

    AppendEntriesMessage() : Message(APPEND_ENTRIES),
        prevLogIndex(0), prevLogTerm(0), leaderCommit(0)
    {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;
};

class AppendEntriesReply : public Message
{
public:
    // Implied MessageType::APPEND_ENTRIES_REPLY

    // true if follower contained entry matching prevLogIndex and prevLogTerm
    bool        success;

    AppendEntriesReply() : Message(APPEND_ENTRIES_REPLY), success(false)
    {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;
};

class RequestVoteMessage : public Message
{
public:
    // Implied MessageType::REQUEST_VOTE

    // candidate requesting vote
    Address     candidate;

    // index of candidate's last log entry
    INDEX       lastLogIndex;

    // term of candidate's last log entry
    TERM        lastLogTerm;

    
    RequestVoteMessage() : Message(REQUEST_VOTE), lastLogIndex(0), lastLogTerm(0)
    {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;
};

class RequestVoteReply : public Message
{
public:
    // Implied MessageType::REQUEST_VOTE_REPLY

    // true means the candidate received the vote
    bool        voteGranted;

    RequestVoteReply() : Message(REQUEST_VOTE_REPLY), voteGranted(false)
    {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;
};

class InstallSnapshotMessage : public Message
{
public:
    // Implied MessageType::INSTALL_SNAPSHOT

    // so the follower can redirect clients
    Address     leaderAddress;

    // the snapshot replaces all entries up through and including
    // this index
    INDEX       lastIndex;

    // Term of the lastIndex
    TERM        lastTerm;

    // For us, the config is the same as the snapshot
    // Starting position for the address vector (since we may be
    // sending a subset of addresses at a time).
    unsigned int offset;
    vector<Address> addresses;

    // true if this is the last chunk
    bool        done;

    InstallSnapshotMessage() : Message(INSTALL_SNAPSHOT),
        lastIndex(0), lastTerm(0), offset(0), done(false)
    {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;
};

class InstallSnapshotReply : public Message
{
public:
    // Implied MessageType::INSTALL_SNAPSHOT_REPLY

    InstallSnapshotReply() : Message(INSTALL_SNAPSHOT_REPLY)
    {}

    virtual void load(const RawMessage *raw) override;
    virtual unique_ptr<RawMessage> toRawMessage(const Address& from,
                                                const Address& to) override;    
};

// Transactions are used to track RPCs and their replies.  This are
// started and then checked periodically within the onTimeout() call.
//
// The base transaction class (below), supports basic timeout handling.
// In Raft, RPCs are long-lived and can stay alive indefinitely.
//
class Transaction : public std::enable_shared_from_this<Transaction>
{
public:
    // These are the return values from the callbacks.  DELETE means
    // that the system will remove the transaction from the active
    // list.
    enum class RESULT { KEEP=0, DELETE };

    // These are special indexes. These transactions are special in
    // that they always exist (though may not be active).
    enum SPECIALINDEX { ELECTION = -10, HEARTBEAT = -9 };

    Log *       log;
    Params *    par;

    // This is ususally the message transId, but may be
    // set to a negative value (for non-message transactions).
    int         transId;
    TERM        term;

    RaftHandler *    handler;

    Transaction(Log *log, Params *par, RaftHandler *handler)
        : log(log), par(par), transId(0), term(0), handler(handler),
        timeStart(-1), timeout(0), nTimeouts(1), lifetime(0)
    {}

    virtual ~Transaction() { close(); }

    // Call this to get the transaction started (usually sends off some
    // kind of message).
    virtual void start() = 0;

    // Forces the shutdown of the transaction (doesn't matter if completed
    // or not).
    virtual void close();

    // A reply has been received with this transid
    virtual Transaction::RESULT onReply(const RawMessage *raw) = 0;

    // A timeout has occurred
    virtual Transaction::RESULT onTimeout() = 0;

    // Starts the timeout handling
    void startTimeout(int interval)
        { timeStart = par->getCurrtime(); timeout = interval; nTimeouts = 1; }

    // Advance to the next timeout
    void advanceTimeout()
        { nTimeouts++; }

    void resetTimeout(int newStart)
        { timeStart = newStart; nTimeouts = 1; }

    // Stops the timeout handling (the callbacks will not be invoked).
    void stopTimeout() { timeStart = -1; }

    bool isTimedOut(int currtime)
    {
        if (timeStart == -1)
            return false;
        return currtime >= (timeStart + (nTimeouts * timeout));
    }

    // This is an upper bound on the lifetime of a transaction.
    // After this a transaction should request deletion.
    void setLifetime(int lifetime)
    {
        this->lifetime = par->getCurrtime() + lifetime;
    }

    // This function will be called upon completion of the transaction.
    // "Completion" usually means that a decision point has been reached.
    // So for elections, it will be called when a majority of replies has
    // been received (not necessarily when ALL of the replies have been
    // received).  It will also be called when the lifttime limit has been
    // reached.
    //
    // The parent transaction pointer is used when the onCompleted
    // function is a member of a parent transaction. This is to ensure that
    // the object is alive while the callback is in place.
    std::function<void (Transaction *trans, bool success)> onCompleted;
    shared_ptr<Transaction> parent;

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

    // Upper bound on the lifetime of a transaction
    // Setting this to 0 turns off the lifetime check (infinite lifetime)
    int         lifetime;

    bool        isLifetime()
        { return this->lifetime && par->getCurrtime() > this->lifetime; }

    // This should be called when the Transaction has "completed", or
    // come to a decision point (not necessarily fully completed).
    Transaction::RESULT completed(bool success)
    {
        if (this->onCompleted != nullptr)
            this->onCompleted(this, success);
        close();
        return Transaction::RESULT::DELETE;
    }

    shared_ptr<NetworkNode> getNetworkNode();
};

// Use this transaction to handle election timeouts.
// When this class times out, it means that it can start
// an election (if a follower)
class ElectionTimeoutTransaction : public Transaction
{
public:
    ElectionTimeoutTransaction(Log *log, Params *par, RaftHandler *handler)
        : Transaction(log, par, handler)
    {}

    virtual void start() override;
    virtual Transaction::RESULT onReply(const RawMessage *raw) override;
    virtual Transaction::RESULT onTimeout() override;

protected:
};

// Use this transaction to handle heartbeat timeotus
// This will wake up every now and then and send out an AppendEntries
// to every node (basically runs a GroupUpdateTransaction) if the
// node is a leader.
class HeartbeatTimeoutTransaction : public Transaction
{
public:
    HeartbeatTimeoutTransaction(Log *log, Params *par, RaftHandler *handler)
        : Transaction(log, par, handler)
    {}

    virtual void start() override;
    virtual Transaction::RESULT onReply(const RawMessage *raw) override;
    virtual Transaction::RESULT onTimeout() override;

protected:
    void sendGroupHeartbeat();
};

// Use this to monitor the vote for a SINGLE follower.
// For voting, use a GroupElectionTransaction to record the
// results.
// This transaction completes when a majority result is determined.
// Note that the voting list is copied into the transaction, so 
// later changes do not affect the voting result.
class ElectionTransaction : public Transaction
{
public:
    ElectionTransaction(Log *log, Params *par, RaftHandler *handler)
        : Transaction(log, par, handler),
        yesVotes(0), noVotes(0), totalVotes(0)
    {}

    void init(const MemberInfo &member);

    virtual void start() override;
    virtual Transaction::RESULT onReply(const RawMessage *raw) override;
    virtual Transaction::RESULT onTimeout() override;

protected:
    unsigned int    yesVotes;
    unsigned int    noVotes;
    unsigned int    totalVotes;
    set<Address> voted;

    vector<Address> recipients;

    // Returns true if we have received a majority of replies
    // (either success or failure)
    bool isMajority()
        { return (2*yesVotes > totalVotes) || (2*noVotes > totalVotes); }

};

// Use this transaction to update a SINGLE follower. If updating a
// group of servers, use the GroupUpdateTransaction as a parent
// transaction.
// Use this transaction for bringing followers up-to-date with
// the leader. This transaction is not normally used for heartbeats.
class UpdateTransaction : public Transaction
{
public:
    UpdateTransaction(Log *log, Params *par, RaftHandler *handler)
        : Transaction(log, par, handler),
        lastLogIndex(0), lastLogTerm(0),
        lastSentLogIndex(0), lastSentLogTerm(0), offset(0)
    {}

    void init(const Address& address, INDEX lastIndex, TERM lastTerm);

    virtual void start() override;
    virtual Transaction::RESULT onReply(const RawMessage *raw) override;
    virtual Transaction::RESULT onTimeout() override;

    const Address& address() { return recipient; }

protected:
    Address     recipient;

    // We are trying to catch up the logs to these values.
    INDEX       lastLogIndex;
    TERM        lastLogTerm;

    // The values that were in the last appendEntries request
    INDEX       lastSentLogIndex;
    TERM        lastSentLogTerm;

    // snapshot support
    // The beginning index of the next set of data to send
    unsigned int offset;
    shared_ptr<Snapshot> snapshot;

    // Sends an update (may be an AppendEntries or an InstallSnapshot)
    // Passing in force=true will force an AppendEntries with the given
    // index.
    void        sendAppendEntriesRequest(INDEX index);

    Transaction::RESULT     onAppendEntriesReply(const RawMessage *raw);
    Transaction::RESULT     onInstallSnapshotReply(const RawMessage *raw);
};


// Use this to accumulate the results of multiple actions.
// This will "complete" when a quorum (or majority) of actions
// register as completed.  It will remove itself only after
// the timeout is reached or all servers have registered completion.
//
// This is for following the updates of multiple servers.
//
class GroupUpdateTransaction : public Transaction
{
public:
    GroupUpdateTransaction(Log *log, Params *par, RaftHandler *handler)
        : Transaction(log, par, handler),
        lastLogIndex(0), lastLogTerm(0),
        totalVotes(0), successVotes(0), failureVotes(0)
    {}

    void init(const MemberInfo& member, INDEX lastIndex, TERM lastTerm);
    void init(const vector<Address> &members, INDEX lastIndex, TERM lastTerm);

    virtual void start() override;
    virtual Transaction::RESULT onReply(const RawMessage *raw) override;
    virtual Transaction::RESULT onTimeout() override;

protected:
    INDEX   lastLogIndex;
    TERM    lastLogTerm;

    // Recipients that have said they were completed
    // (keeps track of nodes that have replied)
    set<Address> replied;

    unsigned int totalVotes;
    unsigned int successVotes;
    unsigned int failureVotes;

    // Full set of recipients
    vector<Address> recipients;

    // Returns true if we have received a majority of either
    // yes or no replies.
    bool        isMajority()
        { return (2*successVotes > totalVotes) || (2*failureVotes > totalVotes); }

    // This is called by the child transactions when they have completed
    // their operations.
    void onChildCompleted(Transaction *childTrans, bool success);
};

// Use this when Adding/Removing servers from the cluster membership.
// This class will take care of coordinating the various steps needed
// for this operation:
//  (1) Catch the new server up to the previous configuration
//  (2) Wait until the previous config is committed
//  (3) Append the new entry, commit
//  (4) Reply ok
class MemberChangeTransaction : public Transaction
{
public:
    MemberChangeTransaction(Log *log, Params *par, RaftHandler *handler)
        : Transaction(log, par, handler),
        lastLogTerm(0), lastLogIndex(0)
    {}

    // The address of the server being added/deleted
    Address     server;
    Command     command;
    shared_ptr<CommandMessage> commandMessage;

    // We are trying to catch up the logs to these values.
    TERM        lastLogTerm;
    INDEX       lastLogIndex;

    void init(const MemberInfo& member);

    // Starts the operation, sends the initial AppendEntries
    // message and initializes the timeouts.
    virtual void start() override;
    virtual void close() override;
    virtual Transaction::RESULT onReply(const RawMessage *raw) override;
    virtual Transaction::RESULT onTimeout() override;

protected:
    vector<Address>     recipients;

    // The current operation begin performed (depends on
    // where we are in the process). This is a weak_ptr because
    // the child transaction will have a shared_ptr on this.
    weak_ptr<Transaction>   currentTrans;

    // This gets called upon completion of the first step, the
    // update of the target server (this step is not needed when removing).
    void onServerUpdateCompleted(Transaction *trans, bool success);

    // This gets called upon completion of the second step, the
    // committing of the previous configuration (w/o the current command).
    void onPrevConfigCompleted(Transaction *trans, bool success);

    // This gets called upon completion of the third step, the
    // commiting of the newest command (completion of this step is completion
    // of the update). This will send the command reply.
    void onCommandUpdateCompleted(Transaction *trans, bool success);
};

// The MessageHandler for the Raft protocol.  This replaces the normal
// message-handling protocol for Group Membership.
//
class RaftHandler: public IMessageHandler
{
public:
    // The log, par, and store are direct pointers, and the
    // objects are expected to live beyond the lifetime
    // of the MessageHandlers.
    // Since these belong to the system, this is a reliable
    // assumption.
    //
    RaftHandler(Log *log, 
                Params *par,
                StorageInterface *store,
                StorageInterface *snapshotStore,
                shared_ptr<NetworkNode> netnode,
                shared_ptr<IConnection> connection)
        : log(log), par(par), store(store), snapshotStore(snapshotStore),
        netnode(netnode), connection_(connection), lastUpdate(0),
        electionTimeoutModifier(0), nextMessageId(0)
    {
    };

    virtual ~RaftHandler() { transactions.clear(); };

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
    //
    virtual void start(const Address &leader) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

    // Handlers for the individual messages
    void onAppendEntries(const Address& from, const RawMessage *raw);
    void onRequestVote(const Address& from, const RawMessage *raw);
    void onInstallSnapshot(const Address &from, const RawMessage *raw);

    // Replies are all handled here.  This will dispatch the messages
    // to their respective transactions.
    void onReply(const Address& from,
                 const HeaderOnlyMessage& header,
                 const RawMessage *raw);

    // Handlers for command messages
    // i.e. those messages received via JSON command messages
    // The parameters are the command and the address to be changed.
    void onChangeServerCommand(shared_ptr<CommandMessage> message,
                               Command command,
                               const Address& address);

    // Transaction support
    shared_ptr<Transaction> findTransaction(int transid);
    void addTransaction(shared_ptr<Transaction> trans);
    void removeTransaction(int transid);

    // Create the timeout transactions, these are initially 
    // deactivated (timeout == 0). To start processing,
    // set the timeStart and timeout values.
    void initTimeoutTransactions();

    
    // State manipulation APIs
    // This will not add the entry to the log, it applies the
    // change to the "state machine".  To maintain consistency,
    // all changes in the state machine (the membership list)
    // should go through here.
    void applyLogEntry(Command command, const Address& address);
    void applyLogEntry(const RaftLogEntry& entry);
    void applyLogEntries(const vector<RaftLogEntry>& entries);

    // Returns true if the request (represented by the parameters)
    // is more current than our log.
    bool isLogCurrent(INDEX index, TERM term);

    int getNextMessageId() { return ++nextMessageId; }
    Address address() { return connection_->address(); }
    shared_ptr<IConnection> connection() { return connection_; }

    // Be careful with this API as this will keep a refcount on the
    // NetworkNode!
    shared_ptr<NetworkNode> node() { return getNetworkNode(); }

    // Broadcasts a message to all members (skips self).
    void broadcast(Message *message);
    void broadcast(Message *message, vector<Address> & recipients);

    // Transaction callbacks
    void onCompletedElection(Transaction *trans, bool success);
    void onCompletedMemberChange(Transaction *trans, bool success);

    int  getElectionTimeout()
        { return par->electionTimeout + this->electionTimeoutModifier; }
    void setElectionTimeoutModifier(int tm)
        { this->electionTimeoutModifier = tm; }

protected:
    Log *                   log;
    Params *                par;
    StorageInterface *      store;
    StorageInterface *      snapshotStore;
    weak_ptr<NetworkNode>   netnode;
    shared_ptr<IConnection> connection_;
    int                     lastUpdate;
    int                     electionTimeoutModifier;

    // This can be built from the log by executing all
    // of the log entries.
    vector<Address>         addresses;

    // List of currently open transactions
    map<int, shared_ptr<Transaction>>   transactions;

    // Unique id used to track messages/transactions.
    // Use getNextMessageId() to access this variable. That
    // API will increment nextMessageId.
    int                     nextMessageId;

    // Hwlpful pointers to common timeouts
    // This is for tracking election timeouts!  not voting elections!
    shared_ptr<ElectionTimeoutTransaction> election;
    shared_ptr<HeartbeatTimeoutTransaction> heartbeat;

    // If this is not nullptr, then there is an Add/RemoveServer
    // operation going on (only one is allowed at a time).
    shared_ptr<MemberChangeTransaction>  memberchange;

    shared_ptr<NetworkNode> getNetworkNode()
    {
        auto node = netnode.lock();
        if (!node)
            throw AppException("network not available");
        return node;
    }
};

}

#endif /* NCLOUD_RAFT_H */

