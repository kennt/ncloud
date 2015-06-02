/*****
 * MP1.h
 *
 * See LICENSE for details.
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

	virtual void start(const Address &address) override;
	virtual void onMessageReceived(const RawMessage *) override;
	virtual void onTimeout() override;

	void joinGroup(const Address& address);

protected:
	Log *					log;
	Params *				par;
	weak_ptr<NetworkNode>	netnode;
	shared_ptr<IConnection>	connection;
	int						timeout;
};


#endif /* NCLOUD_MP1_H */

