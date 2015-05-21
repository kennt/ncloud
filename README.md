# ncloud
New C++ framework for the Coursera cloud computing course

## Purpose
The purpose of this project is to provide a C++-based framework
for the Coursera Cloud Computing course.

The primary focus is to provide a simple understandable framework for educational purposes.  Thus simplicity/understandability is preferred over efficiency


## Code outline
There are four main sections of the code:
1. The bottom layer network interfaces (both abstract and concrete classes).
2. The intermediate plumbing (used to hold the application level code/data).
3. The top layer application code that runs the simulation.
4. Utility classes/functions


### Network interfaces
There are two main abstract interfaces: INetwork and IConnection.  INetwork is a factory that produces IConnections.  An IConnection encapsulates the idea of a network endpoint that is used for both sending and receiving.

According there are concrete classes: SimNetwork and SimConnection, that simulate the network.

In the future, I plan on writing the SocketNetwork and SocketConnection code.


### NetworkNode and protocol interfaces
The main class here is the NetworkNode. This is used to hold together all of the data that belongs to this "logical node". Thus the MP1/MP2 classes are all part of this class.

In addition, there is a new interface, the IMessageHandler.  This is what is called back when a message is received over the network.


### Application layer
This is where the main send/receive loop are driven from.  Currently, there is MP1App.cpp.  But MP2App.cpp and SocketApp.cpp are envisioned.


## Differences from the old version
1. Ports are now used in the code. In my code I have used port 6000 for the membership protocol and 6001 for the ring protocol. This will allow one machine to run different processes all taking part in the membership/ring protocol.
2. JSON is used as for the communication protocol. Which means that other languages could be used to provide nodes on the network. One idea may be for an interactive Python session to participate in the membership protocol.
3. The params file is now a JSON file.  Makes it easier to read in.


## Future work
* Better documentation
* Implement MP2App.cpp
* Create a version that uses sockets
* Better testing of the higher level code
* Write an interactive node in Python
