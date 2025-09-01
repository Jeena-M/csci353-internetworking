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
#include "message.h"

using namespace std;

class Connection {
public:
    int conn_number; /* -1 means that the connection is not initialized properly */
    int socket_fd; /* -1 means closed by connection-handling thread, -2 means close by console thread and connection may still be active */
    int orig_socket_fd; /* copy of the original socket_fd */
    int bytes_sent; /* number of bytes of response body written into the socket */
    int response_body_length; /* size of the response body in bytes, i.e., Content-Length */

    shared_ptr<thread> read_thread_ptr; /* shared pointer to a socket-reading thread */
    shared_ptr<thread> write_thread_ptr; /* shared pointer to a socket-writing thread */

    /* the next 3 objects are for the socket-reading thread to send work to the corresponding socket-writing thread */
    shared_ptr<mutex> m; /* this is a "2nd-level" mutex */ 
    shared_ptr<condition_variable> cv;
    queue<shared_ptr<Message>> q;
    string neighbor_nodeid;
    string nodeid; 

    Connection() : conn_number(-1), socket_fd(-1), read_thread_ptr(NULL), write_thread_ptr(NULL), m(NULL), cv(NULL) { }
    Connection(int c, int s, shared_ptr<thread> tr, shared_ptr<thread> tw) {
        conn_number = c;
        socket_fd = orig_socket_fd = s;
        read_thread_ptr = tr;
        write_thread_ptr = tw;
        bytes_sent = response_body_length = 0;
        m = make_shared<mutex>();
        cv = make_shared<condition_variable>();
    }
    void add_work(shared_ptr<Message> msg) {
        m->unlock(); 
    	m->lock();  
    	q.push(msg);  
    	cv->notify_all();  
    	m->unlock();  
    }
    shared_ptr<Message> wait_for_work() { 
    	unique_lock<mutex> lock(*m);  

		while (q.empty()) {  
			cv->wait(lock);  
		}

  		shared_ptr<Message> msg = q.front();  
  		q.pop();  

  		return msg;  
    }
};


#endif