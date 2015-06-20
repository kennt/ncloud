/*****
 * NetworkNode.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "NetworkNode.h"

namespace std {
	template<>
 	struct hash<Address> {
 	public:
 		size_t operator()(const Address &x) const
 		{
 			return hash<unsigned int>()(x.ipaddr) ^ hash<unsigned short>()(x.port);
 		}
 	};
}

NetworkNode::NetworkNode(string name, Log *log, Params *par, shared_ptr<INetwork> network)
    : member(log, par), ring(log, par), receivedQuitMessage(false)
{
    this->name = name;
    this->log = log;
    this->par = par;
    this->network = weak_ptr<INetwork>(network);
    this->hasFailed = false;
}

void NetworkNode::registerHandler(ConnectType conntype,
                                  shared_ptr<IConnection> connection,
                                  shared_ptr<IMessageHandler> handler)
{
    auto it = handlers.find(connection->address());
    if (it != handlers.end())
        throw NetworkException("address already registered");

    HandlerInfo     info = { conntype,
                             connection,
                             handler,
                             make_shared<list<unique_ptr<RawMessage>>>()
                            };
    handlers[connection->address()] = info;
}

void NetworkNode::unregisterHandler(const Address &address)
{
    auto it = handlers.find(address);
    if (it != handlers.end())
        handlers.erase(it);
}

vector<Address> NetworkNode::getAddresses()
{
    vector<Address>     v;
    for (auto & info : handlers)
    {
        v.push_back(info.second.connection->address());
    }
    return v;
}

shared_ptr<IConnection> NetworkNode::getConnection(ConnectType conntype)
{
    for (auto & info : handlers) {
        if (info.second.conntype == conntype) {
            return info.second.connection;
        }
    }
    return nullptr;
}

void NetworkNode::fail()
{
    //$ TODO: Do I need to set the fail state on my
    // connections?  (or maybe just as long as the node knows)
    this->hasFailed = true;
}

void NetworkNode::nodeStart(const Address &joinAddress, int timeout)
{
    this->hasFailed = false;
    this->timeout = timeout;

    // Need to start up all of the message handlers
    for (auto & info : handlers) {
        info.second.handler->start(joinAddress);
    }
}

void NetworkNode::receiveMessages()
{
    for (auto & info : handlers) {
        auto raw = info.second.connection->recv(timeout);
        while (raw != nullptr) {
            info.second.queue->push_back(std::move(raw));
            raw = info.second.connection->recv(0);
        }
    }
}

void NetworkNode::processQueuedMessages()
{
    for (auto & info : handlers) {
        // Equivalent to calling the old checkMessages()
        while (!info.second.queue->empty()) {
            auto raw = std::move(info.second.queue->front());
            info.second.queue->pop_front();
            if (raw.get())
                info.second.handler->onMessageReceived(raw.get());
        }

        // Equivalent to calling the old nodeLoopOps()
        info.second.handler->onTimeout();
    }
}

