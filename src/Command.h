/*****
 * Command.h
 *
 * See LICENSE for details.
 *
 * This code implements the command protocol, this is the protocol that
 * communicates with clients (using json).
 *
 *****/


#ifndef NCLOUD_COMMAND_H
#define NCLOUD_COMMAND_H

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "NetworkNode.h"
#include "Member.h"


enum CommandType { CNONE = 0,
    CPING,          // ping the node
    CGETMEMBERS,    // retrieve the list of members
    CGETREPLICACOUNT, // retrieve replica counts of hashtabel entries
    CQUIT,          // tell the node to quit
    CCREATE,        // tell the node to create an entry
    CREAD,          // tell the node to read an entry
    CUPDATE,        // tell the node to update an entry
    CDELETE,        // tell the node to delete an entry

    // Raft Commands
    CRAFT_GETLEADER, // find what server this node thinks is the leader
    CRAFT_ADDSERVER, // Add a server to the membership
    CRAFT_REMOVESERVER, // Remove a server

    CREPLY = 999    // the reply for a command
};

class NetworkNode;

struct CommandMessage
{
public:

    // Create a reply to the current message
    shared_ptr<CommandMessage> makeReply(bool success);

    void load(const RawMessage *raw);
    unique_ptr<RawMessage> toRawMessage(const Address &from, const Address &to);

    CommandType     type;
    int             transId;

    // if type is CREPLY, then replytype contains the
    // type of the message that we are replying to.
    CommandType     replytype;

    string          key;
    string          value;
    bool            success;
    string          errmsg;

    // Used only for the CGETMEMBERS message
    list<MemberListEntry>   memberList;

    // Used only for the CGETREPLICACOUNT message
    vector<int>     counts;

    // Used for CRAFT_XXX messages
    Address         address;

    Address         to;
    Address         from;
};


// See comment above
//
class CommandMessageHandler: public IMessageHandler
{
public:
    CommandMessageHandler(Log *log, 
                          Params *par,
                          shared_ptr<NetworkNode> netnode,
                          shared_ptr<IConnection> connection)
        : log(log), par(par), netnode(netnode), connection(connection)
    {
    }

    virtual ~CommandMessageHandler() {}

    // Initializes the MessageHandler, if needed. This will be called
    // before onMessageReceived() or onTimeout() will be called.
    virtual void start(const Address &address) override;    

    // This is called when a message has been received.  This may be
    // called more than once for a timeslice.
    virtual void onMessageReceived(const RawMessage *) override;

    // Called when no messages are available (and the connection has timed out).
    virtual void onTimeout() override;

protected:
    Log *                   log;
    Params *                par;
    weak_ptr<NetworkNode>   netnode;
    shared_ptr<IConnection> connection;

};


#endif /* NCLOUD_COMMAND_H */

