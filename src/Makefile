#**********************
#*
#* Progam Name: MP1. Membership Protocol.
#*
#* Current file: Makefile
#* About this file: Build Script.
#* 
#***********************

CFLAGS =  -Wall -g -std=c++11 -D DEBUG

all: Application

Application: Log.o \
		Util.o \
		SimNetwork.o \
		Params.o \
		NetworkNode.o \
		MP1App.o \
		MP1.o \
		jsoncpp.o \
		Member.o
	g++ -o Application MP1App.o Log.o Util.o SimNetwork.o Params.o NetworkNode.o MP1.o jsoncpp.o Member.o ${CFLAGS}

test: TestMain.o \
		TestNetwork.o \
		TestSimNetwork.o \
		Params.o \
		SimNetwork.o \
		Util.o \
		jsoncpp.o
	g++ -o test TestMain.o TestNetwork.o TestSimNetwork.o SimNetwork.o Util.o Params.o jsoncpp.o

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

Util.o: Util.cpp Util.h Network.h
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
		NetworkNode.h \
		stdincludes.h \
		Log.h \
		Params.h \
		Member.h \
		Util.h
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

MP1.o:  MP1.cpp \
		MP1.h \
		stdincludes.h \
		Log.h \
		Params.h \
		NetworkNode.h
	g++ -c MP1.cpp ${CFLAGS}

jsoncpp.o: jsoncpp.cpp
	g++ -c jsoncpp.cpp ${CFLAGS}

Member.o: Member.cpp \
		Member.h \
		Network.h \
		NetworkNode.h
	g++ -c Member.cpp ${CFLAGS}

clean:
	rm -rf *.o Application dbg.log msgcount.log stats.log machine.log
