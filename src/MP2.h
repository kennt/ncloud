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
#include "json/json.h"
#include "NetworkNode.h"



enum RingMessageType { RINGNONE=0, CREATE, READ, UPDATE, DELETE, REPLY, READREPLY };
enum ReplicaType { REPLNONE=-1, PRIMARY=0, SECONDARY, TERTIARY };

class NetworkNode;


// See comment above.
//
struct CreateMessage
{
	// Implied RingMessageType::CREATE
	string 	key;
	string	value;
	ReplicaType replica;

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct ReadMessage
{
	// Implied RingMessageType::READ
	string 	key;

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct UpdateMessage
{
	// Implied RingMessageType::UPDATE
	string 	key;
	string 	value;
	ReplicaType replica;

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct DeleteMessage
{
	// Implied RingMessageType::DELETE
	string 	key;

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct ReplyMessage
{
	// Implied RingMessageType::REPLY
	bool 	success;

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};

struct ReadReplyMessage
{
	// Implied RingMessageType::READREPLY
	string 	value;

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
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
	Log *					log;
	Params *				par;
	weak_ptr<NetworkNode>	netnode;
	shared_ptr<IConnection>	connection;
};


#endif /* NCLOUD_MP2_H */

