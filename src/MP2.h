/*****
 * MP2.h
 *
 * See LICENSE for details.
 *
 * Holds the classes needed to receive and handle messages for the 
 * Membership protocol.
 *
 * JoinRequestMessage represents a JoinRequest message.
 * MP1MessageHandler is called when a message is available (or if there
 * is a timeout, i.e. on idle).
 *
 *****/


#ifndef NCLOUD_MP2_H
#define NCLOUD_MP2_H

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Network.h"
#include "Ring.h"
#include "NetworkNode.h"

class NetworkNode;

struct Message
{
public:
    static shared_ptr<Message> Create(int transid, string key, string value, ReplicaType replica=ReplicaType::REPLNONE);
    static shared_ptr<Message> Read(int transid, string key);
    static shared_ptr<Message> Update(int transid, string key, string value, ReplicaType replcia=ReplicaType::REPLNONE);
    static shared_ptr<Message> Delete(int transid, string key);
    static shared_ptr<Message> Reply(int transid, bool success);
    static shared_ptr<Message> ReadReply(int _transid, string value);

    void load(istringstream& ss);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);

    RingMessageType type;
    ReplicaType     replica;
    string          key;
    string          value;
    int             transid;
    bool            success;
    Address         to;
    Address         from;
};


// See comment above
//
class MP2MessageHandler: public IMessageHandler
{
public:
    MP2MessageHandler(Log *log, 
                      Params *par,
                      shared_ptr<NetworkNode> netnode,
                      shared_ptr<IConnection> connection)
        : log(log), par(par), netnode(netnode), connection(connection)
    {
    }

    virtual ~MP2MessageHandler() {}

    // Initializes the MessageHandler, if needed. This will be called
    // before onMessageReceived() or onTimeout() will be called.
    virtual void start(const Address &address) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

protected:
    Log *                   log;
    Params *                par;
    weak_ptr<NetworkNode>   netnode;
    shared_ptr<IConnection> connection;

    // Server-side DB apis
    bool createKeyValue(string key, string value, ReplicaType replica);
    string readKey(string key);
    bool updateKeyValue(string key, string value, ReplicaType replica);
    bool deleteKey(string key);
};


#endif /* NCLOUD_MP2_H */

