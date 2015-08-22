/*****
 * SocketApp.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "RaftSockApp.h"
#include "Raft.h"
#include "MP2.h"
#include "Command.h"

int main(int argc, char *argv[])
{
    if (argc != ARGS_COUNT) {
        cout << "Requires three parameters: config_file ip_address port_number";
        cout << endl;
        return FAILURE;
    }

    // We expect write failures to occur but we want to handle them where 
    // the error occurs rather than in a SIGPIPE handler.
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_IGN;

    sigaction(SIGPIPE, &act, NULL);


    // If you want a deterministic scenario, then enter
    // a value here rather than time(NULL)
    srand((unsigned int)time(NULL));

    try {
        Application app;
        Address     baseAddress;

        baseAddress.parse(argv[2], argv[3]);

        app.init(argv[1], baseAddress);
        app.run();
    }
    catch (exception & e)
    {
        cout << e.what() << endl;
    }

    return SUCCESS;
}

Application::Application()
    : store("raft.log")
{
    log = nullptr;
    par = nullptr;
}

Application::~Application()
{
    socketnet.reset();

    delete log;
    delete par;
}

void Application::init(const char *filename, const Address &base)
{
    par = new Params();
    par->load(filename);
    par->resetCurrtime();

    joinAddress.parse(par->coordinatorAddress.c_str(), par->coordinatorPort);

    log = new Log(par);

    socketnet = SocketNetwork::createNetwork(par);

    shared_ptr<INetwork> network(socketnet);

    string      name = string_format("Node %s", base.toString().c_str());
    node = make_shared<NetworkNode>(name, log, par, network);

    // Create the handlers for MP1
    auto connection = network->create(base);
    auto rafthandler = make_shared<Raft::RaftHandler>(log, par, &store, node, connection);
    node->registerHandler(ConnectType::MEMBER,
                          connection, rafthandler);
    cout << "Membership protocol address: " << base << endl;

    // Create the handlers for MP2
    Address     addr2(base.getIPv4Address(), base.getPort()+1);
    auto connection2 = network->create(addr2);
    auto mp2handler = make_shared<MP2MessageHandler>(log, par, node, connection2);
    node->registerHandler(ConnectType::RING,
                          connection2, mp2handler);
    cout << "DHT Ring protocol address: " << addr2 << endl;

    // Create the command handler
    Address     addr3(base.getIPv4Address(), base.getPort()+2);
    auto connection3 = network->create(addr3);
    auto cmdhandler = make_shared<CommandMessageHandler>(log, par, node, connection3);
    node->registerHandler(ConnectType::COMMAND,
                          connection3, cmdhandler);
    cout << "Command protocol address: " << addr3 << endl;
}

void Application::run()
{
    node->nodeStart(joinAddress, 1 /* timeout in secs */);
    cout << "node is using:";

    auto conn = node->getConnection(ConnectType::MEMBER);
    if (conn != nullptr)
        cout << " " << conn->address();
          cout << endl;

    while(!node->quitReceived() && !node->failed()) {
        par->updateCurrtime();
        node->receiveMessages();
        node->processQueuedMessages();
    }
    
    cout << "quit received? " << node->quitReceived() << endl;

    // dump statistics
    socketnet->writeMsgcountLog();
}




