/*****
 * MP1App.h
 *
 * See LICENSE for details.
 *
 * Contains main().  Also contains Application() which runs the event
 * loop.
 *
 *****/


#ifndef NCLOUD_MP1APP_H
#define NCLOUD_MP1APP_H

#include "stdincludes.h"
#include "Network.h"
#include "SimNetwork.h"
#include "Log.h"
#include "Params.h"
#include "NetworkNode.h"


const int	ARGS_COUNT = 2;
const int	TOTAL_RUNNING_TIME = 700;
const unsigned short MEMBER_PROTOCOL_PORT = 6000;
const unsigned int COORDINATOR_IP = 0x01000000;	// 1.0.0.0

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
	void fail();

protected:
	shared_ptr<SimNetwork>	simnetwork;
	Log *					log;
	vector<shared_ptr<NetworkNode>> nodes;
	Params *				par;

	// The address of the coordinator node
	Address 				joinAddress;
};

#endif /* NCLOUD_MP1APP_H */
