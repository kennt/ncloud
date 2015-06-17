/*****
 * Command.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Command.h"
#include "NetworkNode.h"

void CommandMessage::load(const RawMessage *raw)
{
}

unique_ptr<RawMessage> CommandMessage::toRawMessage(const Address &to, const Address& from)
{
    return nullptr;
}

shared_ptr<CommandMessage> CommandMessage::makeReply(bool success)
{
    return nullptr;
}

// Initializes the message handler.  Call this before any calls to
// onMessageReceived() or onTimeout().  Or if the connection has been reset.
//
void CommandMessageHandler::start(const Address &joinAddress)
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
void CommandMessageHandler::onMessageReceived(const RawMessage *raw)
{
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");
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
void CommandMessageHandler::onTimeout()
{
    // run the node maintenance loop
    auto node = netnode.lock();
    if (!node)
        throw AppException("Network has been deleted");
}

