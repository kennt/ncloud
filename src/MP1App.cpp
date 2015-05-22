/*****
 * MP1App.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "MP1App.h"
#include "MP1.h"

int main(int argc, char *argv[])
{
	if (argc != ARGS_COUNT) {
		cout << "Configuration (i.e., *.conf) file is required" << endl;
		return FAILURE;
	}

	// If you want a deterministic scenario, then enter
	// a value here rather than time(NULL)
	srand((unsigned int)time(NULL));

	try {
		Application app;

		app.init(argv[1]);
		app.run();
	}
	catch (exception & e)
	{
		cout << e.what() << endl;
	}

	return SUCCESS;
}

Application::Application() :
	joinAddress(COORDINATOR_IP, MEMBER_PROTOCOL_PORT)
{
	log = nullptr;
	par = nullptr;
}

Application::~Application()
{
	simnetwork.reset();

	delete log;
	delete par;
}

void Application::init(const char *filename)
{
	par = new Params();
	par->load(filename);

	log = new Log(par);

	simnetwork = SimNetwork::createNetwork(par);

	// Create all of the network nodes
	// Addresses take the form X.0.0.0:6000
	// X goes from 1...N

	shared_ptr<INetwork> network(simnetwork);

	for (int i=0; i<par->numberOfNodes; i++) {
		Address 	addr(makeIPAddress(i+1, 0, 0, 0), MEMBER_PROTOCOL_PORT);
		string		name = string_format("Node %d:%d", i, MEMBER_PROTOCOL_PORT);

		auto networknode = make_shared<NetworkNode>(name, par, network);
		auto connection = network->create(addr);
		auto mp1handler = make_shared<MP1MessageHandler>(log, par, networknode, connection);

		networknode->registerHandler(NetworkNode::ConnectionType::MEMBER,
									 connection, mp1handler);

		nodes.push_back(networknode);

		log->log(addr, "APP NetworkNode created");
	}
}

void Application::run()
{
	for (; par->getCurrtime() < TOTAL_RUNNING_TIME; par->addToCurrtime(1)) {
		// run the main loop for each node
		mp1Run();

		// fail certain nodes
		fail();
	}

	simnetwork->writeMsgcountLog(MEMBER_PROTOCOL_PORT);

	// clean everything out
	nodes.clear();
}

void Application::mp1Run()
{
	// Receive all messages (places them into a queue)
	for (int i = 0; i < (int) nodes.size(); i++) {
		if (par->getCurrtime() > (int)(par->stepRate*i)) {
			nodes[i]->receiveMessages();
		}
	}

	// Process all current messages
	// This is split up into two steps so that messages
	// will always have at least a delay of 1 time unit.
	for (int i = (int)(nodes.size()-1); i >= 0; --i) {
		auto node = nodes[i];

		// Start up the nodes (not all the nodes start up together)
		if (par->getCurrtime() == (int)(par->stepRate * i)) {
			node->nodeStart(joinAddress, 0);
			cout << i << "-th introduced node is using:";
			auto conn = node->getConnection(NetworkNode::ConnectionType::MEMBER);
			if (conn != nullptr)
				cout << " " << conn->address();
            cout << endl;
		}
		// normal message handling after startup
		else if ((par->getCurrtime() > (int)(par->stepRate*i)) &&
				 !node->failed()) {
			// Pull messages off of the net and call the callbacks
			node->processQueuedMessages();
		}
	}
}

void Application::fail()
{
	Address 	addr;

	int removed;
	shared_ptr<IConnection> conn;

	if (par->enableDropMessages && par->getCurrtime() == 50) {
		par->dropMessages = true;
	}

	if (par->singleFailure && par->getCurrtime() == 100) {
		removed = rand() % par->numberOfNodes;

		conn = nodes[removed]->getConnection(NetworkNode::ConnectionType::MEMBER);
		DEBUG_LOG(log, conn->address(), "Node failed at time=%d", par->getCurrtime());

		nodes[removed]->fail();
	}
	else if (par->getCurrtime() == 100) {
		removed = rand() % par->numberOfNodes/2;

		for (int i = removed; i < removed + par->numberOfNodes/2; i++) {

			conn = nodes[i]->getConnection(NetworkNode::ConnectionType::MEMBER);
			DEBUG_LOG(log, conn->address(), "Node failed at time=%d", par->getCurrtime());

			nodes[i]->fail();
		}
	}

	if (par->enableDropMessages && par->getCurrtime() == 300) {
		par->dropMessages = false;
	}
}




