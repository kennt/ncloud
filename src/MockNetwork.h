/*****
 * MockNetwork.h
 *
 * See LICENSE for details.
 *
 * Concrete implementation of the Network Intefaces.
 * Creates an interface used by the test framework.  This is
 * not as robust and mostly saves the messages for test
 * verification.
 *
 *****/

#ifndef NCLOUD_MOCKNETWORK_H
#define NCLOUD_MOCKNETWORK_H

#include "Params.h"
#include "Network.h"


// Maximum size of an individual message
const int MAX_BUFFER_SIZE = 16*1024;

class MockNetwork;

// Concrete implementation of an IConnection
//
class MockConnection : public IConnection
{
public:
    MockConnection(Params *par, weak_ptr<MockNetwork> simnet);
    virtual ~MockConnection();

    // Copying not allowed
    MockConnection(const MockConnection &) = delete;
    MockConnection &operator= (const MockConnection &) = delete;

    virtual int init(const Address &myAddress) override;
    virtual void close() override;

    virtual const Address &address() override { return myAddress; }
    virtual IConnection::Status getStatus() override;

    virtual void send(const RawMessage *message) override;
    virtual unique_ptr<RawMessage> recv(int timeout) override;

    // Use these functions to change state
    // (for example, changing running to true/false).
    //    setOption<bool>("running", false);
    template<typename T>
    void setOption(const char *name, T val);

    template<typename T>
    T getOption(const char *name);

    void reset();

protected:
    Params *        par;
    weak_ptr<MockNetwork>    simnet;
    Address         myAddress;

    IConnection::Status status;

    size_t          messagesSent;
    size_t          messagesReceived;
};


// Used to store a message in the simulated network.
//
struct MockMessage
{
    MockMessage() : timestamp(0), messageID(0), dataSize(0)
    {
    }

    Address     fromAddress;
    Address     toAddress;
    int         timestamp;
    int         messageID;

    size_t      dataSize;
    unique_ptr<byte[]> data;
};


// Concrete implementation of an INetwork
// This represents a "network".  Messages that are "sent" are stored
// here in queues in a MockMessage.
//
class MockNetwork : public INetwork, public enable_shared_from_this<MockNetwork>
{
public:
    // copying not allowed
    MockNetwork(const MockNetwork &) = delete;
    MockNetwork& operator =(MockNetwork &) = delete;

    // Factory to create a MockNetwork. MockNetwork's should be created
    // using this function, do not instantiate directly!
    static shared_ptr<MockNetwork> createNetwork(Params *par)
    {   return make_shared<MockNetwork>(par); }

    virtual ~MockNetwork();

    virtual shared_ptr<IConnection> create(const Address &myAddress) override;
    virtual shared_ptr<IConnection> find(const Address &address) override;
    virtual void remove(const Address &address) override;
    virtual void removeAll() override;

    // Retrieves the next available message ID.  This is an internal ID within
    // the simulated network.
    int     getNextMessageID() { return nextMessageID++; }

    // These are the internal APIS to the actual simulated network
    // This will add a message that will be stored by our "network".
    // It is expected that if you want an error to be propagated
    // up, throw an exception. This is different from the case where
    // the network eats the message up but the higher-level code
    // received message success.
    void    send(IConnection *conn, shared_ptr<MockMessage> message);
    shared_ptr<MockMessage> recv(IConnection *conn);

    // Internal API, used mostly for test purposes.
    shared_ptr<MockConnection> findMockConnection(const Address &address);

    // Internal API, resets and clears the connection.
    // Returns the network to a valid state, appearing to be
    // a just-initialized network.
    void reset();

    // This should be protected/private, do not use this constructor.
    MockNetwork(Params *par)
    {
        this->par = par;
        this->nextMessageID = 1;
    }

    // List of all messages "in" the network, they have
    // been sent but not received.  This is public for easy
    // manipulation/verfication by the unit tests.
    //
    // However, it does not remove the connections. It will
    // call reset() on each connection though.
    vector<shared_ptr<MockMessage>>     messages;

protected:
    Params *    par;

    // Use this to store ID numbers for messages
    int         nextMessageID = 0;

    // Maps the network address (IP address + port) to a
    // connection/message queue.
    map<Address, shared_ptr<MockConnection>> connections;

};


#endif  /* NCLOUD_MOCKNETWORK_H */
