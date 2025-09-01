MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

lab9b: lab9b.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab9b lab9b.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread

lab9c: lab9c.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab9c lab9c.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread

clean:
	rm -f *.o lab9b lab9c