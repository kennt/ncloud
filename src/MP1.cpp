
#include "MP1.h"
#include "NetworkNode.h"
#include "json/json.h"

unique_ptr<RawMessage> JoinRequestMessage::toRawMessage()
{
	//$ TODO: Check to see that we have all the data we need
	Json::Value 	root;
	root["msgtype"] = msgtype;
	root["address"] = fromAddress.toString();

	return rawMessageFromJson(fromAddress, toAddress, root);
}

unique_ptr<RawMessage> rawMessageFromJson(const Address &fromAddress,
										  const Address &toAddress,
										  Json::Value root)
{
	Json::FastWriter writer;
	string data = writer.write(root);

	auto raw = make_unique<RawMessage>();
	unique_ptr<unsigned char[]> temp(new unsigned char[data.length()]);
	memcpy(temp.get(), data.data(), data.length());

	raw->fromAddress = fromAddress;
	raw->toAddress = toAddress;
	raw->size = data.length();
	raw->data = std::move(temp);

	return raw;
}

Json::Value jsonFromRawMessage(const RawMessage *raw)
{
	Json::Value root;
	istringstream is((const char *) raw->data.get(), raw->size);
	is >> root;
	return root;	
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

	switch(msgtype) {
		case MEMBER_MSGTYPE::JOINREQ:
			DEBUG_LOG(log, raw->toAddress, "JOINREQ received from %s", 
				raw->fromAddress.toString().c_str());
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
		JoinRequestMessage 	joinReq(conn->address(), joinAddress);
		auto raw = joinReq.toRawMessage();

		DEBUG_LOG(log, conn->address(), "Trying to join...");

		conn->send(raw.get());
	}
}
