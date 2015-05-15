
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
	MP1MessageHandler(Params *par, shared_ptr<NetworkNode> netnode);
	virtual ~MP1MessageHandler() {}

	virtual void start() override;
	virtual void onReceive(const RawMessage *) override;
	virtual void onEmptyLoop() override;
};


#endif /* _MP1NODE_H */

