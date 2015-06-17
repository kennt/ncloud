/*****
 * SimNetwork.h
 *
 * See LICENSE for details.
 *
 * Concrete implementation of the Network Intefaces.
 * This simulates a network. Messages sent to other interfaces
 * are stored in queues internally.
 *
 * SimConnection
 *  Simulated connection interface.
 *
 * SimMessage
 *  Internal message class for the Simulated Network.
 *
 * SimNetwork
 *  Factory interface for the simlated network.
 *
 *****/

#ifndef NCLOUD_SIMNETWORK_H
#define NCLOUD_SIMNETWORK_H

#include "Params.h"
#include "Network.h"
#include "SparseMatrix.h"


// Maximum size of an individual message
const int MAX_BUFFER_SIZE = 16*1024;

class SimNetwork;

// Concrete implementation of an IConnection
// This is used with the simulated network (SimNetwork).
//
class SimConnection : public IConnection
{
public:
    SimConnection(Params *par, weak_ptr<SimNetwork> simnet);
    virtual ~SimConnection();

    // Copying not allowed
    SimConnection(const SimConnection &) = delete;
    SimConnection &operator= (const SimConnection &) = delete;

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

protected:
    Params *        par;
    weak_ptr<SimNetwork>    simnet;
    Address         myAddress;

    IConnection::Status status;
};


// Used to store a message in the simulated network.
//
struct SimMessage
{
    SimMessage() : timestamp(0), messageID(0), dataSize(0)
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
// here in queues in a SimMessage.
//
class SimNetwork : public INetwork, public enable_shared_from_this<SimNetwork>
{
public:
    // copying not allowed
    SimNetwork(const SimNetwork &) = delete;
    SimNetwork& operator =(SimNetwork &) = delete;

    // Factory to create a SimNetwork. SimNetwork's should be created
    // using this function, do not instantiate directly!
    static shared_ptr<SimNetwork> createNetwork(Params *par)
    {   return make_shared<SimNetwork>(par); }

    virtual ~SimNetwork();

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
    void    send(IConnection *conn, shared_ptr<SimMessage> message);
    shared_ptr<SimMessage> recv(IConnection *conn);

    // Internal API, used mostly for test purposes.
    shared_ptr<SimConnection> findSimConnection(const Address &address);

    // Write out the summary statistics
    void writeMsgcountLog(int memberProtocolPort);

    // This should be protected/private, do not use this constructor.
    SimNetwork(Params *par)
    {
        this->par = par;
        this->nextMessageID = 1;
    }

    // Statisical counts of msgs sent/recevied by this network
    int getSentCount(const Address& addr, int time)
    {   return sent(addr, time); }

    int getReceivedCount(const Address& addr, int time)
    {   return received(addr, time); }

protected:

    // Stores the messages sent to this connection/address.
    //
    struct ConnectionInfo
    {
        shared_ptr<SimConnection> connection;

        // A list of buffered messages for this connection
        list<shared_ptr<SimMessage>>    messages;
    };

    Params *    par;

    // Use this to store ID numbers for messages
    int         nextMessageID = 0;

    // Maps the network address (IP address + port) to a
    // connection/message queue.
    map<Address, ConnectionInfo> connections;

    // Statistical counts
    // this means int matrix[Address][int]
    SparseMatrix<int, Address, int> sent;
    SparseMatrix<int, Address, int> received;
};


#endif  /* NCLOUD_SIMNETWORK_H */
