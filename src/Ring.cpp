/*****
 * Ring.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Ring.h"
#include "NetworkNode.h"

void RingInfo::init(NetworkNode *node, const Address& address)
{
	this->node = node;
	hashCode = hashFunction(address.toString());
}

// This function hashes the key and returns the position on the ring.
// THIS IS THE HASH FUNCTION USED FOR CONSISTENT HASHING.
// 
size_t RingInfo::hashFunction(const string key)
 {
 	std::hash<string> 	hashFunc;
 	return hashFunc(key) % RING_SIZE;
 }

// This is the client side CREATE API
// The function does the following:
//	(1) Constructs the message
//	(2) Finds the replicas of this key
//	(3) Sends a message to the replica
//
void RingInfo::clientCreate(string key, string value)
{
	//
	// Implement this
	//
}

// This is the client side READ API
// The function does the following:
//	(1) Constructs the message
//	(2) Finds the replicas of this key
//	(3) Sends a message to the replica
//
void RingInfo::clientRead(string key)
{
	//
	// Implement this
	//
}

// This is the client side UPDATE API
// The function does the following:
//	(1) Constructs the message
//	(2) Finds the replicas of this key
//	(3) Sends a message to the replica
//
void RingInfo::clientUpdate(string key, string value)
{
	//
	// Implement this
	//
}

// This is the client side DELETE API
// The function does the following:
//	(1) Constructs the message
//	(2) Finds the replicas for this key
//	(3) Sends a message to the replica
//
void RingInfo::clientDelete(string key)
{
	//
	// Implement this
	//
}

// Finds the replicas of the given key.
// This function requires that the variable be ring contain a list
// of member nodes.
//
vector<RingEntry> RingInfo::findNodes(const string key)
{
	size_t 	pos = hashFunction(key);
	vector<RingEntry> addressVector;

	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.front().hashcode || pos > ring.back().hashcode) {
			addressVector.emplace_back(ring.at(0));
			addressVector.emplace_back(ring.at(1));
			addressVector.emplace_back(ring.at(2));
		}
		else {
			for (int i=1; i<ring.size(); i++) {
				RingEntry entry = ring.at(i);
				if (pos <= entry.hashcode) {
					addressVector.emplace_back(entry);
					addressVector.emplace_back(ring.at((i+1) % RING_SIZE));
					addressVector.emplace_back(ring.at((i+2) % RING_SIZE));
					break;
				}
			}
		}
	}
	return addressVector;
}

// This function goes through the membership list from the membership protocol and
//	(1) Generates the hash code for each member
//	(2) Populates the ring member
// It returns a vector of RingEntry.  Each element contains:
//	(a) the address of the node (for the RING protocol)
//	(b) The hash code obtained by consistent hashing of the address
//
vector<RingEntry> RingInfo::getMembershipList()
{
	vector<RingEntry> curList;

	for (auto & entry : node->member.memberList) {
		RingEntry 	re;
		//$ TODO: The address of the ring protocol is always
		// one port after the membership protocol
		Address 	ringAddress(entry.address.getIPAddress(), 
								entry.address.getPort() + 1);

		re.address = ringAddress;
		re.hashcode = hashFunction(re.address.toString());

		curList.push_back(re);
	}

	return curList;
}
