/*****
 * TestNetwork.cpp
 *
 * See LICENSE for details.
 *
 *
 *****/

#include "stdincludes.h"
#include "catch.hpp"
#include "Network.h"

TEST_CASE("Address creation", "[Address]")
{
	// Test creation of an address
	Address 	addr0;
	REQUIRE( addr0.getIPAddress() == 0 );
	REQUIRE( addr0.getPort() == 0 );

	Address 	addr1(31, 32);
	REQUIRE( addr1.getIPAddress() == 31 );
	REQUIRE( addr1.getPort() == 32 );
}

TEST_CASE("Address parsing", "[Address]")
{
	Address 	addr;

	SECTION("successful parsing test") {
		addr.parse("100.101.102.103:80");
		REQUIRE( addr.getIPAddress() == 0x64656667 );
		REQUIRE( addr.getPort() == 80 );
	}

	SECTION("Bad IP address formatting") {
		// Improper IP address
		REQUIRE_THROWS( addr.parse("100:90") );
		REQUIRE_THROWS( addr.parse("100.1:90") );
		REQUIRE_THROWS( addr.parse("100.2.3:90") );
		REQUIRE_THROWS( addr.parse("100.300.3:90") );
		REQUIRE_THROWS( addr.parse(":8080") );

		// Missing port
		REQUIRE_THROWS( addr.parse("100") );
	}
}

TEST_CASE("Address string functions", "[Address]")
{
	Address 	addr(0x64656667, 8080);

	REQUIRE( addr.toString() == "100.101.102.103:8080" );
}

TEST_CASE("Address hash code / Network ID", "[Address]")
{
	Address 	addr(0x64656667, 0xabcd);
	REQUIRE( addr.getNetworkID() == 0x64656667abcd );
}

TEST_CASE("Address comparison operators", "[Address]")
{
	Address 	addr1(0x64656667, 0xabcd);
	Address 	addr2(0x64656667, 0xabcd);	// same as addr1
	Address 	addr3(0x64656677, 0xabcd);	
	Address 	addr4(0x64656667, 0x0000);

	REQUIRE( addr1 == addr2 );
	REQUIRE( addr1 != addr3 );
	REQUIRE( addr1 != addr4 );
	REQUIRE( addr1 < addr3 );
	REQUIRE( addr4 < addr1 );
}

TEST_CASE("Address octet access", "[Address]")
{
	Address 	addr(0x64656667, 0xabcd);
	REQUIRE( addr.getIPOctet(0) == 100 );
	REQUIRE( addr.getIPOctet(1) == 101 );
	REQUIRE( addr.getIPOctet(2) == 102 );
	REQUIRE( addr.getIPOctet(3) == 103 );	
}
