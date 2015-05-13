

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

	if (fpStats != nullptr)
		fclose(fpStats);
	fpStats = nullptr;
}

void Log::log(const Address &address, const char *fmt, ...)
{
	static char	buffer[30000];

	if (firstTime)
	{
		fpStats = fopen(STATS_LOG.c_str(), "w");
		fpDebug = fopen(DBG_LOG.c_str(), "w");

		int magicNumber = 0;
		string magic = MAGIC_NUMBER;
		int len = magic.length();
		for (int i = 0; i < len; i++)
			magicNumber += (int) magic.at(i);
		fprintf(fpDebug, "%x\n", magicNumber);

		firstTime = false;
	}

	string prefix = address.toString();

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (memcmp(buffer, "#STATSLOG#", 10) == 0) {
		fprintf(fpStats, "\n %s [%d] %s", prefix.c_str(), par->getCurrtime(), buffer);
	} else {
		fprintf(fpDebug, "\n %s [%d] %s", prefix.c_str(), par->getCurrtime(), buffer);
	}

	if (++numWrites >= MAX_WRITES)
	{
		fflush(fpStats);
		fflush(fpDebug);
		numWrites = 0;
	}
}

void Log::logNodeAdd(const Address &thisNode, const Address &addedNode)
{
	string stdstring = string_format("Node %s joined at time %d",
		addedNode.toString().c_str(), par->getCurrtime());
	log(thisNode, stdstring.c_str());
}

void Log::logNodeRemove(const Address &thisNode, const Address &removedNode)
{
	string stdstring = string_format("Node %s removed at time %d",
		removedNode.toString().c_str(), par->getCurrtime());
	log(thisNode, stdstring.c_str());
}

