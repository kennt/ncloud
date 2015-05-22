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
This is where the main send/receive loop are driven from.  Currently, there is MP1App.cpp.

### File descriptions
* catch.hpp : a C++ unit-testing framework.
* jsoncpp.cpp : the Jsoncpp framework
* json/* : the Jsoncpp framework
* Log.* : logging implementation
* Member.* : Holds data for MP1 (the membership protocol). MP2 classes will be able to access this data through the NetworkNode.
* MP1.* : Holds the message handlers/messages for MP1
* MP1App.* : Application for MP1, runs the test simulation
* Network.h : Primary network interface header file
* NetworkNode.* : A node on the network. This combines the network and the data for a particular node (which allows MP2 to access MP1 data).
* Params.* : Parameter header file
* SimNetwork.* : Implementation for a simulated network
* SparseMatrix.h : sparse matrix implementation to store statistics
* stdincludes.h
* Test*.cpp : Unit testing files
* Util.* : Utility functions

To solve MP1, the student should only need to change Member.* and MP1.*.

## Differences from the old version
1. Ports are now used in the code. In my code I have used port 6000 for the membership protocol and 6001 for the ring protocol. This will allow one machine to run different processes all taking part in the membership/ring protocol.
2. JSON is used as for the communication protocol.
3. The params file is now a JSON file.


## Future work
* Better documentation
* Implement MP2App.cpp
* Create a version that uses sockets
* Better testing of the higher level code

