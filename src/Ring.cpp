/*****
 * Ring.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Ring.h"

void RingInfo::clientCreate(string key, string value)
{
}

const string RingInfo::clientRead(string key)
{
	return string("");
}

void RingInfo::clientUpdate(string key, string value)
{
}

void RingInfo::clientDelete(string key)
{
}

vector<shared_ptr<NetworkNode>> RingInfo::findNodes(const string key)
{
	return vector<shared_ptr<NetworkNode>>();
}
