/*****
 * Member.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Member.h"

void MemberInfo::addToMemberList(const Address &address, int timestamp, long heartbeat)
{
    MemberListEntry entry = {address, timestamp, heartbeat};
    memberList.push_back(entry);
}

void MemberInfo::removeFromMemberList(const Address &address)
{
	for (auto it = memberList.begin(); it != memberList.end(); it++) {
		if (it->address == address) {
			memberList.erase(it);
			break;
		}
	}
}

bool MemberInfo::isMember(const Address& address) const
{
	for (auto & elem: memberList) {
		if (elem.address == address)
			return true;
	}
	return false;
}