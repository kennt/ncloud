#**********************
#*
#* Progam Name: MP1. Membership Protocol.
#*
#* Current file: Makefile
#* About this file: Build Script.
#* 
#***********************

CFLAGS =  -Wall -g -std=c++11 -D DEBUG

all: mp1

mp1: Log.o \
		Util.o \
		SimNetwork.o \
		Params.o \
		NetworkNode.o \
		MP1App.o \
		MP1.o \
		jsoncpp.o \
		Member.o
	g++ -o Application MP1App.o Log.o Util.o SimNetwork.o Params.o NetworkNode.o MP1.o jsoncpp.o Member.o ${CFLAGS}

mp2: Log.o \
		Util.o \
		SimNetwork.o \
		Params.o \
		NetworkNode.o \
		MP2App.o \
		MP1.o \
		MP2.o \
		jsoncpp.o \
		Member.o \
		Ring.o
	g++ -o Application MP2App.o Log.o Util.o SimNetwork.o Params.o NetworkNode.o MP1.o MP2.o jsoncpp.o Member.o Ring.o ${CFLAGS}

socket: Log.o \
		Util.o \
		SocketNetwork.o \
		Params.o \
		NetworkNode.o \
		SocketApp.o \
		MP1.o \
		MP2.o \
		jsoncpp.o \
		Member.o \
		Ring.o \
		Command.o
	g++ -o Application SocketApp.o Log.o Util.o SocketNetwork.o Params.o NetworkNode.o MP1.o MP2.o jsoncpp.o Member.o Ring.o Command.o ${CFLAGS}

rafts: 	RaftSockApp.o \
		Command.o \
		Context.o \
		Log.o \
		Member.o \
		MP2.o \
		NetworkNode.o \
		Params.o \
		Raft.o \
		Ring.o \
		SocketNetwork.o \
		Util.o \
		jsoncpp.o
	g++ -o Application RaftSockApp.o Log.o Util.o SocketNetwork.o Params.o NetworkNode.o Raft.o MP2.o jsoncpp.o Context.o Member.o Ring.o Command.o ${CFLAGS}

raft: 	RaftApp.o \
		Command.o \
		Context.o \
		Log.o \
		Member.o \
		MP2.o \
		NetworkNode.o \
		Params.o \
		Raft.o \
		Ring.o \
		SimNetwork.o \
		Util.o \
		jsoncpp.o
	g++ -o Application RaftApp.o Log.o Util.o SimNetwork.o Params.o NetworkNode.o Raft.o MP2.o jsoncpp.o Context.o Member.o Ring.o Command.o ${CFLAGS}

test: TestMain.o \
		TestContext.o \
		TestNetwork.o \
		TestRaft.o \
		TestSimNetwork.o \
		MockNetwork.o \
		Command.o \
		Context.o \
		Log.o \
		Member.o \
		NetworkNode.o \
		Params.o \
		Raft.o \
		Ring.o \
		SimNetwork.o \
		Util.o \
		jsoncpp.o
	g++ -o test TestMain.o TestContext.o TestNetwork.o TestSimNetwork.o TestRaft.o MockNetwork.o Command.o Context.o NetworkNode.o Member.o Log.o SimNetwork.o Raft.o Util.o Params.o Ring.o jsoncpp.o

Log.o: Log.cpp \
		Log.h \
		Params.h \
		Network.h
	g++ -c Log.cpp ${CFLAGS}

Params.o: Params.cpp \
		Params.h \
		Util.h
	g++ -c Params.cpp ${CFLAGS}

SimNetwork.o: SimNetwork.cpp \
		SimNetwork.h \
		Network.h \
		Params.h \
		SparseMatrix.h
	g++ -c SimNetwork.cpp ${CFLAGS}

SocketNetwork.o: SocketNetwork.cpp \
		SocketNetwork.h \
		Network.h \
		Params.h \
		SparseMatrix.h
	g++ -c SocketNetwork.cpp ${CFLAGS}

Util.o: Util.cpp Util.h Network.h
	g++ -c Util.cpp ${CFLAGS}

MockNetwork.o: MockNetwork.cpp \
		MockNetwork.h \
		Network.h
	g++ -c MockNetwork.cpp ${CFLAGS}

NetworkNode.o: NetworkNode.cpp \
		NetworkNode.h \
		Context.h \
		Log.h \
		Member.h \
		Params.h \
		Raft.h \
		Util.h \
		stdincludes.h
	g++ -c NetworkNode.cpp ${CFLAGS}

