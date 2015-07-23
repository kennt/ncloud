/*****
 * Network.h
 *
 * See LICENSE for details.
 *
 * This file contains the network interfaces. These are the
 * abstract base classes that isolate the idea of a network.
 *
 * AddressException
 *  Errors when dealing with parsing IP address strings.
 *
 * NetworkException
 *  Errors with using the network (not initialized, etc...).
 *
 * Address
 *  Encapsulates an IPv4 address and a port.
 *
 * RawMessage
 *  Represents a raw message that has been received (contains the
 *  binary data).  Used for both receiving and sending.
 *
 * IConnection
 *  Abstract idea of a network connection.
 *
 * INetwork
 *  Abstract network (mainly used as a connection factory).
 *
 *****/

#ifndef NCLOUD_NETWORK_H
#define NCLOUD_NETWORK_H

#include "Util.h"

using namespace std;

using byte = unsigned char;


// This exception is thrown by the Address class
// when a problem is found when parsing a string to 
// extract the IP address and port.
//
class AddressException : public exception
{
public:
    AddressException(const char *description)
    {   desc = description; }

    virtual const char * what() const throw()
    {   return desc.c_str(); }

protected:
    string  desc;
};

// Class used for general networking-related
// errors. Mainly used to indicate that a network
// operation could not be performed.
//
class NetworkException : public exception
{
public:
	NetworkException(const char *description)
	{
		this->description = string_format("%s", description);
		this->error = 0;
	}
	NetworkException(const string desc)
	{
		this->description = desc;
		this->error = 0;
	}
	NetworkException(int err, const char *description)
	{
		this->error = err;
		this->description = string_format("%s (%d)", description, err);
	}

	virtual const char * what() const throw()
	{	return description.c_str(); }

	virtual int errcode()
	{ 	return this->error; }

protected:
    string  description;
    int     error;
};


// Creates a 32-bit unsigned int that contains an IP-address.
// Takes the four separate octets and combines them into a
// single unsigned int.
//
inline unsigned int makeIPAddress(int a, int b, int c, int d)
{
    return ((a & 0xFF) << 24) +
            ((b & 0xFF) << 16) +
            ((c & 0xFF) << 8) +
            (d & 0xFF);
}


// This class represents the idea of a Network Address.  It is
// a combination of an IP-address and a port.
//
class Address
{
friend bool operator< (const Address&, const Address&);
friend bool operator> (const Address&, const Address&);
friend bool operator== (const Address&, const Address&);
friend bool operator!= (const Address&, const Address&);
friend struct hash<Address>;

public:
    enum class AddressType { None = 0, IPv4, IPv6 };

    Address() : addrtype(AddressType::None), ipaddr(0), port(0)
    {
    }

    Address(unsigned int ipaddr, unsigned short port)
        : addrtype(AddressType::IPv4), ipaddr(ipaddr), port(port)
    {
    }

    Address(int a, int b, int c, int d, unsigned short port)
    {
        this->addrtype = AddressType::IPv4;
        this->ipaddr = makeIPAddress(a, b, c, d);
        this->port = port;
    }

    // Returns true if all fields are 0.  This is typically used to
    // check if the address has been set (all fields are initialized to 0).
    bool isZero() const
    {
        return ipaddr == 0 && port == 0;
    }

    // property accessors
    unsigned int getIPv4Address() const
    {
        return ipaddr;
    }

    // Retrieves the individual octet of an IP address.  The
    // 0th octet are the highest order bits. The pos is [0..3].
    // Asking for a pos outside of [0..3] is undefined.
    //
    unsigned char getIPv4Octet(int pos) const
    {
        return (this->ipaddr >> ((3 - pos)*8)) & 0xFF;
    }

    unsigned short getPort() const
    {
        return port;
    }

    string toString() const
    {
        return string_format("%d.%d.%d.%d:%d",
            (ipaddr >> 24) & 0xFF,
            (ipaddr >> 16) & 0xFF,
            (ipaddr >> 8) & 0xFF,
            ipaddr & 0xFF,
            port
            );      
    }

