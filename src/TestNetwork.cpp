/*
 * TestNetwork.cpp
 *
*/

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

TEST_CASE("Address serialziation to/from binary", "[Address]")
{
	Address 	addr(0x64656667, 0xabcd);
	unsigned char	buf[1024];
	size_t bytesWritten = addr.writeToNetworkBuffer(buf, 1024);

	if (isLittleEndian())
	{
		REQUIRE( buf[0] == 0x67 );
		REQUIRE( buf[1] == 0x66 );
		REQUIRE( buf[2] == 0x65 );
		REQUIRE( buf[3] == 0x64 );
		REQUIRE( buf[4] == 0xcd );
		REQUIRE( buf[5] == 0xab );
	}
	else
	{
		//$ TODO: I don't have a big-endian system so I
		// don't know if this is the correct memory layout.
		REQUIRE( buf[0] == 0x64 );
		REQUIRE( buf[1] == 0x65 );
		REQUIRE( buf[2] == 0x66 );
		REQUIRE( buf[3] == 0x67 );
		REQUIRE( buf[4] == 0xab );
		REQUIRE( buf[5] == 0xcd );
	}

	Address 	addr2;
	addr2.readFromNetworkBuffer(buf, bytesWritten);
	REQUIRE( addr2.getIPAddress() == 0x64656667 );
	REQUIRE( addr2.getPort() == 0xabcd );

	SECTION("Bad size handling") {
		REQUIRE_THROWS( addr.readFromNetworkBuffer(buf, 4) );
		REQUIRE_THROWS( addr.readFromNetworkBuffer(buf, 0) );
		REQUIRE_THROWS( addr.writeToNetworkBuffer(buf, 4) );
	}
}
