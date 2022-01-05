CC=g++
LFLAGS=
CFLAGS=-c -Wall -std=c++11 -O2
OBJS=server.o serialport.o
EXE=server

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $(EXE) $(LFLAGS)

server.o: server.cpp
	$(CC) server.cpp -o server.o $(CFLAGS)

serialport.o: serialport.h serialport.cpp
	$(CC) serialport.cpp -o serialport.o $(CFLAGS)

clean:
	@rm $(OBJS) $(EXE)
	