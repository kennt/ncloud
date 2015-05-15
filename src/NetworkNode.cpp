
#include "NetworkNode.h"

MP1MessageHandler::MP1MessageHandler(Params *par, shared_ptr<NetworkNode> netnode)
{
}

void MP1MessageHandler::onReceive(const RawMessage *message)
{
}

void MP1MessageHandler::onEmptyLoop()
{
}

NetworkNode::NetworkNode(Params *par, shared_ptr<INetwork> network)
{
	this->par = par;
	this->network = weak_ptr<INetwork>(network);
	this->failed = false;
}

void NetworkNode::registerHandler(ConnectionType conntype,
								  shared_ptr<IConnection> connection,
						   		  shared_ptr<IMessageHandler> handler)
{
	auto it = handlers.find(connection->address().getNetworkID());
	if (it != handlers.end())
		throw new NetworkException("address already registered");

	handlers[connection->address().getNetworkID()] = 
		std::make_tuple(conntype,
						connection,
					   	make_shared<MessageQueue>(),
					   	handler);
}

void NetworkNode::unregisterHandler(const Address &address)
{
	auto it = handlers.find(address.getNetworkID());
	if (it != handlers.end())
		handlers.erase(it);
}