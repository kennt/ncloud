/*****
 * MP1App.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "MP2App.h"
#include "MP2.h"

int main(int argc, char *argv[])
{
    if (argc != ARGS_COUNT) {
        cout << "Configuration (i.e., *.conf) file is required" << endl;
        return FAILURE;
    }

    // If you want a deterministic scenario, then enter
    // a value here rather than time(NULL)
    srand((unsigned int)time(NULL));

    try {
        Application app;

        app.init(argv[1]);
        app.run();
    }
    catch (exception & e)
    {
        cout << e.what() << endl;
    }

    return SUCCESS;
}

Application::Application() :
    nodesStarted(0),
    joinAddress(COORDINATOR_IP, MEMBER_PROTOCOL_PORT)
{
    log = nullptr;
    par = nullptr;
}

Application::~Application()
{
    simnetwork.reset();

    delete log;
    delete par;
}

void Application::init(const char *filename)
{
    par = new Params();
    par->load(filename);

    log = new Log(par);

    simnetwork = SimNetwork::createNetwork(par);

    // Create all of the network nodes
    // Addresses take the form X.0.0.0:6000
    // X goes from 1...N

    shared_ptr<INetwork> network(simnetwork);

    for (int i=0; i<par->numberOfNodes; i++) {
        string      name = string_format("Node %d", i);
        auto networknode = make_shared<NetworkNode>(name, log, par, network);

        // Create the handlers for MP1
        Address     addr(makeIPAddress(i+1, 0, 0, 0), MEMBER_PROTOCOL_PORT);
        auto connection = network->create(addr);
        auto mp1handler = make_shared<MP1MessageHandler>(log, par, networknode, connection);
        networknode->registerHandler(ConnectType::MEMBER,
                                     connection, mp1handler);

        // Create the handlers for MP2
        Address     addr2(makeIPAddress(i+1, 0, 0, 0), RING_PROTOCOL_PORT);
        auto connection2 = network->create(addr2);
        auto mp2handler = make_shared<MP2MessageHandler>(log, par, networknode, connection2);
        networknode->registerHandler(ConnectType::RING,
                                     connection2, mp2handler);

        nodes.push_back(networknode);

        log->log(addr, "APP NetworkNode created");
        log->log(addr2, "APP MP2 NetworkNode created");
    }
}

void Application::run()
{
    bool    allNodesJoined = false;

    for (; par->getCurrtime() < TOTAL_RUNNING_TIME; par->addToCurrtime(1)) {
        // run the main loop for each node
        mp1Run();

        if (nodesStarted == par->numberOfNodes && !allNodesJoined) {
            allNodesJoined = true;
        }
        else if (allNodesJoined) {
            mp2Run();
        }

        // fail certain nodes (MP1 only)
        //fail();
    }

    // dump statistics
    simnetwork->writeMsgcountLog(MEMBER_PROTOCOL_PORT);

    // clean everything out
    nodes.clear();
}

void Application::mp1Run()
{
    // Receive all messages (places them into a queue)
    for (int i = 0; i < (int) nodes.size(); i++) {
        if (par->getCurrtime() > (int)(par->stepRate*i)) {
            nodes[i]->receiveMessages();
        }
    }

    // Process all current messages
    // This is split up into two steps so that messages
    // will always have at least a delay of 1 time unit.
    for (int i = (int)(nodes.size()-1); i >= 0; --i) {
        auto node = nodes[i];
        if (node->failed())
            continue;

        // Start up the nodes (not all the nodes start up together)
        if (par->getCurrtime() == (int)(par->stepRate * i)) {
            node->nodeStart(joinAddress, 0);
            cout << i << "-th introduced node is using:";
            auto conn = node->getConnection(ConnectType::MEMBER);
            if (conn != nullptr)
                cout << " " << conn->address();
            cout << endl;
            nodesStarted++;
        }
        // normal message handling after startup
        else if ((par->getCurrtime() > (int)(par->stepRate*i))) {
            // Pull messages off of the net and call the callbacks
            node->processQueuedMessages();
        }
    }
}

void Application::mp2Run()
{
    // Runs the specific mp2 test code
    if (par->getCurrtime() == INSERT_TIME) {
        initTestKVPairs();
    }

    // Test CRUD operations
    if (par->getCurrtime() >= TEST_TIME) {
        if (par->getCurrtime() == TEST_TIME && 
            Params::TEST_TYPE::CREATE == par->CRUDTestType) {
            //
            // CREATE TEST
            //
            // TEST 1: Checks if there are REPLICATION_FACTOR * NUMBER_OF_INSERTS
            //         CREATE SUCCESS messages are in the log
            //
            cout << endl << "Doing create test at time: " << par->getCurrtime() << endl;
        }
        else if (par->getCurrtime() == TEST_TIME &&
                 Params::TEST_TYPE::DELETE == par->CRUDTestType) {
            //
            // DELETE TESTS
            //
            // TEST 1: NUMBER_OF_INSERTS/2 Key Value pair are deleted.
            //         Check whether REPLICATION_FACTOR * NUMBER_OF_INSERTS/2
            //         DELETE SUCCESS messages are in the log
            // TEST 2: Delete a non-existent key. Check for a DELETE FAIL
            //         message in the log
            deleteTest();
        }
        else if (par->getCurrtime() >= TEST_TIME &&
            Params::TEST_TYPE::READ == par->CRUDTestType) {
            //
            // READ TESTS
            //
            // TEST 1: Read a key. Check for correct value being read
            //         in quorum of replicas
            //
            // Wait for some time after TEST 1
            //
            // TEST 2: Fail a single replica of a key. Check for correct value
            //         of the key being read in quorum of replicas
            //
            // Wait for STABILIZE_TIME after TEST 2 (stabilization protocol
            // should ensure at least 3 replicas for all keys at all times)
            //
            // TEST 3 part 1: Fail two replicas of a key. Read the key and
            //                check for READ FAIL message in the log.
            //                READ should fail because quorum replicas
            //                of the key are not up
            //
            // Wait for another STABILIZE_TIME after TEST 3 part 1
            // (stabilization protocol should ensure at least 3 replicas
            // for all keys at all times)
            //
            // TEST 3 part 2: Read the same key as TEST 3 part 1.
            //                Check for correct value of the key
            //                being read in quorum of replicas
            //
            // Wait for some time after TEST 3 part 2
            //
            // TEST 4: Fail a non-replica. Check for correct value of the key
            //         being read in quorum of replicas
            //
            // TEST 5: Read a non-existent key. Check for a READ FAIL message
            //         in the log
            // 
            readTest();
        }
        else if (par->getCurrtime() >= TEST_TIME &&
                 Params::TEST_TYPE::UPDATE == par->CRUDTestType) {
            //
            // UPDATE TESTS
            //
            // TEST 1: Update a key. Check for correct new value being updated
            //         in quorum of replicas
            //
            // Wait for some time after TEST 1
            //
            // TEST 2: Fail a single replica of a key. Update the key.
            //         Check for correct new value of the key
            //         being updated in quorum of replicas
            //
            // Wait for STABILIZE_TIME after TEST 2 (stabilization protocol
            // should ensure at least 3 replicas for all keys at all times)
            //
            // TEST 3 part 1: Fail two replicas of a key. Update the key and
            //                check for READ FAIL message in the log
            //                UPDATE should fail because quorum replicas
            //                of the key are not up
            //
            // Wait for another STABILIZE_TIME after TEST 3 part 1
            // (stabilization protocol should ensure at least
            // 3 replicas for all keys at all times)
            //
            // TEST 3 part 2: Update the same key as TEST 3 part 1.
            //                Check for correct new value of the key
            //                being update in quorum of replicas
            //
            // Wait for some time after TEST 3 part 2
            //
            // TEST 4: Fail a non-replica. Check for correct new value of the key
            //         being updated in quorum of replicas
            //
            // TEST 5: Update a non-existent key. Check for a UPDATE FAIL
            //         message in the log
            //
            updateTest();
        }
    }
}

shared_ptr<NetworkNode> Application::randomAliveNode()
{
    int number = -1;

    //$ TODO: Hmmm.. this can get into an infinite loop if all
    // of the nodes are dead.
    do {
        number = (rand() % par->numberOfNodes);
    } while (nodes[number]->failed());

    return nodes[number];
}

void Application::initTestKVPairs()
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";
    string  key;

    testKVPairs.clear();

    while (testKVPairs.size() < NUMBER_OF_INSERTS) {
        key.clear();
        for (int i=0; i<KEY_LENGTH; i++)
            key.push_back( alphanum[rand() % (sizeof(alphanum)-1)] );

        if (testKVPairs.count(key) != 0)
            continue;

        string value = "value" + to_string(rand() % NUMBER_OF_INSERTS);

        testKVPairs[key] = value;

        shared_ptr<NetworkNode> node = randomAliveNode();
        log->log(node->getConnection(ConnectType::RING)->address(),
                 "CREATE OPERATION KEY: %s VALUE: %s at time: %d",
                    key.c_str(), value.c_str(), par->getCurrtime());
        node->ring.clientCreate(key, value);
    }
}

void Application::deleteTest()
{
    //
    // Test 1: Delete half of the KV pairs
    cout << endl << "Deleting " << testKVPairs.size()/2
         << " valid keys.... ... .. . ." << endl;

    auto it = testKVPairs.begin();
    for ( int i = 0; i < testKVPairs.size()/2; i++ ) {
        it++;

        // Step 1.a. Find a node that is alive
        shared_ptr<NetworkNode> node = randomAliveNode();

        // Step 1.b. Issue a delete operation
        log->log(node->getConnection(ConnectType::RING)->address(),
                 "DELETE OPERATION KEY: %s VALUE: %s at time: %d",
                    it->first.c_str(), it->second.c_str(), par->getCurrtime());
        node->ring.clientDelete(it->first);
    }

    //
    // Test 2: Delete a non-existent key
    //
    {
        cout << endl << "Deleting an invalid key.... ... .. . ." << endl;
        string invalidKey = "invalidKey";

        // Step 2.a. Find a node that is alive
        shared_ptr<NetworkNode> node = randomAliveNode();

        // Step 2.b. Issue a delete operation
        log->log(node->getConnection(ConnectType::RING)->address(),
                 "DELETE OPERATION KEY: %s at time: %d",
                 invalidKey.c_str(), par->getCurrtime());
        node->ring.clientDelete(invalidKey);
    }
}

void Application::readTest()
{
    // Step 0. Key to be read
    // This key is used for all read tests
    auto firstPair = testKVPairs.begin();
    vector<Address> replicas;
    int replicaIdToFail = TERTIARY;
    shared_ptr<NetworkNode> nodeToFail;
    bool failedOneNode = false;

    //
    // Test 1: Test if value of a single read operation is read correctly
    // in quorum number of nodes.
    //
    if ( par->getCurrtime() == TEST_TIME ) {
        // Step 1.a. Find a node that is alive
        shared_ptr<NetworkNode> node = randomAliveNode();

        // Step 1.b Do a read operation
        cout << endl << "Reading a valid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "READ OPERATION KEY: %s VALUE: %s at time: %d",
                 firstPair->first.c_str(), firstPair->second.c_str(), par->getCurrtime());
        node->ring.clientRead(firstPair->first);
    }

    // End of Test 1

    //
    // Test 2: FAIL ONE REPLICA. Test if value is read correctly in
    //         quorum number of nodes after ONE OF THE REPLICAS IS FAILED
    //
    if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME) ) {
        // Step 2.a Find a node that is alive
        auto node = randomAliveNode();

        // Step 2.b Find the replicas of this key
        replicas.clear();
        replicas = node->ring.findReplicas(firstPair->first);

        // if less than quorum replicas are found then exit
        //$ CHECK: Should this be REPLICATION_FACTOR or REPLICATION_FACTOR-1 ??
        if ( replicas.size() < (REPLICATION_FACTOR-1) ) {
            cout << endl
                 << "Could not find at least quorum replicas for this key. "
                 << "Exiting!!! size of replicas vector: " << replicas.size()
                 << endl;
            log->log(node->address(ConnectType::RING),
                     "Could not find at least quorum replicas for this key. "
                     "Exiting!!! size of replicas vector: %d", replicas.size());
            exit(1);
        }

        // Step 2.c Fail a replica
        for (auto & node : nodes) {
            if (node->address(ConnectType::RING) == replicas.at(replicaIdToFail)) {
                if ( !node->failed() ) {
                    nodeToFail = node;
                    failedOneNode = true;
                    break;
                }
                else {
                    // Since we fail at most two nodes, one of the replicas must be alive
                    if ( replicaIdToFail > 0 ) {
                        replicaIdToFail--;
                    }
                    else {
                        failedOneNode = false;
                    }
                }
            }
        }

        if ( failedOneNode ) {
            log->log(nodeToFail->getConnection(ConnectType::RING)->address(),
                     "Node failed at time=%d", par->getCurrtime());
            nodeToFail->fail();
            cout << endl << "Failed a replica node" << endl;
        }
        else {
            // The code can never reach here
            log->log(node->getConnection(ConnectType::RING)->address(),
                     "Could not fail a node");
            cout << "Could not fail a node. Exiting!!!";
            exit(1);
        }

        node = randomAliveNode();

        // Step 2.d Issue a read
        cout << endl << "Reading a valid key.... ... .. . ." << endl;
        log->log(node->getConnection(ConnectType::RING)->address(),
                 "READ OPERATION KEY: %s VALUE: %s at time: %d",
                 firstPair->first.c_str(), firstPair->second.c_str(), par->getCurrtime());
        node->ring.clientRead(firstPair->first);

        failedOneNode = false;
    }

    // end of Test 2

    //
    // Test 3 part 1: Fail two replicas. Test if value is read correctly
    //                in quorum number of nodes after TWO OF THE REPLICAS ARE FAILED
    //
    // Wait for STABILIZE_TIME and fail two replicas
    //
    if ( par->getCurrtime() >= (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME) ) {
        vector<shared_ptr<NetworkNode>> nodesToFail;
        int count = 0;

        if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME) ) {
            // Step 3.a. Find a node that is alive
            auto node = randomAliveNode();

            // Get the keys replicas
            replicas.clear();
            replicas = node->ring.findReplicas(firstPair->first);

            // Step 3.b. Fail two replicas
            //cout<<"REPLICAS SIZE: "<<replicas.size();
            if ( replicas.size() > 2 ) {
                replicaIdToFail = TERTIARY;
                while ( count != 2 ) {
                    // $ CHECK: Could this get into an infinite loop
                    // if one of the nodes is already down? In which case
                    // count will never reach 2 (especially if the
                    // REPLICATION_FACTOR is 3).
                    for (auto & inode : nodes) {
                        if (inode->address(ConnectType::RING) == replicas.at(replicaIdToFail)) {
                            if (!inode->failed()) {
                                nodesToFail.emplace_back(inode);
                                replicaIdToFail--;
                                count++;
                                break;
                            }
                            else {
                                // Since we fail at most two nodes, one of the replicas must be alive
                                if ( replicaIdToFail > 0 ) {
                                    replicaIdToFail--;
                                }
                            }
                        }
                    }
                }
            }
            else {
                // If the code reaches here. Test your stabilization protocol
                cout << endl
                     << "Not enough replicas to fail two nodes. Number of replicas of this key: "
                     << replicas.size() << ". Exiting test case !! " << endl;
                exit(1);
            }

            if ( count == 2 ) {
                for (auto & nodeFail : nodesToFail) {
                    // Fail a node
                    log->log(nodeFail->address(ConnectType::RING),
                             "Node failed at time=%d", par->getCurrtime());
                    nodeFail->fail();
                    cout << endl << "Failed a replica node" << endl;
                }
            }
            else {
                // The code can never reach here
                log->log(node->address(ConnectType::RING), "Could not fail two nodes");
                //cout<<"COUNT: " <<count;
                cout << "Could not fail two nodes. Exiting!!!";
                exit(1);
            }

            node = randomAliveNode();

            // Step 3.c Issue a read
            cout << endl << "Reading a valid key.... ... .. . ." << endl;
            log->log(node->address(ConnectType::RING),
                     "READ OPERATION KEY: %s VALUE: %s at time: %d",
                     firstPair->first.c_str(), firstPair->second.c_str(), par->getCurrtime());
            // This read should fail since at least quorum nodes are not alive
            node->ring.clientRead(firstPair->first);
        }

        /**
         * TEST 3 part 2: After failing two replicas and waiting for STABILIZE_TIME, issue a read
         */
        // Step 3.d Wait for stabilization protocol to kick in
        if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME + STABILIZE_TIME) ) {
            auto node = randomAliveNode();

            // Step 3.e Issue a read
            cout << endl << "Reading a valid key.... ... .. . ." << endl;
            log->log(node->address(ConnectType::RING), "Starting Test 3.2");
            log->log(node->address(ConnectType::RING),
                     "READ OPERATION KEY: %s VALUE: %s at time: %d",
                     firstPair->first.c_str(), firstPair->second.c_str(),
                     par->getCurrtime());
            // This read should be successful
            node->ring.clientRead(firstPair->first);
        }
    }

    /** end of test 3 **/

    /**
     * Test 4: FAIL A NON-REPLICA. Test if value is read correctly in quorum number of nodes after a NON-REPLICA IS FAILED
     */
    if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME + STABILIZE_TIME + LAST_FAIL_TIME ) ) {
        // Step 4.a. Find a node that is alive
        auto node = randomAliveNode();

        // Step 4.b Find a non - replica for this key
        replicas.clear();
        replicas = node->ring.findReplicas(firstPair->first);
        for (auto & inode : nodes) {
            if (inode->failed())
                continue;

            Address addr = node->address(ConnectType::RING);

            if (addr != replicas.at(PRIMARY) &&
                addr != replicas.at(SECONDARY) &&
                addr != replicas.at(TERTIARY)) {
                // Step 4.c Fail a non-replica node
                log->log(addr, "Node failed at time=%d", par->getCurrtime());
                inode->fail();
                failedOneNode = true;
                cout << endl << "Failed a non-replica node" << endl;
                break;
                }
        }
        if ( !failedOneNode ) {
            // The code can never reach here
            log->log(node->address(ConnectType::RING), "Could not fail a node(non-replica)");
            cout << "Could not fail a node(non-replica). Exiting!!!";
            exit(1);
        }

        node = randomAliveNode();

        // Step 4.d Issue a read operation
        cout << endl << "Reading a valid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "READ OPERATION KEY: %s VALUE: %s at time: %d",
                 firstPair->first.c_str(), firstPair->second.c_str(),
                 par->getCurrtime());
        // This read should fail since at least quorum nodes are not alive
        node->ring.clientRead(firstPair->first);
    }

    /** end of test 4 **/

    /**
     * Test 5: Read a non-existent key.
     */
    if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME + STABILIZE_TIME + LAST_FAIL_TIME ) ) {
        string invalidKey = "invalidKey";

        // Step 5.a Find a node that is alive
        auto node = randomAliveNode();

        // Step 5.b Issue a read operation
        cout << endl << "Reading an invalid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "READ OPERATION KEY: %s at time: %d",
                 invalidKey.c_str(), par->getCurrtime());
        // This read should fail since at least quorum nodes are not alive
        node->ring.clientRead(invalidKey);
    }

    /** end of test 5 **/

}


