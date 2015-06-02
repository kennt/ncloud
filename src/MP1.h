/*****
 * MP1.h
 *
 * See LICENSE for details.
 *
 * Holds the classes needed to receive and handle messages for the 
 * Membership protocol.
 *
 * JoinRequestMessage represents a JoinRequest message.
 * MP1MessageHandler is called when a message is available (or if there
 * is a timeout, i.e. on idle).
 *
 *****/


#ifndef NCLOUD_MP1_H
#define NCLOUD_MP1_H

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "json/json.h"
#include "NetworkNode.h"


const int TREMOVE 	= 20;
const int TFAIL 	= 5;


enum MEMBER_MSGTYPE { NONE=0, JOINREQ };

class NetworkNode;


// See comment above.
//
struct JoinRequestMessage
{
	// Implied MEMBER_MSGTYPE::JOINREQ
	Address 	address;
	long 		heartbeat;

	JoinRequestMessage() : heartbeat(0)
	{}

	void load(istringstream& ss);
	unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);
};


// See comment above
//
class MP1MessageHandler: public IMessageHandler
{
public:
	MP1MessageHandler(Log *log, 
					  Params *par,
					  shared_ptr<NetworkNode> netnode,
					  shared_ptr<IConnection> connection)
		: log(log), par(par), netnode(netnode), connection(connection), timeout(0)
	{
	}

	virtual ~MP1MessageHandler() {}

	// Initializes the MessageHandler, if needed. This will be called
	// before onMessageReceived() or onTimeout() will be called.
	virtual void start(const Address &address) override;	

	// This is called when a message has been received.  This may be
	// called more than once for a timeslice.
	virtual void onMessageReceived(const RawMessage *) override;

	// Called when no messages are available (and the connection has timed out).
	virtual void onTimeout() override;

	// Performs the action to join a group (sending a JoinRequest message).
	void joinGroup(const Address& address);

protected:
	Log *					log;
	Params *				par;
	weak_ptr<NetworkNode>	netnode;
	shared_ptr<IConnection>	connection;
	int						timeout;
};


#endif /* NCLOUD_MP1_H */

