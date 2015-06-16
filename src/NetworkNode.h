/*****
 * NetworkNode.h
 *
 * See LICENSE for details.
 *
 * Contains the central classes to implement the protocol and a place to store
 * the membership information.
 *
 * IMessageHandler
 *	An interface to handle messages that arrive on a certain connection.
 *
 * NetworkNode
 *	A placeholder to hold all connections/handlers/info for a particular
 *	network node (endpoint).
 *
 *****/

#ifndef NCLOUD_NETWORKNODE_H
#define NCLOUD_NETWORKNODE_H

#include "stdincludes.h"	
#include "Params.h"
#include "Network.h"
#include "Member.h"

// Abstract interface for the message callbacks.  This is called by
// the network code when a message is received or during an idle period.
//
class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}

	// Initializes the MessageHandler.
	virtual void start(const Address& address) = 0;

	// Called when a message has been received for this address.
	virtual void onMessageReceived(const RawMessage *message) = 0;

	// Called when "idle".  This is called after all queued messages
	// have been processed (SimNetwork and SocketNetwork) and 
	// also when the recv() call has timed out (SocketNetwork).
	virtual void onTimeout() = 0;
};


// This class represents a single network node (or a process).
// This everything that belongs to one machine (or process)
// should be here (where they information can be shared).
//
class NetworkNode
{
public:
	// Describes what the connection is for
	// MP1 : MEMBER
	// MP2 : RING
	enum class ConnectionType { MEMBER, RING };

	// log and par are both owned at the application level and thus
	// will be valid for the lifetime of this object.
	//
	NetworkNode(string name, Log *log, Params *par, shared_ptr<INetwork> network);

	// Registers the handler to handle messages received by thie connection.
	// The conntype is used to identify the connection/handler so that 
	// the application can lookup a particular connection.
	// 
	// The connection/handlers are kept in a map based on the 
	// connection's address (thus the same connection cannot be used twice).
	void registerHandler(ConnectionType conntype,
						 shared_ptr<IConnection> connection,
						 shared_ptr<IMessageHandler> handler);
	// Removes the connection/handler associated with the 
	void unregisterHandler(const Address &address);


	// Initializes the node (prepares the node for running)
	void nodeStart(const Address &joinAddress, int timeout);

	// This looks for messages by calling recv() on each
	// connection.  Pull messages off of the net and places them onto
	// an internal queue.
	void receiveMessages();

	// Takes messages off the queue and then calls the associated MessageHandler
	// to process the messages.  And then calls the onTimeout() on the MessageHandler
	// after all of the queued messages have been processed.
	void processQueuedMessages();

	string getName()	{ return this->name; }

	// Returns a list of addresses in use by this node.
	vector<Address> getAddresses();

	// Returns a connection for the given conntype.  In the presence of
	// multiple entries with the same type, the return value is not defined.
	shared_ptr<IConnection> getConnection(ConnectionType conntype);

	// Has the node failed?  Failure means that the node
	// no longer sends/receives messages across all connections.
	bool 		failed() const { return hasFailed; }

	// Fail this node (it will no longer send/receive messages).
	void 		fail();

	// MP1 stuff
	MemberInfo 	member;

	// MP2 stuff
	//Node 		node;

protected:

	struct HandlerInfo
	{
		ConnectionType 				conntype;
		shared_ptr<IConnection> 	connection;
		shared_ptr<IMessageHandler>	handler;
		shared_ptr<list<unique_ptr<RawMessage>>> queue;
	};

	bool 				hasFailed;
	string 				name;	
	map<Address, HandlerInfo> handlers;
	Log *				log;
	Params *			par;
	weak_ptr<INetwork>	network;
	int 				timeout;
};

#endif /* NCLOUD_NETWORKNODE_H */
