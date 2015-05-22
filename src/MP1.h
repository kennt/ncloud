
#ifndef _MP1NODE_H
#define _MP1NODE_H

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "json/json.h"
#include "NetworkNode.h"


const int TREMOVE 	= 20;
const int TFAIL 	= 5;


enum MEMBER_MSGTYPE { NONE=0, JOINREQ };

class NetworkNode;

class Message
{
public:
	Message(MEMBER_MSGTYPE msgtype) : msgtype(msgtype) {}
	virtual ~Message() {};

	virtual unique_ptr<RawMessage> toRawMessage() = 0;

protected:
	MEMBER_MSGTYPE 	msgtype;
};


class JoinRequestMessage : public Message
{
public:
	JoinRequestMessage(const Address &fromAddress,
				   	   const Address &toAddress,
				   	   long heartbeat)
		: Message(MEMBER_MSGTYPE::JOINREQ), 
		fromAddress(fromAddress),
		toAddress(toAddress),
		heartbeat(heartbeat)
	{
	}

	virtual unique_ptr<RawMessage> toRawMessage() override;

protected:
	Address 	fromAddress;
	Address 	toAddress;
	long 		heartbeat;
};

unique_ptr<RawMessage> rawMessageFromJson(const Address &fromAddress,
	const Address &toAddress, Json::Value root);

class MP1MessageHandler: public IMessageHandler
{
public:
	MP1MessageHandler(Log *log, Params *par, shared_ptr<NetworkNode> netnode)
		: log(log), par(par), netnode(netnode), timeout(0)
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
	int						timeout;
};


#endif /* _MP1NODE_H */

