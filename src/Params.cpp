/**********************************
 * FILE NAME: Params.cpp
 *
 * DESCRIPTION: Definition of Parameter class
 **********************************/

#include "Params.h"

/**
 * Constructor
 */
Params::Params(): PORTNUM(8001) {}

/**
 * FUNCTION NAME: setparams
 *
 * DESCRIPTION: Set the parameters for this test case
 */
void Params::load(const char *config_file) {
	//trace.funcEntry("Params::setparams");
	char CRUD[10];
	FILE *fp = fopen(config_file,"r");

	fscanf(fp,"MAX_NNB: %d", &maxNumberOfNeighbors);
	fscanf(fp,"\nSINGLE_FAILURE: %d", &SINGLE_FAILURE);
	fscanf(fp,"\nDROP_MSG: %d", &DROP_MSG);
	fscanf(fp,"\nMSG_DROP_PROB: %lf", &MSG_DROP_PROB);
	fscanf(fp,"\nCRUD_TEST: %s", CRUD);

	if ( 0 == strcmp(CRUD, "CREATE") ) {
		this->CRUDTEST = CREATE_TEST;
	}
	else if ( 0 == strcmp(CRUD, "READ") ) {
		this->CRUDTEST = READ_TEST;
	}
	else if ( 0 == strcmp(CRUD, "UPDATE") ) {
		this->CRUDTEST = UPDATE_TEST;
	}
	else if ( 0 == strcmp(CRUD, "DELETE") ) {
		this->CRUDTEST = DELETE_TEST;
	}

	//printf("Parameters of the test case: %d %d %d %lf\n", MAX_NNB, SINGLE_FAILURE, DROP_MSG, MSG_DROP_PROB);
	numberOfNodes = maxNumberOfNeighbors;
	STEP_RATE=.25;
	MAX_MSG_SIZE = 4000;
	globaltime = 0;
	dropmsg = 0;
	allNodesJoined = 0;
	for ( unsigned int i = 0; i < numberOfNodes; i++ ) {
		allNodesJoined += i;
	}
	fclose(fp);
	//trace.funcExit("Params::setparams", SUCCESS);
	return;
}

/**
 * FUNCTION NAME: getcurrtime
 *
 * DESCRIPTION: Return time since start of program, in time units.
 * 				For a 'real' implementation, this return time would be the UTC time.
 */
//int Params::getCurrtime(){
//    return globaltime;
//}
