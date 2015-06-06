/*****
 * Log.h
 *
 * See LICENSE for details.
 *
 * Implements a log class that will log to dbg.log.  There are special
 * functions to log specific operations (such as when a node is added or
 * removed).  This is necessary for the grader.
 *
 *****/


#ifndef NCLOUD_LOG_H
#define NCLOUD_LOG_H

#include "stdincludes.h"
#include "Params.h"
#include "Network.h"

// Number of writes before the log is flushed to the disk.
const int 	 MAX_WRITES = 1;

const string DBG_LOG = "dbg.log";

// See comment at the top of the file for details.
//
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

	// success
	void logCreate(const Address& addr, bool isCoord, bool isSuccess, int transid, string key, string value);
	void logRead(const Address& addr, bool isCoord, bool isSuccess, int transid, string key, string value);
	void logUpdate(const Address& addr, bool isCoord, bool isSuccess, int transid, string key, string newValue);
	void logDelete(const Address& addr, bool isCoord, bool isSuccess, int transid, string key);


protected:
	Params *par = nullptr;
	bool	firstTime = true;

	FILE *	fpDebug = nullptr;
	int 	numWrites = 0;
};


// Provide for debug output.  This helps to avoid having ifdef guards around
// debug output.
#ifdef DEBUG

	#define DEBUG_LOG(log, address, msg, ...) log->log(address, msg, ##__VA_ARGS__)

#else

	#define DEBUG_LOG(log, name, msg, ...)

#endif


#endif 	/* NCLOUD_LOG_H */