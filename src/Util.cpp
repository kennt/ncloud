
#include "Util.h"
#include "Network.h"

bool isLittleEndian()
{
	int n = 1;
	return (*(char *)&n == 1);
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
	istringstream is(std::string((const char *)raw->data.get(), raw->size));
	is >> root;
	return root;	
}

