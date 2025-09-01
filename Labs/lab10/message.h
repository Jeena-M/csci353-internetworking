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
	Message(string m, string v, string n){
		method = m;
		version = v;
		nodeid = n; 
	}

    
};

#endif