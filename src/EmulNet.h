

#ifndef _EMULNET_H
#define _EMULNET_H

#define MAX_NODES 	1000
#define MAX_TIME	3600

// Force a maximum buffer size to ensure that we don't
// grow too big in the case of an error.
#define MAX_EMBUFFSIZE 30000

using namespace std;

// Network and Connection are the
// top level abstract classes.
//
// SimNetwork and SimConnection are implementations
// of the top level abstract classes.
// For instance, SocketNetwork and SocketConnection would
// provide a real implemention on top of sockets.
//
// Network and Connection provide higher level
// functionality (based on the interfaces).




class EmulNetConnection
{
public:
	EmulNetConnection(Params *p, EmulNet *emNet);
	~EmulNetConnection();

	// setup/teardown functions
	void init(const Address &address);
	int cleanup();

	// Encapsulation of basic network functions	
	int send(const Address &toaddr, string data);
	int send(const Address &toaddr, unsigned char *data, size_t size);
	int recv(int (* enq(void *, char *, int)), int timeout, void *queue);

	const Address&	address() { return this->myAddr; }

protected:
	bool		inited;
	bool		running;

	// Shared pointer?
	Params *	params;
	Address 	myAddr;
	EmulNet *	emNet;
	Connection *	connection;
	EmulNetConnectionStats stats;

private:
	EmulNetConnection(EmulNetConnection &other);
	EmulNetConnection& operator= (EmulNetConnection &other);
};

struct EmulNetConnectionStats
{
	unsigned int 	msgsReceived[MAXTIME];
	unsigned int 	msgsSent[MAXTIME];

	EmulNetConnectionStats()
	{
		for (int i=0; i<MAX_TIME; i++)
		{
			msgsSent[i] = 0;
			msgsReceived[i] = 0;
		}
	}

private:
	EmulNetConnectionStats(EmulNetConnectionStats &);
	EmulNetConnectionStats& operator= (EmulNetConnectionStats &);
};


struct EmulNetMessage
{
	EmulNetMessage()
	{
		timestamp = 0;
		messageID = 0;
		dataSize = 0;
		data = NULL;
	}

	~EmulNetMessage()
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
	EmulNetMessage(const EmulNetMessage &other);
	EmulNetMessage& operator= (EmulNetMessage& other);
};

// This class supports the idea of a network.
// All messages are stored here.
//
// All messages are stored here (they are assigned an internal id).
//
class EmulNet : public Network
{
public
	EmulNet(Params *params);
	virtual ~EmulNet();

	virtual Connection * createInterface(const Address &address);

	// Appends the message to the network buffer
	// Returns if the "send" was successful (this could
	// be a simulated error or a real error).
	bool sendToNetwork(EmulNetMessage *);

	// Retrieves a message from the network buffer
	// Returns a message if one was available otherwise NULL.
	EmulNetMessage * receiveFromNetwork(const Address &to);

private:
	EmulNet(EmulNet &other);
	EmulNet& operator= (EmulNet &other);

protected:
	bool	inited;

	// List of buffered messages (messages that have been sent
	// but not yet recv'd).
	vector<EmulNetMessage *>	messages;
};


#endif /* _EMULNET_H */
