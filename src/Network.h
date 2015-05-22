/*****
 * Network.h
 *
 * See LICENSE for details.
 *
 * This file contains the network interfaces. These are the
 * abstract base classes that isolate the idea of a network.
 *
 *****/

#ifndef NCLOUD_NETWORK_H
#define NCLOUD_NETWORK_H

#include "Util.h"

using namespace std;

// A NetworkID is a combination of an IP address and a port.
using NetworkID = long long;
using byte = unsigned char;


// This exception is thrown by the Address class
// when a problem is found when parsing a string to 
// extract the IP address and port.
//
class AddressException : public exception
{
public:
	AddressException(const char *description)
	{	desc = description; }

	virtual const char * what() const throw()
	{	return desc.c_str(); }

protected:
	string 	desc;
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
		this->description = description;
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
	string 	description;
	int 	error;
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
public:
	Address() : ipaddr(0), port(0)
	{
	}

	Address(unsigned int ipaddr, unsigned short port)
		: ipaddr(ipaddr), port(port)
	{
	}

	Address(int a, int b, int c, int d, unsigned short port)
	{
		this->ipaddr = makeIPAddress(a, b, c, d);
		this->port = port;
	}

	Address(NetworkID id)
	{
		this->ipaddr = (id >> 16) & 0xFFFFFFFF;
		this->port = (id & 0xFFFF);
	}

	bool operator ==(const Address &other) const
	{
		return (this->ipaddr == other.ipaddr &&
				this->port == other.port);
	}

	bool operator<(const Address &other) const
	{
		if (this->ipaddr < other.ipaddr)
			return true;
		else if (this->ipaddr > other.ipaddr)
			return false;
		else
			return this->port < other.port;
	}

	// property accessors
	unsigned int getIPAddress() const
	{
		return ipaddr;
	}

	unsigned short getPort() const
	{
		return port;
	}

	string 	toString() const
	{
		return string_format("%d.%d.%d.%d:%d",
			(ipaddr >> 24) & 0xFF,
			(ipaddr >> 16) & 0xFF,
			(ipaddr >> 8) & 0xFF,
			ipaddr & 0xFF,
			port
			);
	}

	NetworkID getNetworkID() const
	{
		return (((unsigned long long )ipaddr) << 16) + port;
	}

	void parse(string address)
	{
		size_t	pos = address.find(":");

		if (pos == string::npos)
			throw AddressException("missing ':'");

		// For simplicity, make this explicit, expect a
		// full address
		unsigned short a, b, c, d;
		if (sscanf(address.substr(0, pos).c_str(), "%hu.%hu.%hu.%hu", &a, &b, &c, &d) != 4)
			throw AddressException("improper IP address format");

		// Make sure that we're not passing in bogus addresses
		if ((a > 255) || (b > 255) || (c > 255) || (d > 255))
			throw AddressException("IPv4 address sections must be less than 255");

		this->ipaddr = (a << 24) + (b << 16) + (c << 8) + d;
		this->port = (unsigned short) stoi(address.substr(pos + 1,
									 	   address.size()-pos-1));
	}

protected:
	unsigned int 	ipaddr;
	unsigned short	port;
};


inline ostream& operator<<(ostream& os, const Address& address)
{
	os << address.toString();
	return os;
}

// RawMessage is used for both sending/receiving.  This is
// what gets passed to and from the interfaces.
struct RawMessage
{
	Address 		fromAddress;	// ignored on send()
	Address 		toAddress;
	size_t 			size;
	unique_ptr<byte[]>	data;

	RawMessage()
		: size(0)
	{}
};


class IConnection
{
public:
	enum Status { UNINITIALIZED=0, CONNECTING, RUNNING, DISABLED, CLOSED };

	virtual ~IConnection() {};

	virtual const Address &address() = 0;

	virtual Status getStatus() = 0;

	// CHECK: This assumes that we are doing UDP-like
	// communication.  Do we want TCP instead?
	virtual void send(const RawMessage *message) = 0;
	virtual unique_ptr<RawMessage> recv(int timeout) = 0;
};


class INetwork
{
public:
	virtual ~INetwork() {};

	virtual shared_ptr<IConnection> create(const Address &address) = 0;
	virtual shared_ptr<IConnection> find(const Address& address) = 0;
	virtual void remove(const Address& address) = 0;
};

#endif /* NCLOUD_NETWORK_H */
