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
#include "Ring.h"

class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}

	virtual void start(const Address& address) = 0;

	virtual void onMessageReceived(const RawMessage *message) = 0;
	virtual void onTimeout() = 0;
};

// Describes what the connection is for
// MP1 : MEMBER
// MP2 : RING
enum class ConnectType { MEMBER, RING };

// This class represents a single network node (or a process).
// This everything that belongs to one machine (or process)
// should be here (where they information can be shared).
class NetworkNode
{
public:

	NetworkNode(string name, Log *log, Params *par, shared_ptr<INetwork> network);

	// Contains list of connections to use
	void registerHandler(ConnectType conntype,
						 shared_ptr<IConnection> connection,
						 shared_ptr<IMessageHandler> handler);
	void unregisterHandler(const Address &address);


	// Node actions
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
	vector<Address> getAddresses();
	shared_ptr<IConnection> getConnection(ConnectType conntype);

	inline const Address address(ConnectType conntype)
	{
		return this->getConnection(conntype)->address();
	}


	// Has the node failed?  Failure means that the node
	// no longer sends/receives messages across all connections.
	bool 		failed() const { return hasFailed; }
	void 		fail();

	// MP1 stuff
	MemberInfo 	member;

	// MP2 stuff
	RingInfo 	ring;
	//Node 		node;

protected:

	struct HandlerInfo
	{
		ConnectType 				conntype;
		shared_ptr<IConnection> 	connection;
		shared_ptr<IMessageHandler>	handler;
		shared_ptr<list<unique_ptr<RawMessage>>> queue;
	};

	bool 				hasFailed;
	string 				name;	
	map<NetworkID, HandlerInfo> handlers;
	Log *				log;
	Params *			par;
	weak_ptr<INetwork>	network;
	int 				timeout;
};

#endif /* NCLOUD_NETWORKNODE_H */
