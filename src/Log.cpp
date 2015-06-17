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
    static char buffer[30000];

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

static string sCoordinator("coordinator");
static string sServer("server");
static string sSuccess("success");
static string sFail("fail");

void Log::logCreate(const Address& address,
                    bool isCoord,
                    bool isSuccess,
                    int transid,
                    string key,
                    string value)
{
    string  role;
    string  result;

    if (isCoord)
        role = sCoordinator;
    else
        role = sServer;

    if (isSuccess)
        result = sSuccess;
    else
        result = sFail;

    string s;
    s = string_format("%s: create %s at time %d, transID=%d, key=%s, value=%s",
                      role.c_str(),
                      result.c_str(),
                      par->getCurrtime(),
                      transid,
                      key.c_str(),
                      value.c_str());
    log(address, s.c_str());
}

void Log::logRead(const Address& address,
                  bool isCoord,
                  bool isSuccess,
                  int transid,
                  string key,
                  string value)
{
    string  role;

    if (isCoord)
        role = sCoordinator;
    else
        role = sServer;

    string s;
    if (isSuccess)
        s = string_format("%s: read success at time %d, transID=%d, key=%s, value=%s",
                          role.c_str(),
                          par->getCurrtime(),
                          transid,
                          key.c_str(),
                          value.c_str());
    else
        s = string_format("%s: read fail at time %d, transID=%d, key=%s",
                          role.c_str(),
                          par->getCurrtime(),
                          transid,
                          key.c_str());
    log(address, s.c_str());
}

void Log::logUpdate(const Address& address,
                    bool isCoord,
                    bool isSuccess,
                    int transid,
                    string key,
                    string value)
{
    string  role;
    string  result;

    if (isCoord)
        role = sCoordinator;
    else
        role = sServer;

    if (isSuccess)
        result = sSuccess;
    else
        result = sFail;

    string s;
    s = string_format("%s: update %s at time %d, transID=%d, key=%s, value=%s",
                      role.c_str(),
                      result.c_str(),
                      par->getCurrtime(),
                      transid,
                      key.c_str(),
                      value.c_str());
    log(address, s.c_str());
}

void Log::logDelete(const Address& address,
                    bool isCoord,
                    bool isSuccess,
                    int transid,
                    string key)
{
    string  role;
    string  result;

    if (isCoord)
        role = sCoordinator;
    else
        role = sServer;

    if (isSuccess)
        result = sSuccess;
    else
        result = sFail;

    string s;
    s = string_format("%s: delete %s at time %d, transID=%d, key=%s",
                      role.c_str(),
                      result.c_str(),
                      par->getCurrtime(),
                      transid,
                      key.c_str());
    log(address, s.c_str());    
}
