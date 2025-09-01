MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

lab12a: lab12a.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab12a lab12a.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread

lab12b: lab12b.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o lab12b lab12b.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread


clean:
	rm -f *.o lab12a lab12b