void Application::updateTest()
{
    // Step 0. Key to be updated
    // This key is used for all update tests
    auto kvEntry = testKVPairs.begin();
    kvEntry++;
    string newValue = "newValue";
    vector<Address> replicas;
    int replicaIdToFail = TERTIARY;
    shared_ptr<NetworkNode> nodeToFail;
    bool failedOneNode = false;

    //
    // Test 1: Test if value is updated correctly in quorum number of nodes
    //
    if ( par->getCurrtime() == TEST_TIME ) {
        // Step 1.a. Find a node that is alive
        auto node = randomAliveNode();

        // Step 1.b Do a update operation
        cout << endl << "Updating a valid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "UPDATE OPERATION KEY: %s VALUE: %s at time: %d",
                 kvEntry->first.c_str(), newValue.c_str(), par->getCurrtime());
        node->ring.clientUpdate(kvEntry->first, newValue);
    }

    // end of test 1

    //
    // Test 2: FAIL ONE REPLICA. Test if value is updated correctly
    //         in quorum number of nodes after ONE OF THE REPLICAS IS FAILED
    //
    if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME) ) {
        // Step 2.a Find a node that is alive and assign it as number
        auto node = randomAliveNode();

        // Step 2.b Find the replicas of this key
        replicas.clear();
        replicas = node->ring.findReplicas(kvEntry->first);

        // if quorum replicas are not found then exit
        //$ CHECK: should this be <= REPLICATION_FACTOR-1 ?
        if ( replicas.size() < REPLICATION_FACTOR-1 ) {
            log->log(node->address(ConnectType::RING),
                     "Could not find at least quorum replicas for this key."
                     "Exiting!!! size of replicas vector: %d", replicas.size());
            cout << endl
                 << "Could not find at least quorum replicas for this key. Exiting!!!"
                    " size of replicas vector: "
                 << replicas.size() << endl;
            exit(1);
        }

        // Step 2.c Fail a replica
        for (auto & inode : nodes) {
            if (inode->address(ConnectType::RING) == replicas.at(replicaIdToFail)) {
                if (!inode->failed()) {
                    nodeToFail = inode;
                    failedOneNode = true;
                    break;
                }
                else {
                    // Since we fail at most two nodes, one of the replicas must be alive
                    if ( replicaIdToFail > 0 ) {
                        replicaIdToFail--;
                    }
                    else {
                        failedOneNode = false;
                    }
                }
            }
        }
        if ( failedOneNode ) {
            log->log(nodeToFail->address(ConnectType::RING), 
                     "Node failed at time=%d", par->getCurrtime());
            nodeToFail->fail();
            cout << endl << "Failed a replica node" << endl;
        }
        else {
            // The code can never reach here
            log->log(node->address(ConnectType::RING), "Could not fail a node");
            cout << "Could not fail a node. Exiting!!!";
            exit(1);
        }

        node = randomAliveNode();

        // Step 2.d Issue a update
        cout << endl << "Updating a valid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "UPDATE OPERATION KEY: %s VALUE: %s at time: %d",
                 kvEntry->first.c_str(), newValue.c_str(), par->getCurrtime());
        node->ring.clientUpdate(kvEntry->first, newValue);

        failedOneNode = false;
    }

    // end of test 2

    //
    // Test 3 part 1: Fail two replicas. Test if value is updated correctly
    //                in quorum number of nodes after TWO OF THE REPLICAS ARE FAILED
    //
    if ( par->getCurrtime() >= (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME) ) {
        vector<shared_ptr<NetworkNode>> nodesToFail;
        int count = 0;

        if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME) ) {
            // Step 3.a. Find a node that is alive
            auto node = randomAliveNode();

            // Get the keys replicas
            replicas.clear();
            replicas = node->ring.findReplicas(kvEntry->first);

            // Step 3.b. Fail two replicas
            if ( replicas.size() > 2 ) {
                replicaIdToFail = TERTIARY;
                while ( count != 2 ) {
                    for (auto & inode : nodes) {
                        if (inode->address(ConnectType::RING) == replicas.at(replicaIdToFail)) {
                            if (!inode->failed()) {
                                nodesToFail.emplace_back(inode);
                                replicaIdToFail--;
                                count++;
                                break;
                            }
                            else {
                                // Since we fail at most two nodes, one of the replicas must be alive
                                if ( replicaIdToFail > 0 ) {
                                    replicaIdToFail--;
                                }
                            }
                        }
                    }
                }
            }
            else {
                // If the code reaches here. Test your stabilization protocol
                cout << endl
                     << "Not enough replicas to fail two nodes. Exiting test case !!"
                     << endl;
            }
            if ( count == 2 ) {
                for (auto & failNode : nodesToFail) {
                    // Fail a node
                    log->log(failNode->address(ConnectType::RING),
                             "Node failed at time=%d", par->getCurrtime());
                    failNode->fail();
                    cout << endl << "Failed a replica node" << endl;
                }
            }
            else {
                // The code can never reach here
                log->log(node->address(ConnectType::RING), "Could not fail two nodes");
                cout << "Could not fail two nodes. Exiting!!!";
                exit(1);
            }

            node = randomAliveNode();

            // Step 3.c Issue an update
            cout << endl << "Updating a valid key.... ... .. . ." << endl;
            log->log(node->address(ConnectType::RING),
                     "UPDATE OPERATION KEY: %s VALUE: %s at time: %d",
                     kvEntry->first.c_str(), newValue.c_str(), par->getCurrtime());
            // This update should fail since at least quorum nodes are not alive
            node->ring.clientUpdate(kvEntry->first, newValue);
        }

        //
        // TEST 3 part 2: After failing two replicas and waiting for STABILIZE_TIME,
        // issue an update
        //
        // Step 3.d Wait for stabilization protocol to kick in
        if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME + STABILIZE_TIME) ) {
            auto node = randomAliveNode();

            // Step 3.e Issue a update
            cout << endl << "Updating a valid key.... ... .. . ." << endl;
            log->log(node->address(ConnectType::RING),
                     "UPDATE OPERATION KEY: %s VALUE: %s at time: %d",
                     kvEntry->first.c_str(), newValue.c_str(), par->getCurrtime());
            // This update should be successful
            node->ring.clientUpdate(kvEntry->first, newValue);
        }
    }

    // end of test 3

    //
    // Test 4: FAIL A NON-REPLICA. Test if value is read correctly in
    // quorum number of nodes after a NON-REPLICA IS FAILED
    //
    if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME + STABILIZE_TIME + LAST_FAIL_TIME ) ) {
        // Step 4.a. Find a node that is alive
        auto node = randomAliveNode();

        // Step 4.b Find a non - replica for this key
        replicas.clear();
        replicas = node->ring.findReplicas(kvEntry->first);
        for (auto & inode : nodes) {
            if (inode->failed())
                continue;

            Address addr = inode->address(ConnectType::RING);

            if (addr != replicas.at(PRIMARY) &&
                addr != replicas.at(SECONDARY) &&
                addr != replicas.at(TERTIARY)) {
                // Step 4.c Fail a non-replica node
                log->log(inode->address(ConnectType::RING),
                         "Node failed at time=%d", par->getCurrtime());
                inode->fail();
                failedOneNode = true;
                cout << endl << "Failed a non-replica node" << endl;
                break;
            }
        }

        if ( !failedOneNode ) {
            // The code can never reach here
            log->log(node->address(ConnectType::RING), 
                     "Could not fail a node(non-replica)");
            cout << "Could not fail a node(non-replica). Exiting!!!";
            exit(1);
        }

        node = randomAliveNode();

        // Step 4.d Issue a update operation
        cout << endl << "Updating a valid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "UPDATE OPERATION KEY: %s VALUE: %s at time: %d",
                 kvEntry->first.c_str(), newValue.c_str(), par->getCurrtime());
        // This read should fail since at least quorum nodes are not alive
        node->ring.clientUpdate(kvEntry->first, newValue);
    }

    // end of test 4

    //
    // Test 5: Udpate a non-existent key.
    //
    if ( par->getCurrtime() == (TEST_TIME + FIRST_FAIL_TIME + STABILIZE_TIME + STABILIZE_TIME + LAST_FAIL_TIME ) ) {
        string invalidKey = "invalidKey";
        string invalidValue = "invalidValue";

        // Step 5.a Find a node that is alive
        auto node = randomAliveNode();

        // Step 5.b Issue a read operation
        cout << endl << "Updating a valid key.... ... .. . ." << endl;
        log->log(node->address(ConnectType::RING),
                 "UPDATE OPERATION KEY: %s VALUE: %s at time: %d",
                 invalidKey.c_str(), invalidValue.c_str(), par->getCurrtime());
        // This read should fail since at least quorum nodes are not alive
        node->ring.clientUpdate(invalidKey, invalidValue);
    }

    // end of test 5
}


void Application::fail()
{
    Address     addr;

    int removed;
    shared_ptr<IConnection> conn;

    if (par->enableDropMessages && par->getCurrtime() == 50) {
        par->dropMessages = true;
    }

    if (par->singleFailure && par->getCurrtime() == 100) {
        removed = rand() % par->numberOfNodes;

        conn = nodes[removed]->getConnection(ConnectType::MEMBER);
        DEBUG_LOG(log, conn->address(), "Node failed at time=%d", par->getCurrtime());

        nodes[removed]->fail();
    }
    else if (par->getCurrtime() == 100) {
        removed = rand() % par->numberOfNodes/2;

        for (int i = removed; i < removed + par->numberOfNodes/2; i++) {

            conn = nodes[i]->getConnection(ConnectType::MEMBER);
            DEBUG_LOG(log, conn->address(), "Node failed at time=%d", par->getCurrtime());

            nodes[i]->fail();
        }
    }

    if (par->enableDropMessages && par->getCurrtime() == 300) {
        par->dropMessages = false;
    }
}




