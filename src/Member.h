/*****
 * Member.h
 *
 * See LICENSE for details.
 *
 *****/


#ifndef NCLOUD_MEMBER_H
#define NCLOUD_MEMBER_H

#include "stdincludes.h"
#include "Network.h"

struct MemberListEntry
{
	Address 	address;
	int 		timestamp;
	long 		heartbeat;
};


class MemberInfo
{
public:
	MemberInfo() :
		inGroup(false),
		inited(false)
	{
	}

	list<MemberListEntry> 	memberList;
	bool 					inGroup;
	bool 					inited;
	long					heartbeat;

	void addToMemberList(const Address &address, int timestamp, long heartbeat);
};



#endif /* NCLOUD_MEMBER_H */
