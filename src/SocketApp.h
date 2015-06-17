/*****
 * SocketApp.h
 *
 * See LICENSE for details.
 *
 * Contains main().  Also contains Application() which runs the event
 * loop.
 *
 *****/


#ifndef NCLOUD_SOCKETAPP_H
#define NCLOUD_SOCKETAPP_H

#include "stdincludes.h"
#include "Network.h"
#include "SocketNetwork.h"
#include "Log.h"
#include "Params.h"
#include "NetworkNode.h"
#include "MP1.h"
#include "MP2.h"


const int   ARGS_COUNT = 4;
const int   REPLICATION_FACTOR = 3;
const int   KEY_LENGTH = 5;


class Application
{
public:
    Application();
    ~Application();

    Application(const Application &) = delete;
    Application &operator= (const Application &) = delete;

    void init(const char *filename, const Address& baseAddress);
    void run();

protected:
    shared_ptr<SocketNetwork>   socketnet;
    Log *                   log;
    shared_ptr<NetworkNode> node;
    Params *                par;

    // The address of the coordinator node
    Address                 joinAddress;

};

#endif /* NCLOUD_SOCKETAPP_H */
