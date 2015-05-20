
#ifndef _NETWORKNODE_H
#define _NETWORKNODE_H

#include "stdincludes.h"	
#include "Params.h"
#include "Network.h"
#include "Member.h"

class NetworkNode;

class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}

	virtual void start() = 0;

	virtual void onMessageReceived(const RawMessage *message) = 0;
	virtual void onTimeout() = 0;
};


// This class represents a single network node (or a process).
// This everything that belongs to one machine (or process)
// should be here (where they information can be shared).
class NetworkNode
{
public:
	// Connection types:
	// MEMBER : MP1 Membership protocol
	// RING : MP2 Ring protocol
	enum class ConnectionType { MEMBER, RING };

	struct HandlerInfo
	{
		ConnectionType 				conntype;
		shared_ptr<IConnection> 	connection;
		shared_ptr<IMessageHandler>	handler;
	};

	NetworkNode(Params *par, shared_ptr<INetwork> network);

	// Contains list of connections to run
	void registerHandler(ConnectionType conntype,
						 shared_ptr<IConnection> connection,
						 shared_ptr<IMessageHandler> handler);

	void unregisterHandler(const Address &address);


	// Node actions
	void nodeStart(const Address &joinAddress, int timeout);

	// This looks for messages by calling recv() on each
	// connection
	void runReceiveLoop();

	void processMessageQueues();

	map<NetworkID, HandlerInfo> handlers;

	// Has the node failed?  Failure means that the node
	// no longer sends/receives messages across all connections.
	bool 		failed;

	// MP1 stuff
	MemberInfo 	member;

	// MP2 stuff
	//Node 		node;

protected:
	Params *			par;
	weak_ptr<INetwork>	network;
	int 				timeout;
};

#endif /* _NETWORKNODE_H */
