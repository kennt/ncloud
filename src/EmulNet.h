
#include "Network.h"

#ifndef _EMULNET_H
#define _EMULNET_H

#define MAX_NODES 	100
#define MAX_TIME	3600

// Force a maximum buffer size to ensure that we don't
// grow too big in the case of an error.
#define MAX_EMBUFFSIZE 30000

using namespace std;

// INetwork and IConnection are the top level
// interfaces.
//
// SimNetwork and SimConnection are implementations
// of those network interfaces (simulated).
//
// EmulNet is an implementation on top of the 
// INetwork and IConnection interfaces.  It provides
// a queueing mechanism on top of the network.




class EmulNetConnection
{
public:
	EmulNetConnection(Params *p, shared_ptr<IConnection> connection);
	~EmulNetConnection();

	// Encapsulation of basic network functions	
	int send(const Address &toaddr, string data);
	int send(const Address &toaddr, unsigned char *data, size_t size);
	int recv(std::function< void(void *, RawMessage *) > callback, int timeout, void *context);

	const Address&	address() { return connection->address(); }

protected:
	// Shared pointer?
	Params *	params;
	shared_ptr<IConnection>	connection;
private:
	EmulNetConnection(EmulNetConnection &other);
	EmulNetConnection& operator= (EmulNetConnection &other);
};


#endif /* _EMULNET_H */
