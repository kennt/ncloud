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

unique_ptr<RawMessage> JoinRequestMessage::toRawMessage(const Address &from,
														const Address &to)
{
	//$ TODO: Check to see that we have all the data we need
	stringstream 	ss;

	write_raw<int>(ss, static_cast<int>(MEMBER_MSGTYPE::JOINREQ));
	write_raw<int>(ss, this->address.getIPAddress());
	write_raw<short>(ss, this->address.getPort());
	write_raw<long>(ss, this->heartbeat);

	return rawMessageFromStream(from, to, ss);
}

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
