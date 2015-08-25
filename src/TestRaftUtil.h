/*****
 * TestRaftUtil.h
 *
 * See LICENSE for details.
 *
 * Some useful helper function/classes for building the
 * scenarios.
 *
 *****/

#ifndef NCLOUD_TESTRAFTUTIL_H
#define NCLOUD_TESTRAFTUTIL_H

#include "Network.h"
#include "Params.h"
#include "NetworkNode.h"
#include "Context.h"
#include "Raft.h"

using namespace Raft;

// A very simple class that is used to emulate a real node
// that keeps track of its log.
struct TermNode
{
	Address 		address;
	vector<TERM>	terms;

	TermNode(const Address& addr) : address(addr)
	{
		terms.push_back(0);
	}
};

void runMessageLoop(shared_ptr<NetworkNode> node, Params *par, int tm);
void runMessageLoop(vector<shared_ptr<NetworkNode>>& nodes, Params *par, int tm);
void runMessageLoopUntilSilent(shared_ptr<MockNetwork> network,
							   vector<shared_ptr<NetworkNode>>& nodes,
							   Params *par);
void runMessageLoopUntilActive(shared_ptr<MockNetwork> network,
							   vector<shared_ptr<NetworkNode>>& nodes,
							   Params *par);
void flushMessages(shared_ptr<IConnection> conn);

void sendAppendEntries(shared_ptr<IConnection> conn,
					   const Address& addr,
					   int transid,
					   TERM term,
					   const Address& leaderAddr,
					   INDEX lastIndex,
					   TERM lastTerm,
					   INDEX commitIndex,
					   Raft::RaftLogEntry *entry = nullptr);
void sendAppendEntriesReply(shared_ptr<IConnection> conn,
							const Address& addr,
							int transid,
							TERM term,
							bool success);
void sendInstallSnapshot(shared_ptr<IConnection> conn,
						 const Address& addr,
						 const Address& leader,
						 int transid,
						 TERM term,
						 INDEX lastIndex,
						 TERM lastTerm,
						 INDEX offset,
						 bool done,
						 const vector<Address>& addresses);
void sendInstallSnapshotReply(shared_ptr<IConnection> conn,
							  const Address& addr,
							  int transid,
							  TERM term);

Json::Value initializeStore(const Address& leader, const vector<Address>& nodes);
void append(Json::Value& value, TERM term, Command command, const Address& addr);

std::tuple<shared_ptr<NetworkNode>, shared_ptr<RaftHandler>> 
createNode(shared_ptr<MockNetwork> network,
           Params *par,
           Raft::StorageInterface *store,
           Raft::StorageInterface *snapshotStore,
           const Address& leaderAddr,
           const Address& nodeAddr,
           int timeoutModifier = 0);
void electLeader(shared_ptr<MockNetwork> network,
                 Params *par,
                 shared_ptr<NetworkNode> netnode,
                 const Address& leaderAddr,
                 const vector<Address>& nodes);
void handleNodeUpdates(shared_ptr<MockNetwork> network,
                       Params *par,
                       shared_ptr<NetworkNode> netnode,
                       const Address& leaderAddr,
                       const vector<Address>& nodes);
void handleNodeUpdates(shared_ptr<MockNetwork> network,
                       Params *par,
                       shared_ptr<NetworkNode> netnode,
                       const Address& leaderAddr,
                       vector<TermNode>& nodes);

std::tuple<shared_ptr<NetworkNode>, shared_ptr<RaftHandler>> 
createCluster(shared_ptr<MockNetwork> network,
			  Params *par,
			  Raft::StorageInterface *store,
			  Raft::StorageInterface *snapshotStore,
			  const Address& leaderAddr,
			  vector<TermNode>& nodes);

#endif /* NCLOUD_TESTRAFTUTIL_H */

