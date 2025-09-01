#ifndef _CONNECTION_H_
#define _CONNECTION_H_

/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm> 
#include <thread>
#include <fstream> 

using namespace std;

class Connection {
	public:
    int conn_number; /* -1 means that the connection is not initialized properly */
    int socket_fd; /* -1 means that the connection is inactive/closed */
	int orig_socket_fd; /* copy of the original socket_fd */
	int kb_sent; /* number of KB (including partial KB at the end) of response body written into the socket */		
    shared_ptr<thread> thread_ptr; /* shared pointer to a thread object */
	string last_uri;
	struct timeval start_time;
	int last_content_length;
    Connection() : conn_number(-1), socket_fd(-1), thread_ptr(NULL) { }
    Connection(int c, int s, shared_ptr<thread> p) { conn_number = c; socket_fd = orig_socket_fd = s; thread_ptr = p; kb_sent = 0;}
};

#endif