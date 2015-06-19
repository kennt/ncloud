/*****
 * SocketNetwork.cpp
 *
 * See LICENSE for details.
 *
 * Ths SocketNetwork implementation relies on the socket API,
 * mainly to keep the number of external libraries down to a
 * minumum.
 *
 *****/

#include "stdincludes.h"

#include "Network.h"
#include "SocketNetwork.h"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
};

// Converts a socket sockaddr to an Address structure
// Takes care of IPv4 and IPv6 addresses
//
Address sockaddrToAddress(struct sockaddr *sa)
{
    Address     address;
    char        buf[INET6_ADDRSTRLEN];

    assert(INET6_ADDRSTRLEN > INET_ADDRSTRLEN);

    if (sa->sa_family == AF_INET) {
        struct sockaddr_in * psa = (struct sockaddr_in *)sa;
        ::inet_ntop(AF_INET, &(psa->sin_addr), buf, sizeof(buf));
        address.parse(buf, ntohs(psa->sin_port));
    }
    else {
        //$ TODO: should probably throw as v6 addresses not
        // yet supported
        struct sockaddr_in6* psa = (struct sockaddr_in6 *)sa;
        ::inet_ntop(AF_INET6, &(psa->sin6_addr), buf, sizeof(buf));
        address.parse(buf, ntohs(psa->sin6_port));
    }

    return address;
}


SocketInfo::SocketInfo(int fd, struct sockaddr *sa, size_t sa_len)
    : fd(fd), sa_size(sa_len)
{
    memcpy(&(this->sa), sa, sa_len);
}

int SocketConnection::lookupAddress(const Address & address)
{
    return getAddress(address, true /* doBind */, false /* addToCache */);
}

void SocketConnection::cacheAddress(const Address & address)
{
    getAddress(address, false /* doBind */, true /* addToCache */);
}

int SocketConnection::getAddress(const Address & address,
                                 bool doBind,
                                 bool addToCache)
{
    // This is IPv4/IPv6 friendly, however the Address
    // relies on IPv4.

    // Is this address already in our cache?
    if (addToCache) {
        auto it = sockets.find(address);
        if (it != sockets.end()) {
            return it->second->fd;
        }
    }

    struct addrinfo     hints;
    struct addrinfo *   servinfo;
    int                 rv;
    int                 errcode = 0;
    int                 fd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = ::getaddrinfo(address.toAddressString().c_str(),
                            address.toPortString().c_str(),
                            &hints,
                            &servinfo)) != 0) {
        throw NetworkException(
            string_format("getaddrinfo failed: %s", gai_strerror(rv)).c_str());
    }   

    for (auto p = servinfo; p != NULL; p = p->ai_next) {
        int     sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            errcode = errno;
            continue;
        }
        if (doBind && ::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            errcode = errno;
            ::close(sockfd);
            continue;
        }

        // Add the entry into our cache
        // The SocketInfo takes control of the file descriptor, fd.
        fd = sockfd;
        if (addToCache) {
            shared_ptr<SocketInfo> si =
                make_shared<SocketInfo>(fd, p->ai_addr, p->ai_addrlen);
            sockets.emplace(address, si);
        }
        break;
    }
    ::freeaddrinfo(servinfo);

    if (fd == -1) {
        throw NetworkException(errcode, "failed to bind a socket");
    }

    return fd;
}


SocketConnection::SocketConnection(Params *par, weak_ptr<SocketNetwork> socketnet)
    : par(par),
    socketnet(socketnet),
    status(IConnection::UNINITIALIZED),
    socketDescriptor(-1)
{
}

SocketConnection::~SocketConnection()
{
}

template<>
void SocketConnection::setOption(const char *name, int val)
{
    if (strcmp(name, "status") == 0)
        this->status = static_cast<IConnection::Status>(val);
    else
        throw NetworkException(string_format("Unknown option : %s", name));
}

template<>
int SocketConnection::getOption(const char *name)
{
    if (strcmp(name, "status") == 0)
        return static_cast<int>(this->status);
    else
        throw NetworkException(string_format("Unknown option : %s", name));
    return 0;
}

int SocketConnection::init(const Address &address)
{
    if (this->status != IConnection::UNINITIALIZED &&
        this->status != IConnection::CLOSED)
        return -1;

    assert(socketDescriptor == -1);

    socketDescriptor = lookupAddress(address);
    this->myAddress = address;

    //$ TODO: We're running with UDP, so there is no
    // CONNECTING status.
    this->status = IConnection::RUNNING;

    return 0;
}

void SocketConnection::close()
{
    if (socketDescriptor != -1)
        ::close(socketDescriptor);
    socketDescriptor = -1;
    this->status = IConnection::CLOSED;
}

IConnection::Status SocketConnection::getStatus()
{
    return this->status;
}

