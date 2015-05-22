/**********************************
 * FILE NAME: Params.h
 *
 * DESCRIPTION: Header file of Parameter class
 **********************************/

#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "stdincludes.h"
#include "Params.h"

class Params
{
public:
	enum class TEST_TYPE { NONE, CREATE, READ, UPDATE, DELETE };
	
	Params();
	void load(const char *filename);

	// formerly MAX_NNB
	int 	maxNumberOfNeighbors;

	// formerly EN_GPSZ
	int 	numberOfNodes;

	// formerly MAX_MSG_SIZE
	size_t 	maxMessageSize;

	double 	msgDropProbability;	// formerly MSG_DROP_PROB

	// enableDropMessages enables the dropMessages scenarios
	// but messages are not actually dropped until
	// dropMessages is set to true;
	// formerly DROP_MSG
	bool 	enableDropMessages;

	// formerly dropmsg
	bool 	dropMessages;

	// formerly STEP_RATE
	double	stepRate;

	// formerly SINGLE_FAILURE
	bool 	singleFailure;

	// formerly CRUDTEST
	TEST_TYPE 	CRUDTestType;

	// Moved to be part of the MP2 Application
	//int 	allNodesJoined;

	void	resetCurrtime() { globaltime = 0; }
	int 	getCurrtime() { return globaltime; }
	void 	addToCurrtime(int inc) { globaltime += inc; }

protected:
	int globaltime;
};


#endif /* _PARAMS_H_ */
