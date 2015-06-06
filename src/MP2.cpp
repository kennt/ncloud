/*****
 * MP1.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "MP2.h"
#include "NetworkNode.h"


// Initializes the message handler.  Call this before any calls to
// onMessageReceived() or onTimeout().  Or if the connection has been reset.
//
void MP2MessageHandler::start(const Address &joinAddress)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("");

	node->member.inited = true;

	// Initialize the ring (we should have a connection by this point)
	node->ring.init(node.get(), connection->address());
}

// This is a callback and is called when the connection has received
// a message.
//
// The RawMessage will not be changed with.
//
void MP2MessageHandler::onMessageReceived(const RawMessage *raw)
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

	//istringstream ss(std::string((const char *)raw->data.get(), raw->size));

	//RingMessageType msgtype = static_cast<RingMessageType>(read_raw<int>(ss));
	//ss.seekg(0, ss.beg);	// reset to start of the buffer

	// Handle messages here
	// This function should ensure
	//
	// This function should ensure that all READ and UPDATE operations
	// get QUORUM replies

}

// This is called when there are no messages available (usually on a
// connection timeout).  Thus perform any actions that should be done
// on idle here.
//
void MP2MessageHandler::onTimeout()
{
	// run the node maintenance loop
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

	// Wait until you're in the group
	if (!node->member.inited || !node->member.inGroup)
		return;

	// ...then jump in and share your responsibilties!
	//
	//$ CODE:  Your code goes here

	updateRing();
}

// This function does the following:
//	(1) Gets the current message list from the Membership protcol.
//		The membership list is returned as a vector of shared node ptrs.
//	(2) Constructs the ring based on the membership lists
//	(3)	Calls the stabilization protocol
//
void MP2MessageHandler::updateRing()
{
	auto node = netnode.lock();
	if (!node)
		throw AppException("Network has been deleted");

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
//	(1) Ensures that there are three "CORRECT" replicas of all the keys in
//		spite of failures and joins.
//		Note: "CORRECT" replicas implies that every key is replicated in its
//		two neighboring nodes in the ring.
//
void MP2MessageHandler::stabilizationProtocol()
{
	//
	// Implement this
	//
}

// Server-side CREATE API
// This function does the following:
//	(1) Inserts the key value into the local hash table
//	(2) Return true or false based on sucess or failure
//
bool MP2MessageHandler::createKeyValue(string key, string value, ReplicaType replica)
{
	//
	// Implement this
	//
	// Insert key, value, replica into the hash table
	//
	return false;
}

// Server-side READ API
// This function does the following:
//	(1) Read key from local hash table
//	(2) Return value
//
string MP2MessageHandler::readKey(string key)
{
	//
	// Implement this
	//
	// Read key from local hash table and return value
	return string();
}

// Server-side UPDATE API
// This function does the following:
//	(1) Update the key to the new value in the local hash table
//	(2) Return true or false based on success or failure
//
bool MP2MessageHandler::updateKeyValue(string key, string value, ReplicaType replica)
{
	//
	// Implement this
	//
	// Update key in the local hash table and return true or false
	//
	return false;
}

// Server-side DELETE API
// This function does the following:
//	(1) Delete the key from the local hash table
//	(2) Return true or false based on success or failure
//
bool MP2MessageHandler::deleteKey(string key)
{
	//
	// Implement this
	//
	// Delete the key from the local hash table
	//
	return false;
}