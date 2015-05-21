
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
	catch (exception *pe)
	{
		cout << pe->what() << endl;
	}

	return SUCCESS;
}

Application::Application() :
	joinAddress(COORDINATOR_IP, MEMBER_PROTOCOL_PORT)
{
	log = new Log(par);
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

	simnetwork = SimNetwork::createNetwork(par);

	// Create all of the network nodes
	// Addresses take the form X.0.0.0:6000
	// X goes from 1...N

	shared_ptr<INetwork> network(simnetwork);

	for (int i=0; i<par->numberOfNodes; i++) {
		Address 	addr(makeIPAddress(i+1, 0, 0, 0), MEMBER_PROTOCOL_PORT);

		auto networknode = make_shared<NetworkNode>(par, network);
		auto connection = network->create(addr);
		auto mp1handler = make_shared<MP1MessageHandler>(par, networknode);

		networknode->registerHandler(NetworkNode::ConnectionType::MEMBER, connection, mp1handler);

		nodes.push_back(networknode);

		log->log(addr, "APP NetworkNode created");
	}
}

void Application::run()
{
	for (; par->getCurrtime() < TOTAL_RUNNING_TIME; par->addToCurrtime(1)) {
		mp1Run();
		fail();
	}

	simnetwork->writeMsgcountLog(MEMBER_PROTOCOL_PORT);

	// clean everything out
	nodes.clear();
}

void Application::mp1Run()
{
	int     i;

	for (i=0; i<nodes.size(); i++) {
		if (par->getCurrtime() > (int)(par->stepRate*i))
			continue;

		auto node = nodes[i];
		if (node->failed)
			continue;

		// Pull messages off of the net and place onto the queue
		node->runReceiveLoop();
	}

	for (i=static_cast<int>(nodes.size()-1); i >= 0; --i) {
		auto node = nodes[i];

		if (par->getCurrtime() == (int)(par->stepRate * i)) {
			node->nodeStart(joinAddress, 0);
			cout << i << "-th introduced node is using:";
			for (auto & info: node->handlers) {
				cout << " " << info.second.connection->address().toString();
			}
            cout << endl;
		}
	}
}

void Application::fail()
{
}




