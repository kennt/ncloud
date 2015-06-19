/*****
 * MP1.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "MP1.h"
#include "NetworkNode.h"
#include "json/json.h"

const int TREMOVE 	= 20;
const int TFAIL 	= 5;



// Converts the data in a JoinRequestMessage structure into a RawMessage (which
// is ready to be sent over the wire).
//
// The pointer returned is a unique_ptr<> and should be freed by the caller.
//
unique_ptr<RawMessage> JoinRequestMessage::toRawMessage(const Address &from,
														const Address &to)
{
	//$ TODO: Check to see that we have all the data we need
	stringstream 	ss;

	write_raw<int>(ss, static_cast<int>(MEMBER_MSGTYPE::JOINREQ));
	write_raw<int>(ss, this->address.getIPv4Address());
	write_raw<short>(ss, this->address.getPort());
	write_raw<long>(ss, this->heartbeat);

	return rawMessageFromStream(from, to, ss);
}

// Loads the streamed binary data from the istringstream into a 
// JoinRequestMessage.  It is assumed that the stream is at the head
// of the message.
//
void JoinRequestMessage::load(istringstream &ss)
{
	int 	msgtype = read_raw<int>(ss);
	if (msgtype != MEMBER_MSGTYPE::JOINREQ)
		throw NetworkException("incorrect message type");

	int 	ipaddr = read_raw<int>(ss);
	short 	port = read_raw<short>(ss);
	long 	hb = read_raw<long>(ss);

	Address addr(ipaddr, port);
	this->address = addr;
	this->heartbeat = hb;
}

// Initializes the message handler.  Call this before any calls to
// onMessageReceived() or onTimeout().  Or if the connection has been reset.
//
void MP1MessageHandler::start(const Address &joinAddress)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("");

	node->member.inGroup = false;
	node->member.inited = true;

	node->member.memberList.clear();

	joinGroup(joinAddress);
}

// This is a callback and is called when the connection has received
// a message.
//
// The RawMessage will not be changed.
//
void MP1MessageHandler::onMessageReceived(const RawMessage *raw)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

	istringstream ss(std::string((const char *)raw->data.get(), raw->size));

	MEMBER_MSGTYPE msgtype = static_cast<MEMBER_MSGTYPE>(read_raw<int>(ss));
	ss.seekg(0, ss.beg);	// reset to start of the buffer

	switch(msgtype) {
		case MEMBER_MSGTYPE::JOINREQ: {
				DEBUG_LOG(log, raw->toAddress, "JOINREQ received from %s", 
					raw->fromAddress.toString().c_str());	

				JoinRequestMessage joinReq;
				joinReq.load(ss);

				node->member.addToMemberList(joinReq.address,
											 par->getCurrtime(),
											 joinReq.heartbeat);

				log->logNodeAdd(connection->address(), joinReq.address);

				//
				//$ CODE:  Your code goes here (to send the join reply)
				//
			}
			break;
		//
		//$ CODE:  Your code goes here (to handle the other messagess)
		//
		default:
			break;
	}
}

// This is called when there are no messages available (usually on a
// connection timeout).  Thus perform any actions that should be done
// on idle here.
//
void MP1MessageHandler::onTimeout()
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

// Joins the node to the current group.  The action performed is
// different depending on whether or not this node is the coordinator
// node or not.
//
void MP1MessageHandler::joinGroup(const Address & joinAddress)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

	if (connection->address() == joinAddress) {
		// we are the first process to join the group
		// boot up the group

		DEBUG_LOG(log, connection->address(), "Starting up group...");
		node->member.inGroup = true;
		node->member.addToMemberList(connection->address(),
									 par->getCurrtime(),
									 node->member.heartbeat);
	}
	else {
		// Send a JOINREQ to the coordinator
		JoinRequestMessage joinReq;

		joinReq.address = connection->address();
		joinReq.heartbeat = node->member.heartbeat;

		auto raw = joinReq.toRawMessage(connection->address(), joinAddress);

		DEBUG_LOG(log, connection->address(), "Trying to join...");

		connection->send(raw.get());
	}
}
