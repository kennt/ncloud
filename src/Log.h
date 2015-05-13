#include "stdincludes.h"
#include "Params.h"
#include "Network.h"

#ifndef _LOG_H
#define _LOG_H


const int 	 MAX_WRITES = 1;
const string MAGIC_NUMBER = "CS425";
const string DBG_LOG = "dbg.log";
const string STATS_LOG = "stats.log";

class Log
{
public:
	Log(Params *p);

	// Don't allow any copying of the Log object
	Log(const Log &) = delete;
	Log& operator= (const Log &) = delete;

	~Log();

	void log(const Address &address, const char *fmt, ...);
	void logNodeAdd(const Address &thisNode, const Address &addedNode);
	void logNodeRemove(const Address &thisNode, const Address &removedNode);

protected:
	Params *par = nullptr;
	bool	firstTime = true;

	FILE *	fpDebug = nullptr;
	FILE *	fpStats = nullptr;
	int 	numWrites = 0;
};

#endif 	/* _LOG_H */