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
    this->address = address;
    hashCode = hashFunction(address.toString());
}

// This function hashes the key and returns the position on the ring.
// THIS IS THE HASH FUNCTION USED FOR CONSISTENT HASHING.
// 
size_t RingInfo::hashFunction(const string key)
 {
    std::hash<string>   hashFunc;
    return hashFunc(key) % RING_SIZE;
 }

// This is the client side CREATE API
// The function does the following:
//  (1) Constructs the message
//  (2) Finds the replicas of this key
//  (3) Sends a message to the replica
//
void RingInfo::clientCreate(shared_ptr<CommandMessage> cmdmessage, string key, string value)
{
    //
    // Implement this
    //
}

// This is the client side READ API
// The function does the following:
//  (1) Constructs the message
//  (2) Finds the replicas of this key
//  (3) Sends a message to the replica
//
void RingInfo::clientRead(shared_ptr<CommandMessage> cmdmessage, string key)
{
    //
    // Implement this
    //
}

// This is the client side UPDATE API
// The function does the following:
//  (1) Constructs the message
//  (2) Finds the replicas of this key
//  (3) Sends a message to the replica
//
void RingInfo::clientUpdate(shared_ptr<CommandMessage> cmdmessage, string key, string value)
{
    //
    // Implement this
    //
}

// This is the client side DELETE API
// The function does the following:
//  (1) Constructs the message
//  (2) Finds the replicas for this key
//  (3) Sends a message to the replica
//
void RingInfo::clientDelete(shared_ptr<CommandMessage> cmdmessage, string key)
{
    //
    // Implement this
    //
}

// Finds the replicas of the given key.
// This function requires that the variable be ring contain a list
// of member nodes.
//
vector<Address> RingInfo::findReplicas(const string key)
{
    size_t  pos = hashFunction(key);
    vector<Address> addressVector;

    if (ring.size() >= 3) {
        // if pos <= min || pos > max, the leader is the min
        if (pos <= ring.front().hashcode || pos > ring.back().hashcode) {
            addressVector.emplace_back(ring.at(0).address);
            addressVector.emplace_back(ring.at(1).address);
            addressVector.emplace_back(ring.at(2).address);
        }
        else {
            for (int i=1; i<ring.size(); i++) {
                RingEntry entry = ring.at(i);
                if (pos <= entry.hashcode) {
                    addressVector.emplace_back(entry.address);
                    addressVector.emplace_back(ring.at((i+1) % ring.size()).address);
                    addressVector.emplace_back(ring.at((i+2) % ring.size()).address);
                    break;
                }
            }
        }
    }
    return addressVector;
}

// This function goes through the membership list from the membership protocol and
//  (1) Generates the hash code for each member
//  (2) Populates the ring member
// It returns a vector of RingEntry.  Each element contains:
//  (a) the address of the node (for the RING protocol)
//  (b) The hash code obtained by consistent hashing of the address
//
vector<RingEntry> RingInfo::getMembershipList()
{
    vector<RingEntry> curList;

    for (auto & entry : node->member.memberList) {
        RingEntry   re;
        //$ TODO: The address of the ring protocol is always
        // one port after the membership protocol
        Address     ringAddress(entry.address.getIPv4Address(), 
                                entry.address.getPort() + 1);

        re.address = ringAddress;
        re.hashcode = hashFunction(re.address.toString());

        curList.push_back(re);
    }

    return curList;
}

// This function does the following:
//  (1) Gets the current message list from the Membership protcol.
//      The membership list is returned as a vector of shared node ptrs.
//  (2) Constructs the ring based on the membership lists
//  (3) Calls the stabilization protocol
//
void RingInfo::updateRing()
{
    //
    // Implement this, parts of it are already implemented
    //
    vector<RingEntry> curMemberList;
    bool changed = false;

    //
    // Step 1: Get the current membership list from the Membership protocol
    //
    curMemberList = node->ring.getMembershipList();

    //
    // Step 2: Construct the ring
    //
    sort(curMemberList.begin(), curMemberList.end(),
        [](RingEntry & lhs, RingEntry & rhs) {
            return lhs.hashcode < rhs.hashcode;
        });

    //
    // Step 3: Run the stabilization protocol IF REQUIRED
    //
    // run the stabilization protocol if the hash table size is
    // greater than zero and if there has been a change in the ring.
}

// This runs the stabilization protocol in case of Node joins and leaves.
// It ensures that there are always 3 copies of all keys in the DHT at at
// all times. The function does the following:
//  (1) Ensures that there are three "CORRECT" replicas of all the keys in
//      spite of failures and joins.
//      Note: "CORRECT" replicas implies that every key is replicated in its
//      two neighboring nodes in the ring.
//
void RingInfo::stabilizationProtocol()
{
    //
    // Implement this
    //
}

