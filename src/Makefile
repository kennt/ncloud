#**********************
#*
#* Progam Name: MP1. Membership Protocol.
#*
#* Current file: Makefile
#* About this file: Build Script.
#* 
#***********************

CFLAGS =  -Wall -g -std=c++11

all: Application test

Application: SimNetwork.o

test: TestMain.o \
		TestNetwork.o \
		TestSimNetwork.o \
		Params.o \
		SimNetwork.o \
		Util.o
	g++ -o test TestMain.o TestNetwork.o TestSimNetwork.o SimNetwork.o Util.o Params.o

Log.o: Log.cpp \
		Log.h \
		Params.h \
		Network.h
	g++ -c Log.cpp ${CFLAGS}

Params.o: Params.cpp \
		Params.h
	g++ -c Params.cpp ${CFLAGS}

SimNetwork.o: SimNetwork.cpp \
		SimNetwork.h \
		Network.h \
		Params.h
	g++ -c SimNetwork.cpp ${CFLAGS}

Util.o: Util.cpp Util.h
	g++ -c Util.cpp ${CFLAGS}

TestMain.o: TestMain.cpp
	g++ -c TestMain.cpp ${CFLAGS}

TestSimNetwork.o: TestSimNetwork.cpp
	g++ -c TestSimNetwork.cpp ${CFLAGS}

TestNetwork.o: TestNetwork.cpp \
		Network.h \
		SimNetwork.h \
		Params.h
	g++ -c TestNetwork.cpp ${CFLAGS}

NetworkNode.o: NetworkNode.cpp \
		stdincludes.h \
		Log.h \
		Params.h \
		Member.h \
		NetworkNode.h
	g++ -c NetworkNode.cpp ${CFLAGS}

MP1App.o: MP1App.cpp \
		stdincludes.h \
		Log.h \
		Params.h \
		NetworkNode.h \
		Member.h \
		MP1App.h
	g++ -c MP1App.cpp ${CFLAGS}

Application: Log.o \
		Util.o \
		SimNetwork.o \
		Params.o \
		NetworkNode.o \
		MP1App.o
	g++ -o Application MP1App.o Log.o Util.o SimNetwork.o Params.o NetworkNode.o ${CFLAGS}

#MP1Node.o: MP1Node.cpp MP1Node.h Log.h Params.h Member.h EmulNet.h Queue.h
#	g++ -c MP1Node.cpp ${CFLAGS}
#
#EmulNet.o: EmulNet.cpp EmulNet.h Params.h Member.h
#	g++ -c EmulNet.cpp ${CFLAGS}
#
#Application.o: Application.cpp Application.h Member.h Log.h Params.h Member.h EmulNet.h Queue.h MP2Node.h
#	g++ -c Application.cpp ${CFLAGS}
#
#Log.o: Log.cpp Log.h Params.h Member.h
#	g++ -c Log.cpp ${CFLAGS}
#
#Params.o: Params.cpp Params.h 
#	g++ -c Params.cpp ${CFLAGS}
#
#Member.o: Member.cpp Member.h
#	g++ -c Member.cpp ${CFLAGS}
#
#Trace.o: Trace.cpp Trace.h
#	g++ -c Trace.cpp ${CFLAGS}
#
#MP2Node.o: MP2Node.cpp MP2Node.h EmulNet.h Params.h Member.h Trace.h Node.h HashTable.h Log.h Params.h Message.h
#	g++ -c MP2Node.cpp ${CFLAGS}
#
#Node.o: Node.cpp Node.h Member.h
#	g++ -c Node.cpp ${CFLAGS}
#
#HashTable.o: HashTable.cpp HashTable.h common.h Entry.h
#	g++ -c HashTable.cpp ${CFLAGS}
#
#Entry.o: Entry.cpp Entry.h Message.h
#	g++ -c Entry.cpp ${CFLAGS}
#
#Message.o: Message.cpp Message.h Member.h common.h
#	g++ -c Message.cpp ${CFLAGS}
#
clean:
	rm -rf *.o Application dbg.log msgcount.log stats.log machine.log
