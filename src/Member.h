
#ifndef _MEMBER_H
#define _MEMBER_H

#include "stdincludes.h"

struct MemberListEntry
{
	Address 	address;
	int 		timestamp;
	int 		heartbeat;
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
};



#endif /* _MEMBER_H */
