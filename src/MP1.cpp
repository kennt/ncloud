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

unique_ptr<RawMessage> JoinRequestMessage::toRawMessage()
{
	//$ TODO: Check to see that we have all the data we need
	Json::Value 	root;
	root["msgtype"] = msgtype;
	root["address"] = fromAddress.toString();
	root["heartbeat"] = (Json::Int64) heartbeat;

	return rawMessageFromJson(fromAddress, toAddress, root);
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

	Json::Value root = jsonFromRawMessage(raw);

	MEMBER_MSGTYPE msgtype = static_cast<MEMBER_MSGTYPE>(root.get("msgtype", 0).asInt());
	switch(msgtype) {
		case MEMBER_MSGTYPE::JOINREQ: {
				DEBUG_LOG(log, raw->toAddress, "JOINREQ received from %s", 
					raw->fromAddress.toString().c_str());	

				string saddr = root.get("address", "").asString();
				long hb = root.get("heartbeat", 0).asInt64();

				Address addr;
				addr.parse(saddr);
				node->member.addToMemberList(addr, par->getCurrtime(), hb);	

				log->logNodeAdd(connection->address(), addr);

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
	}
	else {
		// Send a JOINREQ to the coordinator	
		JoinRequestMessage 	joinReq(connection->address(), joinAddress, node->member.heartbeat);
		auto raw = joinReq.toRawMessage();

		DEBUG_LOG(log, connection->address(), "Trying to join...");

		connection->send(raw.get());
	}
}
