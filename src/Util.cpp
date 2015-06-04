/*****
 * Util.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Util.h"
#include "Network.h"

unique_ptr<RawMessage> rawMessageFromJson(const Address &from,
										  const Address &to,
										  Json::Value root)
{
	Json::FastWriter writer;
	string data = writer.write(root);

	auto raw = make_unique<RawMessage>();
	unique_ptr<byte[]> temp(new byte[data.length()]);
	memcpy(temp.get(), data.data(), data.length());

	raw->fromAddress = from;
	raw->toAddress = to;
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

void write_chars_raw(ostream& os, const char *p, size_t length)
{
	os.write(p, length);
}

void read_chars_raw(istream& is, char *p, size_t length)
{
	is.read(p, length);
}

template<>
string read_raw(std::istream& is)
{
    //$ TODO: remove static buffer
    static char buf[1024];

    size_t len = static_cast<size_t>(read_raw<int>(is));
    assert(len < sizeof(buf));

    read_chars_raw(is, buf, len);
    return string(buf, len);
}

template<>
void write_raw(std::ostream& os, const string& value)
{
    write_raw<int>(os, value.size());
    write_chars_raw(os, value.c_str(), value.size());
}


