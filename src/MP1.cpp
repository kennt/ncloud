
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
	Json::Value root = jsonFromRawMessage(raw);

	MEMBER_MSGTYPE msgtype = static_cast<MEMBER_MSGTYPE>(root.get("msgtype", 0).asInt());
	auto node = netnode.lock();
	auto conn = node->getConnection(NetworkNode::ConnectionType::MEMBER);

	switch(msgtype) {
		case MEMBER_MSGTYPE::JOINREQ: {
				DEBUG_LOG(log, raw->toAddress, "JOINREQ received from %s", 
					raw->fromAddress.toString().c_str());	

				string saddr = root.get("address", "").asString();
				Address addr;
				addr.parse(saddr);
				long hb = root.get("heartbeat", 0).asInt64();
				node->member.addToMemberList(addr, par->getCurrtime(), hb);	

				log->logNodeAdd(conn->address(), addr);
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
		throw AppException("");

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
		throw AppException("");

	auto conn = node->getConnection(NetworkNode::ConnectionType::MEMBER);
	if (!conn)
		throw AppException("");

	if (conn->address() == joinAddress) {
		// we are the first process to join the group
		// boot up the group

		DEBUG_LOG(log, conn->address(), "Starting up group...");
		node->member.inGroup = true;
	}
	else {
		// Send a JOINREQ to the coordinator	
		JoinRequestMessage 	joinReq(conn->address(), joinAddress, node->member.heartbeat);
		auto raw = joinReq.toRawMessage();

		DEBUG_LOG(log, conn->address(), "Trying to join...");

		conn->send(raw.get());
	}
}
