
#include "stdincludes.h"	
#include "Params.h"
#include "Network.h"

#ifndef _NETWORKNODE_H
#define _NETWORKNODE_H

class NetworkNode;

class MessageQueue : public vector<RawMessage *>
{
public:
	~MessageQueue()
	{
		for_each(cbegin(), cend(), [](RawMessage *message) { delete message; });
		clear();
	}
};

class IMessageHandler
{
public:
	virtual ~IMessageHandler() {}

	virtual void onReceive(const RawMessage *) = 0;
	virtual void onEmptyLoop() = 0;
};


class MP1MessageHandler: public IMessageHandler
{
public:
	MP1MessageHandler(Params *par, shared_ptr<NetworkNode> netnode);
	virtual ~MP1MessageHandler() {}

	virtual void onReceive(const RawMessage *) override;
	virtual void onEmptyLoop() override;
};


// This class represents a single network node (or a process).
// This everything that belongs to one machine (or process)
// should be here (where they information can be shared).
class NetworkNode
{
public:
	NetworkNode(Params *par, shared_ptr<INetwork> network);
	~NetworkNode() {};

	// Contains list of connections to run
	// This occurs in two steps:
	//	Step 1: take messages off the connection and add to queue
	//  Step 2: take messages and queue and send to MessageHandler
	void registerHandler(shared_ptr<IConnection> connection,
						 shared_ptr<IMessageHandler> handler);

	void unregisterHandler(const Address &address);

	// This looks for messages by calling recv() on each
	// connection
	void runReceiveLoop();

	void processMessageQueues();

	// Set of tuples
	using HandlerTuple = std::tuple<shared_ptr<IConnection>,
			   						shared_ptr<MessageQueue>,
			   						shared_ptr<IMessageHandler>>;

	map<NetworkID, HandlerTuple> handlers;


	// MP1 stuff
	//MemberInfo 	member;

	// MP2 stuff
	//Node 		node;

protected:
	Params *			par;
	weak_ptr<INetwork>	network;
};

#endif /* _NETWORKNODE_H */
