MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

all: lab4a lab4b lab4d

lab4a: lab4a.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab4a lab4a.cpp my_socket.cpp

lab4b: lab4b.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab4b lab4b.cpp my_socket.cpp

lab4d: lab4d.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab4d lab4d.cpp my_socket.cpp

clean:
	rm -f *.o lab4a lab4b lab4d

