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
struct Message;
struct CommandMessage;

struct RingEntry
{
	Address 	address;
	size_t		hashcode;
};


class RingInfo
{
public:
	RingInfo(Log *log, Params *par) :
		log(log),
		par(par),
		hashCode(0),
		node(nullptr)
	{
	}

	Log *					log;
	Params *				par;

	void init(NetworkNode *node, const Address& address);

	// The hash code for this node, computed using
	// the full address (ip:port) as key and hashFunction().
	size_t getHashcode()
	{ return hashCode; }

	size_t hashFunction(string key);

	// Each node can act as the client for a request.
	// This is the client side API.
	// The server APIS are handled in the MessageHandler (since that is where
	// they will receive the request).
	//
	void clientCreate(shared_ptr<CommandMessage> cmdmessage, string key, string value);
	void clientRead(shared_ptr<CommandMessage> cmdmessage, string key);
	void clientUpdate(shared_ptr<CommandMessage> cmdmessage, string key, string value);
	void clientDelete(shared_ptr<CommandMessage> cmdmessage, string key);

	// Find the addresses of the nodes that are resposible for the key
	// This is in the order of the replicas
	vector<Address> findReplicas(const string key);
	vector<RingEntry> getMembershipList();

	void updateRing();

protected:
	size_t 					hashCode;
	Address 				address;

	// This is a raw pointer since this object lives within the
	// NetworkNode object.
	NetworkNode * 			node;

	// Our current view of the DHT ring
	vector<RingEntry> 		ring;

	void stabilizationProtocol();
};



#endif /* NCLOUD_RING_H */
