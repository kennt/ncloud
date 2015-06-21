# ncloud
New C++ framework for the Coursera cloud computing course

## Purpose
The purpose of this project is to provide a C++-based framework
for the Coursera Cloud Computing course.

The primary focus is to provide a simple understandable framework for educational purposes. I've tried to keep the code as portable as possible.


## Code outline
There are four main sections of the code:
1. The bottom layer network interfaces (both abstract and concrete classes).
2. The intermediate plumbing (used to hold the application level code/data).
3. The top layer application code that runs the simulation.
4. Utility classes/functions


### Network interfaces
There are two main abstract interfaces: INetwork and IConnection.  INetwork is a factory that produces IConnections.  An IConnection encapsulates the idea of a network endpoint that is used for both sending and receiving.

According there are concrete classes: SimNetwork and SimConnection, that simulate the network.


### NetworkNode and protocol interfaces
The main class here is the NetworkNode. This is used to hold together all of the data that belongs to this "logical node". Thus the MP1/MP2 classes are all part of this class.

In addition, there is a new interface, the IMessageHandler.  This is what is called back when a message is received over the network.


### Application layer
This is where the main send/receive loop are driven from.  Currently, there is MP1App.cpp,
MP2App.cpp, and SocketApp.cpp.

### File descriptions
Not all files are in all branches.  However, the socket branch contains the entirety
of the frameworks.

* `catch.hpp` : a C++ unit-testing framework.
* `jsoncpp.cpp` : the Jsoncpp framework
* `json/*` : the Jsoncpp framework

* `Command.*` : Command protocol implementation
* `Log.*` : logging implementation
* `Member.*` : Holds data for MP1 (the membership protocol). MP2 classes will be able to access this data through the NetworkNode.
* `MP1.*` : Holds the message handlers/messages for MP1
* `MP1App.*` : Application for MP1, runs the test simulation
* `MP2.*` : MP2 message handlers/messages
* `MP2App.*` : MP2 test simulation application
* `Network.h` : Primary network interface header file
* `NetworkNode.*` : A node on the network. This combines the network and the data for a particular node (which allows MP2 to access MP1 data).
* `Params.*` : Parameter header file
* `Ring.*` : DHT ring protocol implementation
* `SimNetwork.*` : Implementation for a simulated network
* `SocketApp.*` : Socket-based application
* `SocketNetwork.*` : Socket-based network implementation
* `SparseMatrix.h` : sparse matrix implementation to store statistics
* `stdincludes.h`
* `Test*.cpp` : Unit testing files
* `Util.*` : Utility functions

## Differences from the old version
1. Ports are now used in the code. In my code I have used port 6000 for the membership protocol and 6001 for the ring protocol. This will allow one machine to run different processes all taking part in the membership/ring protocol.  For MP2, it is assumed that the port number for the ring protocol is one larger than for the membership protocol.
2. The params file is now a JSON file.
3. Added a socket implementation
4. Added unit tests with the catch.hpp framework
5. Added a command protocol (to initiate client commands). This is based on json
so drivers may be written in other languages (a python example is provided).

## Branches

There are three branches: MP1, MP2, and socket.

The MP1 branch is the framework for MP1, the membership protocol programming assignment.

The MP2 branch is the framework for MP2, the distributed hash-table programming assignment.

The socket branch is built on top of MP2, it is an MP2 framework that uses sockets instead
of the simulated network.


### Master (MP1)
To solve MP1: only `Member.*` and `MP1.*` should be changed.

* `make` -- builds the executable, `Application`.
* `make test` -- builds the unit test executable, `test`.

### MP2
To solve MP2: `Member.*`, `MP1.*`, `Ring.*`, and `MP2.*` should be changed.

* `make mp1` -- builds the executable, `Application` with MP1App.cpp
* `make mp2` -- builds the executable, `Application` with MP2App.cpp
* `make test` -- builds the unit test executable, `test`

### socket
This version supports the json-based protocol to send client commands
to a node.  There is some sample code to send a command and wait for a response
in python/commands.py.

* `make mp1` -- builds the executable, `Application` with MP1App.cpp
* `make mp2` -- builds the executable, `Application` with MP2App.cpp
* `make socket` -- builds the executable, `Application` with SocketApp.cpp
* `make test` -- builds the unit test executable, `test`

## Future work
* Better documentation
* Better testing of the higher level code

