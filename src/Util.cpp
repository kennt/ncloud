/*****
 * Util.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Util.h"
#include "Network.h"

unique_ptr<RawMessage> rawMessageFromJson(const Address &fromAddress,
										  const Address &toAddress,
										  Json::Value root)
{
	Json::FastWriter writer;
	string data = writer.write(root);

	auto raw = make_unique<RawMessage>();
	unique_ptr<byte[]> temp(new byte[data.length()]);
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
	istringstream is(std::string((const char *)raw->data.get(), raw->size));
	is >> root;
	return root;	
}

unique_ptr<RawMessage> rawMessageFromStream(const Address &from,
											const Address &to,
											stringstream &stream)
{
	auto raw = make_unique<RawMessage>();

	size_t dataSize = stream.tellp();
	const char * data = stream.str().c_str();
	unique_ptr<byte[]> temp(new byte[dataSize]);
	memcpy(temp.get(), data, dataSize);

	raw->fromAddress = from;
	raw->toAddress = to;
	raw->size = dataSize;
	raw->data = std::move(temp);

	return raw;
}
