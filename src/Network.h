/**********************************
 * FILE NAME: Address,h
 *
 * DESCRIPTION: Definition of a network address.
 **********************************/

#ifndef _NETWORK_H
#define _NETWORK_H

#include "Util.h"

using namespace std;

typedef unsigned long long NetworkID;

class AddressException : public exception
{
public:
	AddressException(const char *description)
	{
		desc = description;
	}

	virtual const char * what() const throw()
	{
		return desc.c_str();
	}

protected:
	string 	desc;
};

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


inline int makeIPAddress(int a, int b, int c, int d)
{
	return ((a & 0xFF) << 24) +
			((b & 0xFF) << 16) +
			((c & 0xFF) << 8) +
			(d & 0xFF);
}

class Address
{
public:
	Address()
	{
		this->ipaddr = 0;
		this->port = 0;
	}

	Address(unsigned int ipaddr, unsigned short port)
	{
		this->ipaddr = ipaddr;
		this->port = port;
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

	Address(const Address &anotherAddress)
	{
		this->ipaddr = anotherAddress.ipaddr;
		this->port = anotherAddress.port;
	}

	Address& operator =(const Address &other)
	{
		if (this == &other)
			return *this;

		this->ipaddr = other.ipaddr;
		this->port = other.port;

		return *this;
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
			throw new AddressException("missing ':'");

		// For simplicity, make this explicit, expect a
		// full address
		unsigned short a, b, c, d;
		if (sscanf(address.substr(0, pos).c_str(), "%hu.%hu.%hu.%hu", &a, &b, &c, &d) != 4)
			throw new AddressException("improper IP address format");

		// Make sure that we're not passing in bogus addresses
		if ((a > 255) || (b > 255) || (c > 255) || (d > 255))
			throw new AddressException("IPv4 address sections must be less than 255");

		this->ipaddr = (a << 24) + (b << 16) + (c << 8) + d;
		this->port = (unsigned short) stoi(address.substr(pos + 1,
									 	   address.size()-pos-1));
	}


	// Read/write from a buffer (assuming network order for
	// the byte-layout).
	//
	// These functions should use htonl, etc.. but that would
	// require OS-specific header files.  For now, just do
	// it ourselves.
	size_t writeToNetworkBuffer(unsigned char *buffer, size_t bufSize)
	{
		if (bufSize < (sizeof(ipaddr) + sizeof(port)))
			throw new AddressException("buffer size to small");
		memcpy((void *) &buffer[0], &this->ipaddr, sizeof(unsigned int));
		memcpy(&buffer[4], &this->port, sizeof(unsigned short));
		return sizeof(ipaddr) + sizeof(port);
	}

	void readFromNetworkBuffer(unsigned char *buffer, size_t bufSize)
	{
		if (bufSize < (sizeof(ipaddr) + sizeof(port)))
			throw new AddressException("buffer is too small");
		unsigned int addr = 0;
		unsigned short port = 0;

		memcpy(&addr, &buffer[0], sizeof(unsigned int));
		memcpy(&port, &buffer[4], sizeof(unsigned short));

		this->ipaddr = addr;
		this->port = port;
	}

protected:
	unsigned int 	ipaddr;
	unsigned short	port;
};

struct RawMessage
{
	Address 		fromAddress;	// ignored on send()
	Address 		toAddress;
	size_t 			size;
	unsigned char *	data;

	RawMessage()
	{
		size = 0;
		data = nullptr;
	}

	~RawMessage()
	{
		size = 0;
		delete[] data;
		data = nullptr;
	}
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
	virtual RawMessage * recv(int timeout) = 0;
};


class INetwork
{
public:
	virtual ~INetwork() {};

	virtual shared_ptr<IConnection> create(const Address &address) = 0;
	virtual shared_ptr<IConnection> find(const Address& address) = 0;
	virtual void remove(const Address& address) = 0;
};

#endif /* _NETWORK_H */
