/*****
 * Params.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "Params.h"
#include "Util.h"
#include "json/json.h"
#include "fstream"


/**
 * Constructor
 */
Params::Params():
	maxNumberOfNeighbors(0),
	numberOfNodes(0),
	maxMessageSize(4000),
	msgDropProbability(0),
	enableDropMessages(false),
	dropMessages(false),
	stepRate(0.25),
	singleFailure(false),
	CRUDTestType(TEST_TYPE::NONE),
	globaltime(0)
{}

/**
 * FUNCTION NAME: setparams
 *
 * DESCRIPTION: Set the parameters for this test case
 */
void Params::load(const char *config_file) {
	std::ifstream config_doc(config_file, std::ifstream::binary);
	if (config_doc.fail())
		throw AppException("cannot find the config_file");

	try {
		Json::Value	root;
		config_doc >> root;		// read in the root node

		coordinatorAddress = root.get("coordinatorAddress", "").asString();
		coordinatorPort = (unsigned short) root.get("coordinatorPort", 0).asInt();

		maxNumberOfNeighbors = root.get("maxNumberOfNodes", 0).asInt();
		enableDropMessages = root.get("enableDropMessages", false).asBool();
		singleFailure = root.get("singleFailure", false).asBool();
		msgDropProbability = root.get("messageDropProbability", 0).asDouble();

		string s = root.get("CRUDTest", "").asString();

		if (s == "CREATE")
			CRUDTestType = TEST_TYPE::CREATE;
		else if (s == "READ")
			CRUDTestType = TEST_TYPE::READ;
		else if (s == "UPDATE")
			CRUDTestType = TEST_TYPE::UPDATE;
		else if (s == "DELETE")
			CRUDTestType = TEST_TYPE::DELETE;

		numberOfNodes = maxNumberOfNeighbors;
	}
	catch(exception & e) {
		throw AppException(string_format("Error reading in the config file: %s : %s",
			config_file, e.what()).c_str());
	}
}
