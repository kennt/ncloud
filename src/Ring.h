/*****
 * Ring.h
 *
 * See LICENSE for details.
 *
 * Encapsulate all MP2 related data here (particularly the hashtable data).
 * There is a RingInfo class which is stored in a NetworkNode. 
 *
 * Place information in here that needs to be accessed/manipulated by MP2.
 *
 *****/


#ifndef NCLOUD_RING_H
#define NCLOUD_RING_H

#include "stdincludes.h"
#include "Log.h"
#include "Network.h"

class NetworkNode;

class RingInfo
{
public:
	RingInfo(Log *log, Params *par) :
		log(log),
		par(par),
		inited(false)
	{
	}

	Log *					log;
	Params *				par;
	bool 					inited;

	// Each node can act as the client for a request.
	// This is the client side API.
	// The server APIS are handled in the MessageHandler (since that is where
	// they will receive the request).
	//
	void clientCreate(string key, string value);
	const string clientRead(string key);
	void clientUpdate(string key, string value);
	void clientDelete(string key);


	vector<shared_ptr<NetworkNode>> findNodes(const string key);
};



#endif /* NCLOUD_RING_H */
