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

using namespace std;

class Message {
	public:
	string method;
	string uri; 
	string version;
	Message(string m, string u, string v){
		method = m;
		uri = u;
		version = v;
	}

    
};

#endif