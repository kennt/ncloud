
#include "Member.h"

void MemberInfo::addToMemberList(const Address &address, int timestamp, long heartbeat)
{
	MemberListEntry entry = {address, timestamp, heartbeat};
	memberList.push_back(entry);
}