/*****
 * SimNetwork.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

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
		throw NetworkException(string_format("Unknown option : %s", name));
}

template<>
int SimConnection::getOption(const char *name)
{
	if (strcmp(name, "status") == 0)
		return static_cast<int>(this->status);
	else
		throw NetworkException(string_format("Unknown option : %s", name));
	return 0;
}

int SimConnection::init(const Address &address)
{
	if (this->status != IConnection::UNINITIALIZED &&
		this->status != IConnection::CLOSED)
		return -1;

	this->myAddress = address;

	//$ TODO: We're running with UDP, so there is no
	// CONNECTING status.
	this->status = IConnection::RUNNING;

	return 0;
}

void SimConnection::close()
{
	this->status = IConnection::CLOSED;
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
		throw NetworkException(EPERM, "Connection not initialized");
	if (status == IConnection::CLOSED)
		throw NetworkException(ENETDOWN, "Connection has been closed");
	if (status != IConnection::RUNNING)
		throw NetworkException(ENETDOWN, "Connection not enabled");

	//$ TODO: check for a max size

	auto msg = make_shared<SimMessage>();
	msg->fromAddress = this->myAddress;
	msg->toAddress = rawmsg->toAddress;
	msg->timestamp = par->getCurrtime();
	msg->dataSize = rawmsg->size;

	msg->data = unique_ptr<byte[]>(new byte[rawmsg->size]);
	memcpy(msg->data.get(), rawmsg->data.get(), rawmsg->size);

	if (auto network = this->simnet.lock()) {
		msg->messageID = network->getNextMessageID();
		network->send(this, msg);
	}
	else {
		throw NetworkException("the network object has been deleted");
	}
}

unique_ptr<RawMessage> SimConnection::recv(int timeout)
{
	unique_ptr<RawMessage> raw;

	if (status == IConnection::UNINITIALIZED)
		throw NetworkException(EPERM, "Connection not initialized");
	if (status == IConnection::CLOSED)
		throw NetworkException(ENETDOWN, "Connection has been closed");
	if (status != IConnection::RUNNING)
		throw NetworkException(ENETDOWN, "Connection not enabled");

	if (auto network = this->simnet.lock()) {
		shared_ptr<SimMessage> msg = network->recv(this);
		if (msg.get() != nullptr)
		{
			raw = make_unique<RawMessage>();
			raw->toAddress = msg->toAddress;
			raw->fromAddress = msg->fromAddress;
			raw->size = msg->dataSize;
			raw->data = unique_ptr<byte[]>(new byte[msg->dataSize]);
			memcpy(raw->data.get(), msg->data.get(), msg->dataSize);
		}
	}
	else {
		throw NetworkException("the network object has been deleted");		
	}
	return raw;
}


SimNetwork::~SimNetwork()
{
	removeAll();
}

shared_ptr<IConnection> SimNetwork::create(const Address &address)
{
	shared_ptr<IConnection> conn = this->find(address);
	ConnectionInfo 			conninfo;

	if (conn != nullptr)
	{
		// raise an exception, this address is already in use
		// only allowed to create a single instance
		throw NetworkException("address already in use");

	}
	auto simconnection = make_shared<SimConnection>(par, shared_from_this());
	simconnection->init(address);

	conninfo.connection = simconnection;
	connections[address] = conninfo;;

	return shared_ptr<IConnection>(simconnection);
}

shared_ptr<IConnection> SimNetwork::find(const Address &address)
{
	auto it = connections.find(address);
	if (it == connections.end())
		return nullptr;
	return shared_ptr<IConnection>(it->second.connection);
}

shared_ptr<SimConnection> SimNetwork::findSimConnection(const Address &address)
{
    auto it = connections.find(address);
    if (it == connections.end())
        return nullptr;
    return it->second.connection;
}

void SimNetwork::remove(const Address &address)
{
	auto it = connections.find(address);
	if (it != connections.end())
	{
		it->second.connection->close();
		connections.erase(it);
	}
}

void SimNetwork::removeAll()
{
	for (const auto & elem : connections)
		elem.second.connection->close();
	connections.clear();
}

void SimNetwork::send(IConnection *conn, shared_ptr<SimMessage> message)
{
	// Check to see if conn is valid
	if (find(conn->address()).get() != conn)
		throw NetworkException("connection not registered");

	auto it = connections.find(message->toAddress);
	if (it == connections.end())
		throw NetworkException("cannot find connection");

	if (message->dataSize >= par->maxMessageSize)
		throw NetworkException("buffer too large");

	if (par->dropMessages && ((rand() % 100) < (int)(par->msgDropProbability * 100)))
		return;

	if (it->second.messages.size() >= MAX_BUFFER_SIZE)
		throw NetworkException("too many messages, buffer limit exceeded");

	// Add this to the list of messages (note: we are adding the message
	// to the destination's queue).
	it->second.messages.emplace_back(message);

	// update the statistics
	sent(conn->address(), par->getCurrtime()) ++;
}

shared_ptr<SimMessage> SimNetwork::recv(IConnection *conn)
{
	// Check to see if conn is valid
	if (find(conn->address()).get() != conn)
		throw NetworkException("connection not registered");

	auto it = connections.find(conn->address());
	if (it == connections.end())
		throw NetworkException("cannot find connection");

	if (it->second.messages.empty())
		return nullptr;

	shared_ptr<SimMessage> message = it->second.messages.front();
	it->second.messages.pop_front();

	// update the statistics
	received(conn->address(), par->getCurrtime()) ++;

	return message;
}

void SimNetwork::writeMsgcountLog(int memberProtocolPort)
{
	int j;
	int sent_total, recv_total;

	FILE* file = fopen("msgcount.log", "w+");

	for (auto & elem: connections)
	{
		Address id = elem.first;
		Address 	address(id);

		sent_total = 0;
		recv_total = 0;

		fprintf(file, "node %s ", address.toString().c_str());

		for (j=0; j<par->getCurrtime(); j++) {
			sent_total += sent(id, j);
			recv_total += received(id, j);

			fprintf(file, " (%4d, %4d)", sent(id, j), received(id, j));
			if (j % 10 == 9) {
				fprintf(file, "\n         ");
			}
		}
		fprintf(file, "\n");
		fprintf(file, "node %s sent_total %6u  recv_total %6u\n\n", address.toString().c_str(), sent_total, recv_total);
	}

	fclose(file);
}




