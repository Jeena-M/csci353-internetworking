#ifndef _NODE_H_
#define _NODE_H_

/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm> 
#include <thread>
#include <fstream>


class Node {
public:
    string nodeid; /* ORIGIN_NODEID, i.e., extracted from the From: line of a received LSUPDATE message */
    struct timeval timestamp; /* converted from the lart part of an MSGID */
    struct timeval origin_start_time; /* converted from ORIGIN_START_TIME */
    string link_state; /* message body of a received LSUPDATE message */
    vector<string> neighbors; /* extracted from link_state for easy access */
    int level; /* for BFS */
    shared_ptr<Node> pred; /* for BFS */

    Node(string id, struct timeval stamp, struct timeval time, string s, vector<string>& ns) {
    	nodeid = id;
    	timestamp = stamp;
    	origin_start_time = time;
    	link_state = s;
    	neighbors = ns;
        level = -1;
        pred = NULL;

    }
};


#endif