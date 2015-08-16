/*****
 * MockNetwork.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "stdincludes.h"

#include "Network.h"
#include "MockNetwork.h"

MockConnection::MockConnection(Params *par, weak_ptr<MockNetwork> simnet)
    : messagesSent(0), messagesReceived(0)
{
    this->par = par;
    this->simnet = simnet;

    this->status = IConnection::UNINITIALIZED;
}

MockConnection::~MockConnection()
{
}

template<>
void MockConnection::setOption(const char *name, int val)
{
    if (strcmp(name, "status") == 0)
        this->status = static_cast<IConnection::Status>(val);
    else
        throw NetworkException(string_format("Unknown option : %s", name));
}

template<>
int MockConnection::getOption(const char *name)
{
    if (strcmp(name, "status") == 0)
        return static_cast<int>(this->status);
    else
        throw NetworkException(string_format("Unknown option : %s", name));
    return 0;
}

int MockConnection::init(const Address &address)
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

void MockConnection::close()
{
    this->status = IConnection::CLOSED;
}

IConnection::Status MockConnection::getStatus()
{
    return this->status;
}

void MockConnection::send(const RawMessage *rawmsg)
{
    if (status == IConnection::UNINITIALIZED)
        throw NetworkException(EPERM, "Connection not initialized");
    if (status == IConnection::CLOSED)
        throw NetworkException(ENETDOWN, "Connection has been closed");
    if (status != IConnection::RUNNING)
        throw NetworkException(ENETDOWN, "Connection not enabled");

    auto msg = make_shared<MockMessage>();
    msg->fromAddress = this->myAddress;
    msg->toAddress = rawmsg->toAddress;
    msg->timestamp = par->getCurrtime();
    msg->dataSize = rawmsg->size;

    msg->data = unique_ptr<byte[]>(new byte[rawmsg->size]);
    memcpy(msg->data.get(), rawmsg->data.get(), rawmsg->size);

    if (auto network = this->simnet.lock()) {
        msg->messageID = network->getNextMessageID();
        network->send(this, msg);

        messagesSent++;
    }
    else {
        throw NetworkException("the network object has been deleted");
    }
}

unique_ptr<RawMessage> MockConnection::recv(int timeout)
{
    unique_ptr<RawMessage> raw;

    if (status == IConnection::UNINITIALIZED)
        throw NetworkException(EPERM, "Connection not initialized");
    if (status == IConnection::CLOSED)
        throw NetworkException(ENETDOWN, "Connection has been closed");
    if (status != IConnection::RUNNING)
        throw NetworkException(ENETDOWN, "Connection not enabled");

    if (auto network = this->simnet.lock()) {
        shared_ptr<MockMessage> msg = network->recv(this);
        if (msg.get() != nullptr)
        {
            raw = make_unique<RawMessage>();
            raw->toAddress = msg->toAddress;
            raw->fromAddress = msg->fromAddress;
            raw->size = msg->dataSize;
            raw->data = unique_ptr<byte[]>(new byte[msg->dataSize]);
            memcpy(raw->data.get(), msg->data.get(), msg->dataSize);

            messagesReceived++;
        }
    }
    else {
        throw NetworkException("the network object has been deleted");      
    }
    return raw;
}

// Resets the connection to a valid blank state. This is the
// state just after init() was called.  The connection is
// in a state where it is ready to send/recv.
void MockConnection::reset()
{
    messagesSent = 0;
    messagesReceived = 0;
    status = IConnection::RUNNING;
}

MockNetwork::~MockNetwork()
{
    removeAll();
}

shared_ptr<IConnection> MockNetwork::create(const Address &address)
{
    shared_ptr<IConnection> conn = this->find(address);

    if (conn != nullptr)
    {
        // raise an exception, this address is already in use
        // only allowed to create a single instance
        throw NetworkException("address already in use");

    }
    auto mockConnection = make_shared<MockConnection>(par, shared_from_this());
    mockConnection->init(address);

    connections[address] = mockConnection;

    return shared_ptr<IConnection>(mockConnection);
}

shared_ptr<IConnection> MockNetwork::find(const Address &address)
{
    auto it = connections.find(address);
    if (it == connections.end())
        return nullptr;
    return shared_ptr<IConnection>(it->second);
}

shared_ptr<MockConnection> MockNetwork::findMockConnection(const Address &address)
{
    auto it = connections.find(address);
    if (it == connections.end())
        return nullptr;
    return it->second;
}

void MockNetwork::remove(const Address &address)
{
    auto it = connections.find(address);
    if (it != connections.end())
    {
        it->second->close();
        connections.erase(it);
    }
}

void MockNetwork::removeAll()
{
    for (const auto & elem : connections)
        elem.second->close();
    connections.clear();
}

void MockNetwork::send(IConnection *conn, shared_ptr<MockMessage> message)
{
    // Check to see if conn is valid
    if (find(conn->address()).get() != conn)
        throw NetworkException("connection not registered");

    if (message->dataSize >= par->maxMessageSize)
        throw NetworkException("buffer too large");

    if (messages.size() >= MAX_BUFFER_SIZE)
        throw NetworkException("too many messages, buffer limit exceeded");

    // Add this to the list of messages.  Note that we do not
    // check for the existence of the connection.
    messages.emplace_back(message);
}

shared_ptr<MockMessage> MockNetwork::recv(IConnection *conn)
{
    // Check to see if conn is valid
    if (find(conn->address()).get() != conn)
        throw NetworkException("connection not registered");

    auto it = connections.find(conn->address());
    if (it == connections.end())
        throw NetworkException("cannot find connection");

    if (messages.empty())
        return nullptr;

    // Look for messages that are being sent to this address
    shared_ptr<MockMessage> message;

    for (auto it = messages.begin(); it != messages.end(); it++) {
        if ((*it)->toAddress == conn->address()) {
            message = *it;
            messages.erase(it);
            break;
        }
    }
    return message;
}

void MockNetwork::reset()
{
    this->nextMessageID = 1;
    this->messages.clear();

    for(auto & elem: connections) {
        elem.second->reset();
    }
}

void MockNetwork::flush()
{
    this->messages.clear();
}

