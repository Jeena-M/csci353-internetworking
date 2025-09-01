MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

lab6b: lab6b.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab6b lab6b.cpp my_socket.cpp -pthread

lab6c: lab6c.cpp my_socket.cpp my_socket.h
	g++ ${MYDEFS} -o lab6c lab6c.cpp my_socket.cpp -pthread

lab6d: lab6d.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab6d lab6d.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread

clean:
	rm -f *.o lab6d lab6b lab6c