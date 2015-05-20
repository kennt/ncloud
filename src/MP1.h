
#ifndef _MP1NODE_H
#define _MP1NODE_H

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "NetworkNode.h"


const int TREMOVE 	= 20;
const int TFAIL 	= 5;


class MP1MessageHandler: public IMessageHandler
{
public:
	MP1MessageHandler(Params *par, shared_ptr<NetworkNode> netnode)
		: par(par), netnode(netnode)
	{
	}

	virtual ~MP1MessageHandler() {}

	virtual void start() override;
	virtual void onMessageReceived(const RawMessage *) override;
	virtual void onTimeout() override;

protected:
	Params *				par;
	weak_ptr<NetworkNode>	netnode;
	int						timeout;
};


#endif /* _MP1NODE_H */

