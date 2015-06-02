/*****
 * Log.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/


#include "Log.h"

Log::Log(Params *p)
{
	par = p;
}

Log::~Log()
{
	if (fpDebug != nullptr)
		fclose(fpDebug);
	fpDebug = nullptr;
}

// logs a line of debug output, prefixed by the address and then followed
// by the debug output.
//
void Log::log(const Address &address, const char *fmt, ...)
{
	static char	buffer[30000];

	// Lazily create the debug log
	if (firstTime)
	{
		fpDebug = fopen(DBG_LOG.c_str(), "w");
		firstTime = false;
	}

	string prefix = address.toString();

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	fprintf(fpDebug, "\n %s [%d] %s", prefix.c_str(), par->getCurrtime(), buffer);

	if (++numWrites >= MAX_WRITES)
	{
		fflush(fpDebug);
		numWrites = 0;
	}
}

void Log::logNodeAdd(const Address &thisAddress, const Address &addedNode)
{
	string stdstring = string_format("Node %s joined at time %d",
		addedNode.toString().c_str(), par->getCurrtime());
	log(thisAddress, stdstring.c_str());
}

void Log::logNodeRemove(const Address &thisAddress, const Address &removedNode)
{
	string stdstring = string_format("Node %s removed at time %d",
		removedNode.toString().c_str(), par->getCurrtime());
	log(thisAddress, stdstring.c_str());
}

