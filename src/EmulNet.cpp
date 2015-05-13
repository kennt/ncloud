
EmulNetConnection::EmulNetConnection(Params *par, EmulNet *emNet)
{
	//$ TODO: Check for valid values or par, emNet

	this->params = par;
	this->emNet = emNet;

	this->connection = NULL;
}

EmulNetConnection::~EmulNetConnection()
{
	cleanup();
}

void EmulNetConnection::init(const Address &myAddress)
{
	//$ TODO: raise an error if already inited, may have
	// too many connection objects
	this->myAddr = myAddress;

	this->connection = emNet->createConnection(this->myAddr);

}

void EmulNetConnection::cleanup()
{
	this->inited = false;
	this->running = false;

	// Who is responsible for the connection?
	if (this->connection != NULL)
		this->connection->close();
	delete this->connection;
	this->connection = NULL;
}

int EmulNetConnection::send(const Address& toAddr, string data)
{
	return this->send(toAddr, data.c_str(), data.length());
}

int EmulNetConnection::send(const Address &toAddr, unsigned char *data, size_t size)
{
	//$ TODO: what error code to return here?
	if (!inited || !running)
		return -1;
	int ret = this->connection->send(toAddr, data, size);
	if (ret == 0)
		msgsSent[par->getcurrtime()]++;
	return ret;
}

int EmulNetConnection::recv(Address *fromAddress, unsigned char **data, size_t *size, int timeout)
{
	// Call receive on the underlying buffer
	if (!inited || !running)
		return -1;
	int ret = this->connection->recv(fromAddress, data, size, timeout);
	return ret;
}