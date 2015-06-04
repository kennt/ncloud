/*****
 * MP1.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "MP2.h"
#include "NetworkNode.h"


// Initializes the message handler.  Call this before any calls to
// onMessageReceived() or onTimeout().  Or if the connection has been reset.
//
void MP2MessageHandler::start(const Address &joinAddress)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("");

	node->member.inited = true;
}

// This is a callback and is called when the connection has received
// a message.
//
// The RawMessage will not be changed with.
//
void MP2MessageHandler::onMessageReceived(const RawMessage *raw)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

	//istringstream ss(std::string((const char *)raw->data.get(), raw->size));

	//RingMessageType msgtype = static_cast<RingMessageType>(read_raw<int>(ss));
	//ss.seekg(0, ss.beg);	// reset to start of the buffer

}

// This is called when there are no messages available (usually on a
// connection timeout).  Thus perform any actions that should be done
// on idle here.
//
void MP2MessageHandler::onTimeout()
{
	// run the node maintenance loop
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

	// Wait until you're in the group
	if (!node->member.inGroup)
		return;

	// ...then jump in and share your responsibilties!
	//
	//$ CODE:  Your code goes here
	//
}