    string toAddressString() const
    {
        return string_format("%d.%d.%d.%d",
            (ipaddr >> 24) & 0xFF,
            (ipaddr >> 16) & 0xFF,
            (ipaddr >> 8) & 0xFF,
            ipaddr & 0xFF
            );      
    }

    string toPortString() const
    {
        return string_format("%d", port);
    }

    // Parses a IPv4 address of the form "XXX.XXX.XXX.XXX" where XXX
    // is an octet (from 0..255).
    //
    // This could be extended to IPv6 addresses, but those are
    // more complicated than needs to be here.  Should use inet_pton().
    //
    // This function does a minimal amount of error checking.
    //
    void parse(const char *address, const char *port)
    {
        parse(address, static_cast<unsigned short>(stoi(port)));
    }

    void parse(const char *address, unsigned short port)
    {
        // For simplicity, make this explicit, expect a
        // full address
        unsigned short a, b, c, d;
        if (sscanf(address, "%hu.%hu.%hu.%hu", &a, &b, &c, &d) != 4)
            throw AddressException("improper IP address format");

        // Make sure that we're not passing in bogus addresses
        if ((a > 255) || (b > 255) || (c > 255) || (d > 255))
            throw AddressException("IPv4 address sections must be less than 255");

        this->addrtype = AddressType::IPv4;
        this->ipaddr = (a << 24) + (b << 16) + (c << 8) + d;
        this->port = port;
    }

protected:
    AddressType     addrtype;
    unsigned int    ipaddr;
    unsigned short  port;
};

inline bool operator< (const Address& lhs, const Address& rhs)
{
    if (lhs.ipaddr != rhs.ipaddr)
        return lhs.ipaddr < rhs.ipaddr;
    else
        return lhs.port < rhs.port;
}

inline bool operator> (const Address& lhs, const Address& rhs)
{
    return !(lhs < rhs) && !(lhs == rhs);
}

inline bool operator== (const Address& lhs, const Address& rhs)
{
    return (lhs.ipaddr == rhs.ipaddr) && (lhs.port == rhs.port);
}

inline bool operator!= (const Address& lhs, const Address& rhs)
{
    return !(lhs == rhs);
}

inline ostream& operator<<(ostream& os, const Address& address)
{
    os << address.toString();
    return os;
}

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


// RawMessage is used for both sending/receiving.  This is
// what gets passed to and from the interfaces.
//
struct RawMessage
{
    Address         fromAddress;    // ignored on send()
    Address         toAddress;
    size_t          size;
    unique_ptr<byte[]>  data;

    RawMessage()
        : size(0)
    {}
};


// Abstract interface used to model a connection for both the simulated
// network and the real network.
//
class IConnection
{
public:
    enum Status { UNINITIALIZED=0, CONNECTING, RUNNING, DISABLED, CLOSED };

    virtual ~IConnection() {};

    // Opens and closes the connection.  Does the work of setting and
    // tearing down the connection.  init() is called when INetwork::create
    // is called and close() is called when INetwork::remove is called.
    // Calling init() on an already initialized connection will fail.
    // Calling close() on an already closed() connection will do nothing.
    //
    // For legacy reasons, init() returns 0 on success, else failure.
    virtual int init(const Address& address) = 0;
    virtual void close() = 0;

    // Returns the address associated with this connection.
    //
    virtual const Address &address() = 0;

    virtual Status getStatus() = 0;

    virtual void send(const RawMessage *message) = 0;
    virtual unique_ptr<RawMessage> recv(int timeout) = 0;
};


// Abstract interface used as a factory for connections.
//
class INetwork
{
public:
    virtual ~INetwork() {};

    // creates a connection for a given address.  Only one connection
    // per address is allowed at any given time.
    virtual shared_ptr<IConnection> create(const Address &address) = 0;

    // Looks up the connection for an address.
    virtual shared_ptr<IConnection> find(const Address& address) = 0;

    // Removes the interface associated with the address from the network.
    virtual void remove(const Address& address) = 0;

    // Removes all connections and calls close() on each connection.
    virtual void removeAll() = 0;
};

#endif /* NCLOUD_NETWORK_H */
