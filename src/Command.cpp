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
    Json::Value root = jsonFromRawMessage(raw);

    // Fill in fields common to all messages
    this->type = static_cast<CommandType>(root.get("type", 0).asInt());
    this->from = raw->fromAddress;
    this->to = raw->toAddress;
    this->transid = root.get("transid", 0).asInt();

    switch(this->type) {
        case CommandType::CPING:
        case CommandType::CGETMEMBERS:
        case CommandType::CGETREPLICACOUNT:
        case CommandType::CQUIT:
            // Nothing else to do here
            break;
        case CommandType::CCREATE:
            {
                this->key = root.get("key", "").asString();
                this->value = root.get("value", "").asString();
            }
            break;
        case CommandType::CREAD:
            {
                this->key = root.get("key", "").asString();
            }
            break;
        case CommandType::CUPDATE:
            {
                this->key = root.get("key", "").asString();
                this->value = root.get("value", "").asString();
            }
            break;
        case CommandType::CDELETE:
            {
                this->key = root.get("key", "").asString();
            }
            break;
        default:
            throw NetworkException(string_format("Unknown command message:%d", this->type).c_str());
            break;
    }
}

unique_ptr<RawMessage> CommandMessage::toRawMessage(const Address &from, const Address& to)
{
    assert(this->type == CommandType::CREPLY);

    Json::Value     root;

    // fields common to all
    root["type"] = CommandType::CREPLY;
    root["replytype"] = this->replytype;
    root["transid"] = this->transid;
    root["success"] = this->success;
    if (!this->success)
        root["errmsg"] = this->errmsg;
    
    switch(this->replytype) {
        case CommandType::CPING:
        case CommandType::CQUIT:
        case CommandType::CCREATE:
        case CommandType::CUPDATE:
        case CommandType::CDELETE:
            // Nothing to do here
            break;
        case CommandType::CGETMEMBERS:
            {
                Json::Value members;
                for (auto & elem: this->memberList) {
                    Json::Value member;
                    member["a"] = elem.address.toAddressString();
                    member["p"] = elem.address.getPort();
                    member["ts"] = elem.timestamp;
                    member["hb"] = static_cast<unsigned int>(elem.heartbeat);
                    members.append(member);
                }
                root["members"] = members;
            }
            break;
        case CommandType::CGETREPLICACOUNT:
            {
                Json::Value counts;
                for (auto value : this->counts) {
                    counts.append(value);
                }
                root["counts"] = counts;
            }
            break;
        case CommandType::CREAD:
            if (this->success)
                root["value"] = this->value;
            break;
        default:
            throw NetworkException(string_format("Unknown reply type:%d", this->replytype).c_str());
            break;
    }

    return rawMessageFromJson(from, to, root);
}

shared_ptr<CommandMessage> CommandMessage::makeReply(bool success)
{
    auto reply = make_shared<CommandMessage>();
    reply->type = CommandType::CREPLY;
    reply->replytype = this->type;
    reply->transid = this->transid;
    reply->success = success;
    reply->to = this->from;
    reply->from = this->to;
    return reply;
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

    auto command = make_shared<CommandMessage>();
    shared_ptr<CommandMessage>  reply;

    command->load(raw);

    switch(command->type) {
        case CommandType::CPING:
            reply = command->makeReply(true);
            break;
        case CommandType::CQUIT:
            node->quit();
            reply = command->makeReply(true);
            break;
        case CommandType::CGETMEMBERS:
            // This is a success only if we are part of a group
            reply = command->makeReply(node->member.inGroup);
            if (!node->member.inGroup)
                reply->errmsg = "not a member of a group";
            reply->memberList = node->member.memberList;
            break;
        case CommandType::CGETREPLICACOUNT:
            // This is a success only if we are part of a group
            reply = command->makeReply(node->member.inGroup);
                reply->errmsg = "not a member of a group";
            reply->counts.clear();
            reply->counts.push_back(node->ring.getCount(ReplicaType::PRIMARY));
            reply->counts.push_back(node->ring.getCount(ReplicaType::SECONDARY));
            reply->counts.push_back(node->ring.getCount(ReplicaType::TERTIARY));
            break;
        case CommandType::CCREATE:
            node->ring.clientCreate(command, command->key, command->value);
            break;
        case CommandType::CREAD:
            node->ring.clientRead(command, command->key);
            break;
        case CommandType::CUPDATE:
            node->ring.clientUpdate(command, command->key, command->value);
            break;
        case CommandType::CDELETE:
            node->ring.clientDelete(command, command->key);
            break;
        default:
            break;
    }

    if (reply) {
        // Send the reply
        auto raw = reply->toRawMessage(reply->from, reply->to);
        connection->send(raw.get());
    }
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

