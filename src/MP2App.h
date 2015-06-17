/*****
 * MP2App.h
 *
 * See LICENSE for details.
 *
 * Contains main().  Also contains Application() which runs the event
 * loop.
 *
 *****/


#ifndef NCLOUD_MP2APP_H
#define NCLOUD_MP2APP_H

#include "stdincludes.h"
#include "Network.h"
#include "SimNetwork.h"
#include "Log.h"
#include "Params.h"
#include "NetworkNode.h"
#include "MP1.h"
#include "MP2.h"


const int   ARGS_COUNT = 2;
const int   TOTAL_RUNNING_TIME = 700;
const int   INSERT_TIME = TOTAL_RUNNING_TIME - 600;
const int   TEST_TIME = INSERT_TIME + 50;
const int   STABILIZE_TIME = 50;
const int   FIRST_FAIL_TIME = 25;
const int   LAST_FAIL_TIME = 10;
const int   REPLICATION_FACTOR = 3;
const int   NUMBER_OF_INSERTS = 100;
const int   KEY_LENGTH = 5;


const unsigned short MEMBER_PROTOCOL_PORT = 6000;
const unsigned short RING_PROTOCOL_PORT = 6001;
const unsigned int COORDINATOR_IP = 0x01000000; // 1.0.0.0


class Application
{
public:
    Application();
    ~Application();

    Application(const Application &) = delete;
    Application &operator= (const Application &) = delete;

    void init(const char *filename);
    void run();

    void mp1Run();
    void mp2Run();
    void fail();

protected:
    shared_ptr<SimNetwork>  simnetwork;
    Log *                   log;
    vector<shared_ptr<NetworkNode>> nodes;
    Params *                par;

    // Use this to keep track of how many nodes have
    // started up.  Once all nodes have started up, then we
    // can start up the ring protocol.
    int                     nodesStarted;

    map<string, string>     testKVPairs;

    // The address of the coordinator node
    Address                 joinAddress;

    void initTestKVPairs();
    shared_ptr<NetworkNode> randomAliveNode();

    void deleteTest();
    void readTest();
    void updateTest();
};

#endif /* NCLOUD_MP2APP_H */
