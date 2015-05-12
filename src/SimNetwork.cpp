
#include "stdincludes.h"

#include "Network.h"
#include "SimNetwork.h"

SimConnection::SimConnection(Params *par, weak_ptr<SimNetwork> simnet)
{
	this->par = par;
	this->simnet = simnet;

	this->status = IConnection::UNINITIALIZED;
}

SimConnection::~SimConnection()
{
}

template<>
void SimConnection::setOption(const char *name, int val)
{
	if (strcmp(name, "status") == 0)
		this->status = static_cast<IConnection::Status>(val);
	else
		throw new NetworkException(string_format("Unknown option : %s", name));
}

template<>
int SimConnection::getOption(const char *name)
{
	if (strcmp(name, "status") == 0)
		return static_cast<int>(this->status);
	else
		throw new NetworkException(string_format("Unknown option : %s", name));
	return 0;
}

int SimConnection::init(const Address &address)
{
	this->myAddress = address;

	//$ TODO: We're running with UDP, so there is no
	// CONNECTING status.
	this->status = IConnection::RUNNING;

	return 0;
}

IConnection::Status SimConnection::getStatus()
{
	return this->status;
}

void SimConnection::send(const RawMessage *rawmsg)
{
	//$ TODO: Determine what the correct error
	// codes should be here.
	if (status == IConnection::UNINITIALIZED)
		throw new NetworkException(EPERM, "Connection not initialized");
	if (status == IConnection::CLOSED)
		throw new NetworkException(ENETDOWN, "Connection has been closed");
	if (status != IConnection::RUNNING)
		throw new NetworkException(ENETDOWN, "Connection not enabled");

	//$ TODO: check for a max size

	auto msg = make_shared<SimMessage>();
	msg->fromAddress = this->myAddress;
	msg->toAddress = rawmsg->toAddress;
	msg->timestamp = par->getCurrtime();
	msg->dataSize = rawmsg->size;

	msg->data = new unsigned char[rawmsg->size];
	memcpy(msg->data, rawmsg->data, rawmsg->size);

	if (auto network = this->simnet.lock()) {
		msg->messageID = network->getNextMessageID();
		network->send(this, msg);
	}
	else {
		throw new NetworkException("the network object has been deleted");
	}
}

RawMessage * SimConnection::recv(int timeout)
{
	RawMessage * raw = nullptr;

	if (status == IConnection::UNINITIALIZED)
		throw new NetworkException(EPERM, "Connection not initialized");
	if (status == IConnection::CLOSED)
		throw new NetworkException(ENETDOWN, "Connection has been closed");
	if (status != IConnection::RUNNING)
		throw new NetworkException(ENETDOWN, "Connection not enabled");

	if (auto network = this->simnet.lock()) {
		shared_ptr<SimMessage> msg = network->recv(this);
		if (msg.get() != nullptr)
		{
			raw = new RawMessage();
			raw->toAddress = msg->toAddress;
			raw->fromAddress = msg->fromAddress;
			raw->size = msg->dataSize;
			raw->data = new unsigned char[msg->dataSize];
			memcpy(raw->data, msg->data, msg->dataSize);
		}
	}
	else {
		throw new NetworkException("the network object has been deleted");		
	}
	return raw;
}


SimNetwork::~SimNetwork()
{
	messages.clear();
	connections.clear();
}

shared_ptr<IConnection> SimNetwork::create(const Address &address)
{
	shared_ptr<IConnection> conn = this->find(address);
	if (conn != nullptr)
	{
		// raise an exception, this address is already in use
		// only allowed to create a single instance
		throw new NetworkException("address already in use");

	}
	auto simconnection = make_shared<SimConnection>(par, shared_from_this());
	simconnection->init(address);

	connections[address.getNetworkID()] = simconnection;
	return shared_ptr<IConnection>(simconnection);
}

shared_ptr<IConnection> SimNetwork::find(const Address &address)
{
	auto it = connections.find(address.getNetworkID());
	if (it == connections.end())
		return nullptr;
	return shared_ptr<IConnection>(it->second);
}

shared_ptr<SimConnection> SimNetwork::findSimConnection(const Address &address)
{
    auto it = connections.find(address.getNetworkID());
    if (it == connections.end())
        return nullptr;
    return it->second;
}

void SimNetwork::remove(const Address &address)
{
	auto it = connections.find(address.getNetworkID());
	if (it != connections.end())
	{
		it->second->setOption<int>("status", IConnection::CLOSED);
		connections.erase(it);
	}
}

void SimNetwork::send(IConnection *conn, shared_ptr<SimMessage> message)
{
	// Check to see if conn is valid
	if (find(conn->address()).get() != conn)
		throw new NetworkException("invalid connection");

	// Add this to the list of messages
	messages.emplace_back(message);
}

shared_ptr<SimMessage> SimNetwork::recv(IConnection *conn)
{
	shared_ptr<SimMessage> message = nullptr;

	// Check to see if conn is valid
	if (find(conn->address()).get() != conn)
		throw new NetworkException("invalid connection");

	for (auto it = messages.begin(); it != messages.end(); it++)
	{
		if ((*it)->toAddress == conn->address())
		{
			message = *it;
			messages.erase(it);
			break;
		}
	}

	return message;
}


