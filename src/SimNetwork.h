
#ifndef _SIMNETWORK_H
#define _SIMNETWORK_H

#include "Params.h"
#include "Network.h"

class SimNetwork;

class SimConnection : public IConnection
{
public:
	SimConnection(Params *par, weak_ptr<SimNetwork> simnet);
	virtual ~SimConnection();

	int init(const Address &myAddress);

	virtual const Address &address() { return myAddress; }
	virtual IConnection::Status getStatus();

	virtual void send(const RawMessage *message);
	virtual RawMessage * recv(int timeout);

	// Use these functions to change state
	// (for example, changing running to true/false).
	//    setOption<bool>("running", false);
	template<typename T>
	void setOption(const char *name, T val);

	template<typename T>
	T getOption(const char *name);

protected:
	Params *		par;
	weak_ptr<SimNetwork>	simnet;
	Address 		myAddress;

	IConnection::Status status;

private:
	SimConnection(const SimConnection &);
	SimConnection &operator= (const SimConnection &);
};


struct SimMessage
{
	SimMessage()
	{
		timestamp = 0;
		messageID = 0;
		dataSize = 0;
		data = NULL;
	}

	~SimMessage()
	{
		delete data;
	}

	Address		fromAddr;
	Address		toAddr;
	int 		timestamp;
	int 		messageID;

	size_t		dataSize;
	unsigned char * data;

private:
	// Do not allow copying of the messages
	SimMessage(const SimMessage &other);
	SimMessage& operator= (SimMessage& other);
};


class SimNetwork : public INetwork, public enable_shared_from_this<SimNetwork>
{
public:
	static shared_ptr<SimNetwork> createNetwork(Params *par)
	{	return make_shared<SimNetwork>(par); }

	virtual ~SimNetwork();

	virtual shared_ptr<IConnection> create(const Address &myAddress);
	virtual shared_ptr<IConnection> find(const Address &address);
	virtual void remove(const Address &address);


	int 	getNextMessageID() { return nextMessageID++; }

	// These are the internal APIS to the actual simulated network
	// This will add a message that will be stored by our "network".
	// It is expected that if you want an error to be propagated
	// up, throw an exception. This is different from the case where
	// the network eats the message up but the higher-level code
	// received message success.
	void 	send(IConnection *conn, shared_ptr<SimMessage> message);
	shared_ptr<SimMessage> recv(IConnection *conn);

	// Internal API, used mostly for test purposes.
	shared_ptr<SimConnection> findSimConnection(const Address &address);

	// This should be made protected/private but it's doesn't
	// work with the compiler I have.
	SimNetwork(Params *par)
	{
		this->par = par;
		this->nextMessageID = 1;
	}

protected:

	Params *	par;
	int 		nextMessageID = 0;

	// a list of the currently buffered messages
	vector<shared_ptr<SimMessage>>	messages;

	// a list of connections
	// Note that the network is responsible for deleting connections
	map<NetworkID, shared_ptr<SimConnection>> connections;


};


#endif  /* _SIMNETWORK_H */
