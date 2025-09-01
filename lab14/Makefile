MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

lab14a: lab14a.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab14a lab14a.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread

lab14b: lab14b.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab14b lab14b.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread


clean:
	rm -f *.o lab14a lab14b