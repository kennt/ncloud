/*****
 * SocketNetwork.h
 *
 * See LICENSE for details.
 *
 * Concrete implementation of the Network Intefaces based on sockets.
 *
 * SocketConnection
 *  Concreate IConnection implementation on sockets.
 *
 * SocketNetwork
 *  Factory interface for the network. This does very little
 *  because we no longer take care of storing messages.
 *
 *****/

#ifndef NCLOUD_SOCKETNETWORK_H
#define NCLOUD_SOCKETNETWORK_H

#include "Params.h"
#include "Network.h"
#include "SparseMatrix.h"

extern "C" {
#include <sys/socket.h>
};

// Maximum size of an individual message
const int MAX_BUFFER_SIZE = 16*1024;

class SocketNetwork;

struct SocketInfo
{
    SocketInfo() : fd(-1), sa_size(0)
    {
        memset(&sa, 0, sizeof(sa));
    }

    SocketInfo(int fd, struct sockaddr *sa, size_t sa_len);

    ~SocketInfo()
    {
        if (fd != -1)
            ::close(fd);
    }

    // Write copy and move explicitly
    // Copying not allowed
    //SocketInfo(const SocketInfo &) = delete;
    //SocketInfo& operator =(const SocketInfo &) = delete;

    // move constructor
    SocketInfo(SocketInfo && other) noexcept
    {
        SocketInfo  temp;
        *this = std::move(other);
        other = std::move(temp);
    }

    // move assignment
    SocketInfo& operator =(SocketInfo && other) noexcept
    {
        std::swap(fd, other.fd);
        std::swap(sa, other.sa);
        std::swap(sa_size, other.sa_size);
        return *this;
    }

    int                 fd;
    struct sockaddr_storage sa;
    size_t              sa_size;
};

// Concrete implementation of an IConnection
// This is built on top of sockets.
//
class SocketConnection : public IConnection
{
public:
    SocketConnection(Params *par, weak_ptr<SocketNetwork> socketnet);
    virtual ~SocketConnection();

    // Copying not allowed
    SocketConnection(const SocketConnection &) = delete;
    SocketConnection &operator= (const SocketConnection &) = delete;

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
    weak_ptr<SocketNetwork> socketnet;
    Address         myAddress;

    IConnection::Status status;

    int             socketDescriptor;

    // cache of sockets/addresses that we send to
    map<Address, shared_ptr<SocketInfo>>    sockets;

    // Looks up an address and returns a file descriptor
    // The calling code owns the descriptor.
    int lookupAddress(const Address & address);

    // Looks up an address and adds it to the sockets cache.
    void cacheAddress(const Address & address);

    // Internal api that is used for address lookup.
    int getAddress(const Address &address, bool doBind, bool addToCache);
};


// Concrete implementation of an INetwork
//
// This is built on top of sockets, so besides managing a list of
// connections, this interface does very little.
//
class SocketNetwork : public INetwork, public enable_shared_from_this<SocketNetwork>
{
public:
    // copying not allowed
    SocketNetwork(const SocketNetwork &) = delete;
    SocketNetwork& operator =(SocketNetwork &) = delete;

    // Factory to create a SocketNetwork. SocketNetwork's should be created
    // using this function, do not instantiate directly!
    static shared_ptr<SocketNetwork> createNetwork(Params *par)
    {   return make_shared<SocketNetwork>(par); }

    virtual ~SocketNetwork();

    virtual shared_ptr<IConnection> create(const Address &myAddress) override;
    virtual shared_ptr<IConnection> find(const Address &address) override;
    virtual void remove(const Address &address) override;
    virtual void removeAll() override;

    // Retrieves the next available message ID.  This is an internal ID within
    // the simulated network.
    int     getNextMessageID() { return nextMessageID++; }

    // Internal API, used mostly for test purposes.
    shared_ptr<SocketConnection> findSocketConnection(const Address &address);

    // Write out the summary statistics
    void writeMsgcountLog();

    // This should be protected/private, do not use this constructor.
    SocketNetwork(Params *par)
    {
        this->par = par;
        this->nextMessageID = 1;
    }

    // Statisical counts of msgs sent/recevied by this network
    int getSentCount(const Address& addr, int time)
    {   return sent(addr, time); }
    void updateSentCount(const Address& addr, int time)
    {   sent(addr, time)++; }

    int getReceivedCount(const Address& addr, int time)
    {   return received(addr, time); }
    void updateReceivedCount(const Address& addr, int time)
    {   received(addr, time)++; }

protected:

    Params *    par;

    // Use this to store ID numbers for messages
    int         nextMessageID = 0;

    // Maps the network address (IP address + port) to a connection
    map<Address, shared_ptr<SocketConnection>> connections;

    // Statistical counts
    // this means int matrix[Address][int]
    SparseMatrix<int, Address, int> sent;
    SparseMatrix<int, Address, int> received;
};


#endif  /* NCLOUD_SOCKETNETWORK_H */
