MYDEFS = -g -Wall -std=c++11 -DLOCALHOST=\"127.0.0.1\"

pa3: pa3.cpp my_socket.cpp my_socket.h my_timestamp.cpp my_timestamp.h
	g++ ${MYDEFS} -o pa3 pa3.cpp my_socket.cpp my_timestamp.cpp -lcrypto -pthread

clean:
	rm -f *.o pa3 