MP1App.o: MP1App.cpp \
		MP1App.h \
		stdincludes.h \
		Network.h \
		SimNetwork.h \
		Log.h \
		Params.h \
		NetworkNode.h \
		Member.h \
		MP1.h
	g++ -c MP1App.cpp ${CFLAGS}

MP2App.o: MP2App.cpp \
		MP2App.h \
		stdincludes.h \
		Network.h \
		SimNetwork.h \
		Log.h \
		Params.h \
		NetworkNode.h \
		Member.h \
		Ring.h \
		MP1.h \
		MP2.h
	g++ -c MP2App.cpp ${CFLAGS}

RaftApp.o: RaftApp.cpp \
		RaftApp.h \
		Context.h \
		Log.h \
		Member.h \
		MP2.h \
		Network.h \
		NetworkNode.h \
		Params.h \
		Raft.h \
		Ring.h \
		SimNetwork.h \
		stdincludes.h
	g++ -c RaftApp.cpp ${CFLAGS}

SocketApp.o: SocketApp.cpp \
		SocketApp.h \
		Context.h \
		Log.h \
		Network.h \
		NetworkNode.h \
		Member.h \
		MP1.h \
		MP2.h \
		Params.h \
		Ring.h \
		SocketNetwork.h \
		stdincludes.h
	g++ -c SocketApp.cpp ${CFLAGS}

MP1.o:  MP1.cpp \
		MP1.h \
		stdincludes.h \
		Log.h \
		Params.h \
		NetworkNode.h
	g++ -c MP1.cpp ${CFLAGS}

MP2.o:	MP2.cpp \
		MP2.h \
		Context.h \
		Log.h \
		Member.h \
		MP1.h \
		Network.h \
		NetworkNode.h \
		Params.h \
		Ring.h \
		stdincludes.h
	g++ -c MP2.cpp ${CFLAGS}

Raft.o:	Raft.cpp \
		Raft.h \
		Context.h \
		Log.h \
		MP2.h \
		Network.h \
		NetworkNode.h \
		Params.h \
		stdincludes.h
	g++ -c Raft.cpp ${CFLAGS}

RaftSockApp.o:	RaftSockApp.cpp \
		RaftSockApp.h \
		Context.h \
		Log.h \
		MP2.h \
		Network.h \
		NetworkNode.h \
		Params.h \
		Raft.h \
		stdincludes.h
	g++ -c RaftSockApp.cpp ${CFLAGS}

jsoncpp.o: jsoncpp.cpp
	g++ -c jsoncpp.cpp ${CFLAGS}

Member.o: Member.cpp \
		Member.h \
		Context.h \
		Network.h \
		NetworkNode.h
	g++ -c Member.cpp ${CFLAGS}

Context.o: Context.cpp \
		Context.h \
		Network.h \
		NetworkNode.h \
		Util.h
	g++ -c Context.cpp ${CFLAGS}

Ring.o: Ring.cpp \
		Ring.h \
		Context.h \
		Member.h \
		Network.h \
		NetworkNode.h
	g++ -c Ring.cpp ${CFLAGS}

Command.o: Command.cpp \
		Command.h \
		Context.h \
		Log.h \
		Member.h \
		NetworkNode.h \
		Params.h \
		Ring.h \
		stdincludes.h
	g++ -c Command.cpp ${CFLAGS}

TestContext.o: TestContext.cpp \
		Context.h \
		Network.h \
		Raft.h \
		MockNetwork.h \
		Params.h
	g++ -c TestContext.cpp ${CFLAGS}

TestMain.o: TestMain.cpp
	g++ -c TestMain.cpp ${CFLAGS}

TestRaft.o: TestRaft.cpp \
		Context.h \
		Network.h \
		Raft.h \
		MockNetwork.h \
		Params.h
	g++ -c TestRaft.cpp ${CFLAGS}

TestSimNetwork.o: TestSimNetwork.cpp
	g++ -c TestSimNetwork.cpp ${CFLAGS}

TestNetwork.o: TestNetwork.cpp \
		Network.h \
		SimNetwork.h \
		Params.h
	g++ -c TestNetwork.cpp ${CFLAGS}

clean:
	rm -rf *.o Application dbg.log msgcount.log stats.log machine.log
