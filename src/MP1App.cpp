
#include "MP1App.h"

int main(int argc, char *argv[])
{
	if (argc != ARGS_COUNT) {
		cout << "Configuration (i.e., *.conf) file is required" << endl;
		return FAILURE;
	}

	// If you want a deterministic scenario, then enter
	// a value here rather than time(NULL)
	srand(time(NULL));

	Application app(argv[1]);

	app.init();
	app.run();

	return SUCCESS;
}

Application::Application(const char *filename)
{
	par = new Params();
	par->load(filename);

	log = new Log(par);
}

Application::~Application()
{
	simnetwork.reset();

	delete log;
	delete par;
}

void Application::init()
{
	simnetwork = SimNetwork::createNetwork(par);

	// Create all of the network nodes
	// Addresses take the form X.0.0.0:6000
	// X goes from 1...N

	shared_ptr<INetwork> network(simnetwork);

	for (int i=0; i<par->numberOfNodes; i++) {
		Address 	addr(makeIPAddress(i+1, 0, 0, 0), MEMBER_PROTOCOL_PORT);

		auto networknode = make_shared<NetworkNode>(par, network);
		auto connection = network->create(addr);
		auto handler = make_shared<MP1MessageHandler>(par, networknode);

		networknode->registerHandler(connection, handler);

		nodes.push_back(networknode);

		log->log(addr, "APP NetworkNode created");
	}
}

void Application::run()
{
	for (; par->getCurrtime() < TOTAL_RUNNING_TIME; par->addToCurrtime(1))
	{
		mp1Run();
		fail();
	}

	simnetwork->writeMsgcountLog(MEMBER_PROTOCOL_PORT);

	// clean everything out
	nodes.clear();
}

void Application::mp1Run()
{
}

void Application::fail()
{
}




