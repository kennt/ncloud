
#ifndef _NETWORKNODE_H
#define _NETWORKNODE_H

#include "stdincludes.h"	
#include "Params.h"
#include "Network.h"
#include "Member.h"

class NetworkNode;

using MessageQueue = std::list< unique_ptr<RawMessage>>;


class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}

	virtual void start() = 0;

	virtual void onReceive(const RawMessage *message) = 0;
	virtual void onEmptyLoop() = 0;
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
	enum ConnectionType { MEMBER, RING };

	NetworkNode(Params *par, shared_ptr<INetwork> network);

	// Contains list of connections to run
	// This occurs in two steps:
	//	Step 1: take messages off the connection and add to queue
	//  Step 2: take messages and queue and send to MessageHandler
	void registerHandler(ConnectionType conntype,
						 shared_ptr<IConnection> connection,
						 shared_ptr<IMessageHandler> handler);

	void unregisterHandler(const Address &address);


	// Node actions
	void nodeStart(const Address &joinAddress);

	// This looks for messages by calling recv() on each
	// connection
	void runReceiveLoop();

	void processMessageQueues();

	// Set of tuples
	using HandlerTuple = std::tuple<ConnectionType,
									shared_ptr<IConnection>,
			   						shared_ptr<MessageQueue>,
			   						shared_ptr<IMessageHandler>>;

	map<NetworkID, HandlerTuple> handlers;

	// Has the node failed?  Failure means that the node
	// no longer sends/receives messages.
	bool 		failed;

	// MP1 stuff
	MemberInfo 	member;

	// MP2 stuff
	//Node 		node;

protected:
	Params *			par;
	weak_ptr<INetwork>	network;
};

#endif /* _NETWORKNODE_H */
