/*****
 * SimNetwork.h
 *
 * See LICENSE for details.
 *
 * Concrete implementation of the Network Intefaces.
 * This simulates a network. Messages sent to other interfaces
 * are queued internally.
 *
 *****/

#ifndef NCLOUD_SIMNETWORK_H
#define NCLOUD_SIMNETWORK_H

#include "Params.h"
#include "Network.h"
#include "SparseMatrix.h"


const int MAX_BUFFER_SIZE = 16*1024;

class SimNetwork;

class SimConnection : public IConnection
{
public:
	SimConnection(Params *par, weak_ptr<SimNetwork> simnet);
	virtual ~SimConnection();

	SimConnection(const SimConnection &) = delete;
	SimConnection &operator= (const SimConnection &) = delete;

	int init(const Address &myAddress);

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
	Params *		par;
	weak_ptr<SimNetwork>	simnet;
	Address 		myAddress;

	IConnection::Status status;
};


struct SimMessage
{
	SimMessage()
	{
		timestamp = 0;
		messageID = 0;
		dataSize = 0;
	}

	Address		fromAddress;
	Address		toAddress;
	int 		timestamp;
	int 		messageID;

	size_t		dataSize;
	unique_ptr<byte[]> data;
};


class SimNetwork : public INetwork, public enable_shared_from_this<SimNetwork>
{
public:
	SimNetwork(const SimNetwork &) = delete;
	SimNetwork& operator =(SimNetwork &) = delete;

	static shared_ptr<SimNetwork> createNetwork(Params *par)
	{	return make_shared<SimNetwork>(par); }

	virtual ~SimNetwork();

	virtual shared_ptr<IConnection> create(const Address &myAddress) override;
	virtual shared_ptr<IConnection> find(const Address &address) override;
	virtual void remove(const Address &address) override;


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
	void writeMsgcountLog(int memberProtocolPort);

	// This should be made protected/private but it's doesn't
	// work with the compiler I have.
	SimNetwork(Params *par)
	{
		this->par = par;
		this->nextMessageID = 1;
	}

	int getSentCount(NetworkID netID, int time)
	{	return sent(netID, time); }

	int getReceivedCount(NetworkID netID, int time)
	{	return received(netID, time); }

protected:

	struct ConnectionInfo
	{
		shared_ptr<SimConnection> connection;

		// A list of buffered messages for this connection
		list<shared_ptr<SimMessage>>	messages;
	};

	Params *	par;
	int 		nextMessageID = 0;

	// a list of the currently buffered messages
	list<shared_ptr<SimMessage>>	messages;

	// a list of connections
	map<NetworkID, ConnectionInfo> connections;

	// Statistical counts
	// this means int matrix[NetworkID][int]
	SparseMatrix<int, NetworkID, int> sent;
	SparseMatrix<int, NetworkID, int> received;
};


#endif  /* NCLOUD_SIMNETWORK_H */
