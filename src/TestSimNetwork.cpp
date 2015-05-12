
#include "stdincludes.h"
#include "catch.hpp"

#include "SimNetwork.h"

TEST_CASE("Basic SimNetwork ops", "[SimNetwork]")
{
	Address 	addr(0x64656667, 8080);
	Address 	addr2(0x64656667, 8081);

	shared_ptr<SimNetwork> network = SimNetwork::createNetwork(nullptr);
	
	SECTION("Basic connection operations") {
		shared_ptr<IConnection> conn = network->create(addr);

		REQUIRE( conn->getStatus() == IConnection::RUNNING );	
		REQUIRE( conn.get() == network->find(addr).get() );
		REQUIRE( network->find(addr2) == nullptr );

		network->remove(conn->address());

		REQUIRE( network->find(addr) == nullptr );
		REQUIRE( conn->getStatus() == IConnection::CLOSED );
	}

	SECTION("Create two connections") {
		shared_ptr<IConnection> conn = network->create(addr);

		REQUIRE( conn.get() == network->find(addr).get() );
		REQUIRE( network->find(addr2) == nullptr );

		REQUIRE_THROWS( network->create(addr) );
		REQUIRE( conn.get() == network->find(addr).get() );

		network->remove(addr);		
		
		REQUIRE( network->find(addr) == nullptr );
	}

	SECTION("Create, delete, and recreate a connection") {
		shared_ptr<IConnection> conn = network->create(addr);
		network->remove(addr);
		
		REQUIRE( network->find(addr) == nullptr );
		REQUIRE( conn->getStatus() == IConnection::CLOSED );

		conn = network->create(addr);
		
		REQUIRE( conn != nullptr );
		REQUIRE( conn.get() == network->find(addr).get() );
		
		network->remove(addr);
	}

	SECTION("connection deletion") {
		// Delete a non-existent connection
		network->remove(addr);
	}
}

TEST_CASE("SimConnection test", "[SimConnection]")
{
	Address 	addr(0x64656667, 8080);
	shared_ptr<SimNetwork> network = SimNetwork::createNetwork(nullptr);
	shared_ptr<IConnection> conn = network->create(addr);
	shared_ptr<SimConnection> simconn = network->findSimConnection(addr);

	SECTION("Test the option apis") {
		REQUIRE( conn->getStatus() == IConnection::RUNNING );
		REQUIRE( simconn->getOption<int>("status") == IConnection::RUNNING );

		simconn->setOption<int>("status", IConnection::DISABLED );
		REQUIRE( conn->getStatus() == IConnection::DISABLED );
		REQUIRE( simconn->getOption<int>("status") == IConnection::DISABLED );

		simconn->setOption<int>("status", IConnection::RUNNING );
	}	

	SECTION("Unknown option") {
		REQUIRE_THROWS( simconn->getOption<int>("XXXX") );
	}
}

TEST_CASE("SimNetwork data operations", "[SimNetwork]")
{
	Address 	addr(0x64656667, 8080);
	Address 	addr2(0x64656668, 8080);
	shared_ptr<SimNetwork> network = SimNetwork::createNetwork(nullptr);
	shared_ptr<IConnection> conn = network->create(addr);
	shared_ptr<SimConnection> simconn = network->findSimConnection(addr);
	Params 		params;

	RawMessage 	raw;
	raw.toAddress = addr2;
	raw.fromAddress = addr;
	raw.size = 23;
	raw.data = new unsigned char[raw.size];

	SECTION("send and receive data") {
	}

	SECTION("send failure cases") {
		// using uninited connection
		simconn->setOption<int>("status", IConnection::UNINITIALIZED);
		REQUIRE_THROWS( conn->send(&raw) );

		// sending to a dead network
		simconn->setOption<int>("status", IConnection::CLOSED);
		REQUIRE_THROWS( conn->send(&raw) );

		simconn->setOption<int>("status", IConnection::DISABLED);
		REQUIRE_THROWS( conn->send(&raw) );

		simconn->setOption<int>("status", IConnection::CONNECTING);
		REQUIRE_THROWS( conn->send(&raw) );

		simconn->setOption<int>("status", IConnection::RUNNING);
	}	

	SECTION("recv failure cases") {
		simconn->setOption<int>("status", IConnection::UNINITIALIZED);
		REQUIRE_THROWS( conn->recv(0) );

		simconn->setOption<int>("status", IConnection::CLOSED);
		REQUIRE_THROWS( conn->recv(0) );

		simconn->setOption<int>("status", IConnection::DISABLED);
		REQUIRE_THROWS( conn->recv(0) );

		simconn->setOption<int>("status", IConnection::CONNECTING);
		REQUIRE_THROWS( conn->recv(0) );

		simconn->setOption<int>("status", IConnection::RUNNING);
	}

	SECTION("sending/receiving using a network that has been deleted") {
		shared_ptr<SimNetwork> network2 = SimNetwork::createNetwork(&params);
		shared_ptr<IConnection> conn2 = network2->create(addr);
		
		// free up the network
		network2 = nullptr;
		REQUIRE_THROWS( conn2->send(&raw) );
		REQUIRE_THROWS( conn2->recv(0) );
	}

	SECTION("sending/receiving using a removed connection") {
		shared_ptr<SimNetwork> network2 = SimNetwork::createNetwork(&params);
		shared_ptr<IConnection> conn2 = network2->create(addr);

		// remove the connection from the network
		network2->remove(conn2->address());		
		REQUIRE_THROWS( conn2->send(&raw) );
		REQUIRE_THROWS( conn2->recv(0) );
	}
}
