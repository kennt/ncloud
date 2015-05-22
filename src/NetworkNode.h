
#ifndef NCLOUD_NETWORKNODE_H
#define NCLOUD_NETWORKNODE_H

#include "stdincludes.h"	
#include "Params.h"
#include "Network.h"
#include "Member.h"

class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}

	virtual void start(const Address& address) = 0;

	virtual void onMessageReceived(const RawMessage *message) = 0;
	virtual void onTimeout() = 0;
};


// This class represents a single network node (or a process).
// This everything that belongs to one machine (or process)
// should be here (where they information can be shared).
class NetworkNode
{
public:
	// Describes what the connection is for
	// MP1 : MEMBER
	// MP2 : RING
	enum class ConnectionType { MEMBER, RING };

	NetworkNode(string name, Params *par, shared_ptr<INetwork> network);

	// Contains list of connections to use
	void registerHandler(ConnectionType conntype,
						 shared_ptr<IConnection> connection,
						 shared_ptr<IMessageHandler> handler);
	void unregisterHandler(const Address &address);


	// Node actions
	void nodeStart(const Address &joinAddress, int timeout);

	// This looks for messages by calling recv() on each
	// connection
	void receiveMessages();
	void processQueuedMessages();

	string getName()	{ return this->name; }
	vector<Address> getAddresses();
	shared_ptr<IConnection> getConnection(ConnectionType conntype);

	// Has the node failed?  Failure means that the node
	// no longer sends/receives messages across all connections.
	bool 		failed() const { return hasFailed; }
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
	map<NetworkID, HandlerInfo> handlers;
	Params *			par;
	weak_ptr<INetwork>	network;
	int 				timeout;
};

#endif /* NCLOUD_NETWORKNODE_H */
