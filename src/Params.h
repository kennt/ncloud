/**********************************
 * FILE NAME: Params.h
 *
 * DESCRIPTION: Header file of Parameter class
 **********************************/

#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "stdincludes.h"
#include "Params.h"

enum testTYPE { CREATE_TEST, READ_TEST, UPDATE_TEST, DELETE_TEST };

/**
 * CLASS NAME: Params
 *
 * DESCRIPTION: Params class describing the test cases
 */
class Params{
public:
	int maxNumberOfNeighbors;	// formerly MAX_NNB
	int numberOfNodes;			// formerly EN_GPSZ

	int maxMessageSize;			// formerly MAX_MSG_SIZE

	double 	msgDropProbability;	// formerly MSG_DROP_PROB
	// enableDropMessages enables the dropMessages scenarios
	// but messages are not actually dropped until
	// dropMessages is set to true;
	bool 	enableDropMessages;	// formerly DROP_MSG
	bool 	dropMessages;		// formerly dropmsg

	double	stepRate;			// formerly STEP_RATE

	//int MAX_NNB;                // max number of neighbors
	int SINGLE_FAILURE;			// single/multi failure
	//double MSG_DROP_PROB;		// message drop probability
	//double STEP_RATE;		    // dictates the rate of insertion
	//int EN_GPSZ;			    // actual number of peers
	//int MAX_MSG_SIZE;
	//int DROP_MSG;
	//int dropmsg;
	int allNodesJoined;
	short PORTNUM;
	int CRUDTEST;
	Params();
	void load(const char *);
	//int getcurrtime();

	int getCurrtime() { return globaltime; }
	void addToCurrtime(int inc) { globaltime += inc; }

protected:
	int globaltime;
};

#endif /* _PARAMS_H_ */
