/*****
 * Member.h
 *
 * See LICENSE for details.
 *
 * Encapsulate all MP1 related data here (particularly the Membership data).
 * There is a MemberInfo class which is stored in a NetworkNode.  Each MemberInfo
 * class contains a list of Members (each in a MemberListEntry).
 *
 * Place information in here that needs to be accessed/manipulated by MP2.
 *
 *****/


#ifndef NCLOUD_MEMBER_H
#define NCLOUD_MEMBER_H

#include "stdincludes.h"
#include "Log.h"
#include "Network.h"

// Holds information about the members held by a MemberInfo.
//
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

    Address     address;
    int         timestamp;
    long        heartbeat;
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

    Log *                   log;
    Params *                par;
    list<MemberListEntry>   memberList;
    bool                    inGroup;
    bool                    inited;
    long                    heartbeat;

    void addToMemberList(const Address &address, int timestamp, long heartbeat);
};



#endif /* NCLOUD_MEMBER_H */
