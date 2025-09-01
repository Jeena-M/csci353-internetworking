#ifndef _MESSAGE_H_
#define _MESSAGE_H_

/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm> 
#include <thread>
#include <fstream>
#include "connection.h"

using namespace std;

class Message {
	public:
	string method;
	string version;
	string nodeid; // m.from 
	int reason; // Only applicable for LSUPDATE
	int number; // Only applicable for LSUPDATE
	string msg_id; // Only applicable for LSUPDATE
	string origin_start_time; // Only applicable for LSUPDATE
	string body;
	string destination;
	string target;
	string nextLayer;
	string udtmsgtype;
	string sesid;
	Message(string m, string v, string n){
		method = m;
		version = v;
		nodeid = n; 
	}

    
};

#endif