/*****
 * Member.h
 *
 * See LICENSE for details.
 *
 *****/


#ifndef NCLOUD_MEMBER_H
#define NCLOUD_MEMBER_H

#include "stdincludes.h"
#include "Log.h"
#include "Network.h"

struct MemberListEntry
{
	MemberListEntry()
		: timestamp(0), heartbeat(0)
	{}

	MemberListEntry(const Address& addr,
					int timestamp,
					long heartbeat)
		: address(addr), timestamp(timestamp), heartbeat(heartbeat)
	{}

	Address 	address;
	int 		timestamp;
	long 		heartbeat;
};


class MemberInfo
{
public:
	MemberInfo(Log *log, Params *par) :
		log(log),
		par(par),
		inGroup(false),
		inited(false)
	{
	}

	Log *					log;
	Params *				par;
	list<MemberListEntry> 	memberList;
	bool 					inGroup;
	bool 					inited;
	long					heartbeat;

	void addToMemberList(const Address &address, int timestamp, long heartbeat);
};



#endif /* NCLOUD_MEMBER_H */
