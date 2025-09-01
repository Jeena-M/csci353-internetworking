MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

all: lab3d lab3c echo-server echo-client

echo-server: echo-server.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o echo-server echo-server.cpp my_socket.cpp

echo-client: echo-client.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o echo-client echo-client.cpp my_socket.cpp

lab3c: lab3c.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab3c lab3c.cpp my_socket.cpp

lab3d: lab3d.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab3d lab3d.cpp my_socket.cpp

run-server:
	./echo-server 12345

run-client:
	./echo-client 12345

clean:
	rm -f *.o lab3d lab3c echo-server echo-client

