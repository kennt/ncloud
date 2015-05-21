
#include "NetworkNode.h"

NetworkNode::NetworkNode(string name, Params *par, shared_ptr<INetwork> network)
{
	this->name = name;
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
		throw NetworkException("address already registered");

	HandlerInfo 	info = {conntype, connection, handler};
	handlers[connection->address().getNetworkID()] = info;
}

void NetworkNode::unregisterHandler(const Address &address)
{
	auto it = handlers.find(address.getNetworkID());
	if (it != handlers.end())
		handlers.erase(it);
}

vector<Address> NetworkNode::getAddresses()
{
	vector<Address> 	v;
	for (auto & info : handlers)
	{
		v.push_back(info.second.connection->address());
	}
	return v;
}

shared_ptr<IConnection> NetworkNode::getConnection(ConnectionType conntype)
{
	for (auto & info : handlers) {
		if (info.second.conntype == conntype) {
			return info.second.connection;
		}
	}
	return nullptr;
}

void NetworkNode::nodeStart(const Address &joinAddress, int timeout)
{
	this->timeout = timeout;
}

void NetworkNode::receiveMessages()
{
	for (auto & info : handlers) {

		// Equivalent to calling the old checkMessages()
		auto raw = info.second.connection->recv(timeout);
		while (raw != nullptr) {
			info.second.handler->onMessageReceived(raw.get());
			raw = info.second.connection->recv(timeout);
		}

		// Equivalent to calling the old nodeLoopOps()
		info.second.handler->onTimeout();
	}
}