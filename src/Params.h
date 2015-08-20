/*****
 * Params.h
 *
 * See LICENSE for details.
 *
 * The Params class contains any parameters needed to run a simulation.
 *
 *****/

#ifndef NCLOUD_PARAMS_H_
#define NCLOUD_PARAMS_H_

#include "stdincludes.h"
#include "Params.h"

class Params
{
public:
    enum class TEST_TYPE { NONE, CREATE, READ, UPDATE, DELETE };
    
    Params();
    void load(const char *filename);

    string  coordinatorAddress;
    unsigned short coordinatorPort;

    // Begin - SIMULATION PARAMS
    // =====
    // formerly MAX_NNB
    int     maxNumberOfNeighbors;

    // formerly EN_GPSZ
    int     numberOfNodes;

    // formerly MAX_MSG_SIZE
    size_t  maxMessageSize;

    double  msgDropProbability; // formerly MSG_DROP_PROB

    // enableDropMessages enables the dropMessages scenarios
    // but messages are not actually dropped until
    // dropMessages is set to true;
    // formerly DROP_MSG
    bool    enableDropMessages;

    // formerly dropmsg
    bool    dropMessages;

    // formerly STEP_RATE
    double  stepRate;

    // formerly SINGLE_FAILURE
    bool    singleFailure;

    // formerly CRUDTEST
    TEST_TYPE   CRUDTestType;

    // Moved to be part of the MP2 Application
    //int   allNodesJoined;

    // =====
    // End - SIMULATION PORTION


    // Begin - RAFT PARAMS
    // =====

    // Timeout before starting a new election
    int     electionTimeout;

    // Idle timeout, this it the length of time between
    // idle periods.  This should be less than the electionTimeout
    // so that we are sure to send out heartbeats to prevent
    // unnecessary elections.
    // (defaut: 0)
    int     idleTimeout;

    // Length of time allowed for an RPC request. The RPC
    // will be resent if a reply is not receive within this timeout.
    // (default: 0)
    int     rpcTimeout;

    // The max number of entries that we can send in a snapshot
    // (if this is -1, then this is ignored).
    // (default: -1)
    int     maxSnapshotSize;

    // IF the number of logEntries exceeds this threshold, then the
    // log is snapshotted (if this is -1, then this is ignored)
    // (default: -1)
    int     logCompactionThreshold;

    // =====
    // End - RAFT PARAMS

    void    resetCurrtime() { starttime = time(nullptr); globaltime = 0; }
    int     getCurrtime() { return globaltime; }

    // Use this only in the simulation
    void    addToCurrtime(int inc) { globaltime += inc; }

    // Use this only in the socket app
    void    updateCurrtime() { globaltime = static_cast<int>(time(nullptr) - starttime); }

protected:
    time_t  starttime;
    int     globaltime;
};


#endif /* NCLOUD_PARAMS_H_ */