void SocketConnection::send(const RawMessage *rawmsg)
{
    //$ TODO: Determine what the correct error
    // codes should be here.
    if (status == IConnection::UNINITIALIZED)
        throw NetworkException(EPERM, "Connection not initialized");
    if (status == IConnection::CLOSED)
        throw NetworkException(ENETDOWN, "Connection has been closed");
    if (status != IConnection::RUNNING)
        throw NetworkException(ENETDOWN, "Connection not enabled");

    assert(socketDescriptor != -1);

    // Check to see if the address is in the cache and
    // add if it is not
    cacheAddress(rawmsg->toAddress);

    // Pull the socket info out of the cache
    auto it = sockets.find(rawmsg->toAddress);
    if (it == sockets.end())
        throw NetworkException("");

    size_t bytesSent = ::sendto(socketDescriptor,
                                rawmsg->data.get(),
                                rawmsg->size,
                                0,
                                (struct sockaddr *) &(it->second->sa),
                                static_cast<socklen_t>(it->second->sa_size));

    if (bytesSent == -1)
        throw NetworkException(errno, "error sending");

    //$ TODO: assume we can send everything
    if (bytesSent < rawmsg->size)
        throw NetworkException("partial message sent!");
}

unique_ptr<RawMessage> SocketConnection::recv(int timeout)
{
    unique_ptr<RawMessage>  raw;
    byte                    buf[1500];

    if (status == IConnection::UNINITIALIZED)
        throw NetworkException(EPERM, "Connection not initialized");
    if (status == IConnection::CLOSED)
        throw NetworkException(ENETDOWN, "Connection has been closed");
    if (status != IConnection::RUNNING)
        throw NetworkException(ENETDOWN, "Connection not enabled");

    assert(socketDescriptor != -1);

    if (auto network = this->socketnet.lock()) {
        fd_set          fds;
        struct timeval  tv;
        int             n;
        struct sockaddr_storage recvaddr;
        socklen_t       recvaddr_len = sizeof(recvaddr);

        FD_ZERO(&fds);
        FD_SET(socketDescriptor, &fds);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        n = ::select(socketDescriptor+1, &fds, NULL, NULL, &tv);

        if (n == 0)
            return nullptr;
        if (n == -1) {
            // MacOS may return EINTR on timeout
            // EAGAIN == no data available, try again
            if ((errno == EAGAIN) || (errno == EINTR))
                return nullptr;
            else if (errno == EPIPE)
                throw NetworkException("connection closed");
            throw NetworkException(errno, "error in select");
        }

        //$ TODO: assumes that everything is received at once
        size_t bytesRcvd = ::recvfrom(socketDescriptor,
                                   buf,
                                   sizeof buf,
                                   0,
                                   (struct sockaddr *)&recvaddr,
                                   &recvaddr_len);

        Address address = sockaddrToAddress((struct sockaddr *) &recvaddr);
        raw = make_unique<RawMessage>();
        raw->toAddress = this->myAddress;
        raw->fromAddress = address;
        raw->size = bytesRcvd;
        raw->data = unique_ptr<byte[]>(new byte[bytesRcvd]);
        memcpy(raw->data.get(), buf, bytesRcvd);
    }
    else {
        throw NetworkException("the network object has been deleted");      
    }
    return raw;
}


SocketNetwork::~SocketNetwork()
{
    removeAll();
}

shared_ptr<IConnection> SocketNetwork::create(const Address &address)
{
    shared_ptr<IConnection> conn = this->find(address);

    if (conn != nullptr)
    {
        // raise an exception, this address is already in use
        // only allowed to create a single instance
        throw NetworkException("address already in use");

    }
    auto simconnection = make_shared<SocketConnection>(par, shared_from_this());
    simconnection->init(address);

    connections[address] = simconnection;

    return shared_ptr<IConnection>(simconnection);
}

shared_ptr<IConnection> SocketNetwork::find(const Address &address)
{
    auto it = connections.find(address);
    if (it == connections.end())
        return nullptr;
    return shared_ptr<IConnection>(it->second);
}

shared_ptr<SocketConnection> SocketNetwork::findSocketConnection(const Address &address)
{
    auto it = connections.find(address);
    if (it == connections.end())
        return nullptr;
    return it->second;
}

void SocketNetwork::remove(const Address &address)
{
    auto it = connections.find(address);
    if (it != connections.end())
    {
        it->second->close();
        connections.erase(it);
    }
}

void SocketNetwork::removeAll()
{
    for (const auto & elem : connections)
        elem.second->close();
    connections.clear();
}

void SocketNetwork::writeMsgcountLog()
{
    int j;
    int sent_total, recv_total;

    FILE* file = fopen("msgcount.log", "w+");

    for (auto & elem: connections)
    {
        Address id = elem.first;
        Address     address(id);

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




