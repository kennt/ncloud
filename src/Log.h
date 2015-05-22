/*****
 * Log.h
 *
 * See LICENSE for details.
 *
 *****/


#ifndef NCLOUD_LOG_H
#define NCLOUD_LOG_H

#include "stdincludes.h"
#include "Params.h"
#include "Network.h"

const int 	 MAX_WRITES = 1;
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


#ifdef DEBUG

	#define DEBUG_LOG(log, address, msg, ...) log->log(address, msg, ##__VA_ARGS__)

#else

	#define DEBUG_LOG(log, name, msg, ...)

#endif


#endif 	/* NCLOUD_LOG_H */