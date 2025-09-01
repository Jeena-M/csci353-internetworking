/* C++ standard include files first */
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm> 
#include <thread>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cctype>
#include <unordered_set>

using namespace std;

/* C system include files next */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* C standard include files next */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>

/* your own include last */
#include "my_socket.h"
#include "my_timestamp.h"
#include "connection.h"
#include "message.h"
#include "node.h"
#include "event.h"
#include "timer.h"
#include "timercallback.h"
#include "rdt30_state.h"

static int listen_socket_fd = (-1); /* there is nothing wrong with using a global variable */
static int gnDebug = 0; /* change it to 0 if you don't want debugging messages */

static vector<shared_ptr<Connection>> connection_list;
static mutex m;
static int next_conn_number = 1; // Global variable; everytime you assign the value here, must increment
int max_ttl;
int msg_lifetime;
int N = 1; // speed
queue<shared_ptr<Connection>> q;
condition_variable cv;
int total = 0;
static map<string, string> msg_cache;

static map<string, shared_ptr<Node>> adjacency_list;

static ofstream file_H;
static ofstream file_L;
static ofstream file_U;
ostream *mylog=NULL;
ostream *mylogLSUPDATE = NULL;
ostream *mylogU = NULL;

queue<Event> event_queue;
condition_variable cv2;
string tracerouteSesID;

vector<RDT30_State> rdt_sessions;

queue<Event> rdt_event_queue;
condition_variable cv3;


/*
 * This function demonstrate that you can use the ostream to write to cout or a file.
 */
static
void LogALine(string a_line_of_msg, string type)
{
    
    if (type == "H"){
        
        *mylog << a_line_of_msg;
        mylog->flush();
    } else if (type == "L"){
        *mylogLSUPDATE << a_line_of_msg;
        mylogLSUPDATE->flush();

    } else if (type == "U"){
        *mylogU << a_line_of_msg;
        mylogU->flush();
    }
    
}

void GetObjID(
        string node_id,
        const char *obj_category,
        string& hexstring_of_unique_obj_id,
        string& origin_start_time)
{
    static unsigned long seq_no = 1L;
    static int first_time = 1;
    static struct timeval node_start_time;
    static char origin_start_time_buf[18];
    char hexstringbuf_of_unique_obj_id[80];

    /* IMPORTANT: this code must execute inside a critical section of the main mutex */
    if (first_time) {
        gettimeofday(&node_start_time, NULL);
        snprintf(origin_start_time_buf, sizeof(origin_start_time_buf), "%010d.%06d", (int)(node_start_time.tv_sec), (int)(node_start_time.tv_usec));
        first_time = 0;
    }
    seq_no++;
    struct timeval now;
    gettimeofday(&now, NULL);
    snprintf(hexstringbuf_of_unique_obj_id, sizeof(hexstringbuf_of_unique_obj_id),"%s_%1ld_%010d.%06d", node_id.c_str(), (long)seq_no, (int)(now.tv_sec), (int)(now.tv_usec));
    hexstring_of_unique_obj_id = hexstringbuf_of_unique_obj_id;
    origin_start_time = origin_start_time_buf;
}

void reaper_add_work(shared_ptr<Connection> w) {
  m.lock();
  q.push(w);
  cv.notify_all();
  //total++;
  m.unlock();
}

shared_ptr<Connection> reaper_wait_for_work() {
  unique_lock<mutex> l(m);

  while (q.empty()) {
    //if (total == 100) {
      //return (-1);
    //}
    cv.wait(l);
  }
  shared_ptr<Connection> k = q.front();
  q.pop();

  return k;
}

void traceroute_add_work(Event ev){
    //cout << ev.type << endl;
    m.lock();
    event_queue.push(ev);
    cv2.notify_all();
    m.unlock();
}

Event traceroute_wait_for_work(){
    unique_lock<mutex> l(m);

    while (event_queue.empty()){
        cv2.wait(l);
    }

    Event ev = event_queue.front();
    //cout << "returning " << ev.type << endl;
    event_queue.pop();
    return ev;

}

void rdt_add_work (Event ev) {
    m.lock();
    rdt_event_queue.push(ev);

    cv3.notify_all();
    m.unlock();
}

Event rdt_wait_for_work(){
    unique_lock<mutex> l(m);

    while(rdt_event_queue.empty()){
        cv3.wait(l);
    }

    Event ev = rdt_event_queue.front();

    rdt_event_queue.pop();

    return ev;
}

string make_data_pkt(int seq_no, char payload_char) {
    return "RDTPKT:" + to_string(seq_no) + ":" + string(1, payload_char);
}



static
vector<pair<string, int>> getSections(vector<string>& file_lines){
    vector<pair<string, int>> sectionsAndLocation;
    int i = 0; 
    for (string line: file_lines){
        if (line[0] == '['){
            string name = line.substr(1, line.length()-3);
            sectionsAndLocation.push_back(make_pair(name, i));
        }
        i += 1;
    }

    return sectionsAndLocation;
}

static
void Init(bool logToFile, string logfile, string type)
{
    if (type == "H"){
        if (logToFile) {
            //cout << logfile << endl;
            file_H.open(logfile, ofstream::out|ofstream::app);
            mylog = &file_H;
        } else {
            mylog = &cout;
        }
    } else if (type == "L"){
        if (logToFile) {
            file_L.open(logfile, ofstream::out|ofstream::app);
            mylogLSUPDATE = &file_L;
        } else {
            mylogLSUPDATE = &cout;
        }
    } else if (type == "U"){
        if (logToFile){
            file_U.open(logfile, ofstream::out|ofstream::app);
            mylogU = &file_U;
        } else {
            mylogU = &cout;
        }
    }
    
}

void udt_send(string src_nodeid, string message, string target, shared_ptr<Connection> conn_ptr){
    shared_ptr<Message> msg = make_shared<Message>("UCASTAPP", "353NET/1.0", src_nodeid);
    msg->body = message;
    int content_length = message.size();

    m.lock();
    
    string msg_id, origin_time;
    GetObjID(msg->nodeid, "msg", msg_id, origin_time);
    m.unlock();
    

    msg->msg_id = msg_id;
    msg->origin_start_time = origin_time;
    msg->number = max_ttl;
    
    msg->target = target;
    msg->nextLayer = "1";

    vector<string> neighbor_node_ids;
    string destination;

    m.lock();

    for (const auto& conn : connection_list) {
                                    //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
        if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0) {  
            neighbor_node_ids.push_back(conn->neighbor_nodeid);
        }
    }
    m.unlock();
                                
    if (neighbor_node_ids.size() == 0){
        cout << target << "is not reachable\n";
    } else {
        m.lock();
                                    
        struct timeval blank = {0, 0};

        shared_ptr<Node> start_node = make_shared<Node>(conn_ptr->nodeid, blank, blank, "", neighbor_node_ids);
        unordered_set<string> visited;
        queue<shared_ptr<Node>> BFS_q;
        BFS_q.push(start_node);
        visited.insert(conn_ptr->nodeid);

        while(!BFS_q.empty()){
            shared_ptr<Node> current = BFS_q.front();
            BFS_q.pop();
            for (const string& neighborID: current->neighbors){
                //cout << neighborID << endl;
                if (visited.find(neighborID) == visited.end()) {
                    visited.insert(neighborID);
                    adjacency_list[neighborID]->pred = current; // Set pred
                    BFS_q.push(adjacency_list[neighborID]);
                }
            }
                                        
        }
                                    
        for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                                        if (visited.find(it->first) == visited.end()) {
                                            it = adjacency_list.erase(it);  
                                        } else {
                                            ++it;
                                        }
        }

        bool isIn = false;
                                    
        for (const auto& entry : adjacency_list) {
                                        const auto& node_id = entry.first;
                                        const auto& node_ptr = entry.second;
                                        if (node_id == conn_ptr->nodeid) continue; // skip SELF

                                        shared_ptr<Node> current = node_ptr;

                                        
                                        while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != conn_ptr->nodeid) {
                                            
                                            current = adjacency_list[current->nodeid]->pred;
                                        }
                                        //cout << target << " " << node_id << " " << adjacency_list[current->nodeid]->pred->nodeid << endl;
                                        if (node_id == target && adjacency_list[current->nodeid]->pred->nodeid == conn_ptr->nodeid) { // Should always be the case
                                            //cout << "Should route here: "<< current->nodeid << "\n";
                                            destination = current->nodeid;
                                            //cout << "I set destination to " << current->nodeid << endl;
                                            isIn = true;
                                        }
        }

        m.unlock();
    }
    msg->destination = destination;


    m.lock();
    for (auto& conn : connection_list){
        
        if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
            
             conn->add_work(msg);

             string send_utp = "["+get_timestamp_now()+"] " + "i UCASTAPP :" + conn->neighbor_nodeid +
             " " + to_string(max_ttl) + " - " + to_string(content_length) + " :" + 
             msg->msg_id + " " + " :" + msg->nodeid + " :" +  target + " 1 " + msg->body + "\n";
             //cout << send_utp;
             LogALine(send_utp, "U");
        }
    }
    m.unlock();

}


static
void usage_server()
{
    cerr << "usage: ./echo-server port_number" << endl;
    exit(-1);
}

static
int non_ASCII(char ch)
{
    if (ch >= 0x20 && ch < 0x7f) return 0;
    switch (ch) {
    case '\r': return 0;
    case '\n': return 0;
    case '\t': return 0;
    default: break;
    }
    return 1;
}

/**
 * Use this code to return the file size of path.
 *
 * You should be able to use this function as it.
 *
 * @param path - a file system path.
 * @return the file size of path, or (-1) if failure.
 */
static
int get_file_size(string path)
{
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        return (-1);
    }
    return (int)(stat_buf.st_size);
}

/**
 * Read a line from the socket and return a C++ string.
 *         Can call this function repeatedly to read one line at a time.
 *         A line ends with a '\n'.
 *         The last line may be a "partial line", i.e., not ends with a '\n'.
 * Return -1 if there is an error (must ignore the returned C++ string).
 *         End-of-file/end-of-input is considered an error.
 *         After this function returns -1, should continue to return -1 if called again.
 * Otherwise, return the length of the returned C++ string.
 *
 * You should be able to use this function as it.
 * You should only call this function if you are expecting a line of text from socket_fd.
 *
 * @param socket_fd - client socket created by create_client_socket().
 * @param line - returned C++ string.
 */
int read_a_line(int socket_fd, string& line)
{
    string s = "";
    int idx = 0;
    char ch = '\0';
    int debug = 1; /* not a good idea to change this! */

    for (;;) {
        int bytes_read = read(socket_fd, &ch, 1);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                /* not a real error, must retry */
                continue;
            }
            /* a real error, no need to return a line */
            return (-1);
        } else if (bytes_read == 0) {
            /*
             * according to man pages, 0 means end-of-file
             * if we don't break here, read() will keep returning 0!
             */
            if (idx == 0) {
                /* if no data has been read, just treat end-of-file as an error */
                return (-1);
            }
            /*
             * the last line does not terminate with '\n'
             * return the last line (which does not end with '\n')
             */
            break;
        } else {
            /*
             * being super paranoid and harsh here
             * if you are expecting binary data, you shouldn't be calling read_a_line()
             */
            if (debug && non_ASCII(ch)) {
                //cout << "this is it: " << ch << endl;
                /*
                 * if you don't want to abort and crash your program here, you can set debug = 0 above
                 * although I would strongly encourage you not to do that and fix your bugs instead
                 */
                cerr << "Encountered a non-ASCII character (0x" << setfill('0') << setw(2) << hex << (int)ch << ") in read_a_line().  Abort program!" << endl;
                shutdown(socket_fd, SHUT_RDWR);
                close(socket_fd);
                exit(-1);
            }
            s += ch;
            idx++;
            if (ch == '\n') {
                break;
            }
        }
    }
    line = s;
    {   /* [BC: added 1/16/2023 to improve debugging */ extern int my_debug_header_lines;
        if (my_debug_header_lines) {
            cout << "\t" << line;
            cout.flush();
        }
    }
    return idx;
}

/**
 * Use this code to write to a socket (not a good idea to call write() directly).
 * Return 0 if successful.
 * Return -1 if there is an error.
 *
 * You should be able to use this function as it.
 *
 * @param fd - socket file descriptor or a regular file descriptor.
 * @param buf - buffer address.
 * @param bytes_to_wrte - number of bytes to write, starting at buf.
 */
int better_write(int fd, const char *buf, int bytes_to_write)
{
    int bytes_remaining = bytes_to_write;

    while (bytes_remaining > 0) {
        int bytes_written = write(fd, buf, bytes_remaining);

        if (bytes_written > 0) {
            bytes_remaining -= bytes_written;
            buf += bytes_written;
        } else if (bytes_written == (-1)) {
            if (errno == EINTR) {
                continue;
            }
            /* a real error, abort write() */
            return (-1);
        }
    }
    return bytes_to_write;
}

int my_debug_header_lines = 0; /* set to 1 to print all reading/writing header lines to cout */

/**
 * Call this function instead of better_write() when you are writing any LINE of a message HEADER into the socket (or the entire message HEADER).
 *         This function will verify that bytes_to_write is > 1, buf[bytes_to_write-2] is '\r', and buf[bytes_to_write-1] is '\n'.
 *         If debugging is not turned on, this function would just check the above and then call better_write().
 * Must not use this function to write any part of a message BODY into the socket because we are supposed to treat message body as binary data!
 */
int better_write_header(int socket_fd, const char *buf, int bytes_to_write)
{
    if (bytes_to_write < 2) {
        cerr << "Must not call better_write_header() with bytes_to_write < 2.  Abort program!" << endl;
        shutdown(socket_fd, SHUT_RDWR);
        exit(-1);
    } else if (buf[bytes_to_write-2] != '\r' || buf[bytes_to_write-1] != '\n') {
        cerr << "Must not call better_write_header() with the last two character of buf not being \"\\r\\n\".  Abort program!" << endl;
        shutdown(socket_fd, SHUT_RDWR);
        exit(-1);
    }
    if (my_debug_header_lines && bytes_to_write > 0) {
        /* make sure that you are not writing binary data */
        const char *start_ptr = buf;
        for (int i = 0; i < bytes_to_write; i++) {
            if (non_ASCII(buf[i])) {
                cerr << "Encountered a non-ASCII character (0x" << setfill('0') << setw(2) << hex << (int)buf[i] << ") in better_write_header().  Abort program!" << endl;
                shutdown(socket_fd, SHUT_RDWR);
                exit(-1);
            }
            if (buf[i] == '\n') {
                write(1, "\t", 1);
                write(1, start_ptr, (&buf[i]+1)-start_ptr);
                start_ptr = &buf[i+1];
            }
        }
        if (start_ptr != &buf[bytes_to_write]) {
            write(1, "\t", 1);
            write(1, start_ptr, (&buf[bytes_to_write])-start_ptr);
            cerr << "Fatal error in better_write_header().  Abort program!" << endl;
            shutdown(socket_fd, SHUT_RDWR);
            exit(-1);
        }
        fflush(stdout);
    }
    return better_write(socket_fd, buf, bytes_to_write);
}

/**
 * Call better_write_header_debug(1) to turn on debugging in better_write_header().
 * Call better_write_header_debug(0) to turn off debugging in better_write_header().
 */
void better_write_header_debug(int turn_on)
{
    my_debug_header_lines = turn_on;
}
 
static void initiate_LSUPDATE_flood(int reason, shared_ptr<Connection> conn_ptr){ // Create a new LSUPDATE message and send a copy to each active connection
    shared_ptr<Message> msg = make_shared<Message>("LSUPDATE", "353NET/1.0", conn_ptr->nodeid);
    
    
    msg->reason = reason;

    m.lock();
    
    string msg_id, origin_time;
    GetObjID(msg->nodeid, "msg", msg_id, origin_time);
    m.unlock();
    

    msg->msg_id = msg_id;
    msg->origin_start_time = origin_time;
    msg->number = max_ttl;

    string body = "";

    m.lock();
    
    
    for (const auto& conn : connection_list) {
        if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0) {  
            body += conn->neighbor_nodeid; 
            body += ",";
        }
    }

    if (!body.empty() && body.back() == ',') {
        body.pop_back();  
    }
    m.unlock();
    


    msg->body = body;

    int content_length = body.size();

    m.lock();
    for (auto& conn : connection_list){
        
        if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0){
            
             conn->add_work(msg);

             
             string printBody = "";
             istringstream iss(body);
             vector<string> parts;
             string segment;

             while (getline(iss, segment, ',')) {
                                    parts.push_back(segment);
             }
             for (size_t i = 0; i < parts.size(); ++i) {
                                    printBody += ":"+parts[i];
                                    if (i != parts.size() - 1) {
                                        printBody += ",";
                                    }
             }
             int printCL = content_length + parts.size();
             string initiate_flood = "["+get_timestamp_now()+"] " + "i LSUPDATE :" + conn->neighbor_nodeid +
             " " + to_string(max_ttl) + " F " + to_string(printCL) + " :" + 
             msg->msg_id + " " + msg->origin_start_time + " :" + msg->nodeid + " " + "(" + printBody + ")\n";
             LogALine(initiate_flood, "L");
        }
    }
    m.unlock();

    

    

}



static void read_from_client(shared_ptr<Connection> conn_ptr)
{
    conn_ptr->m->lock();
    conn_ptr->m->unlock();

    int bytes_received;

    string line;
    vector<string> header_lines;
    string client_ip_and_port = get_ip_and_port_for_server(conn_ptr->socket_fd, 0);
    

    for (;;) {
        //cout << "here I am " << endl;
        bytes_received = read_a_line(conn_ptr->socket_fd, line);
        
        // If read_a_line returns a value <= 0: socket is dead
        if (bytes_received <= 0){
            m.unlock();
            m.lock();
            conn_ptr->m->unlock();
            conn_ptr->m->lock();
            if (conn_ptr->socket_fd >= 0){
                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
            }
            conn_ptr->socket_fd = -1;
            //close(conn_ptr->orig_socket_fd);
            conn_ptr->m->unlock();
            m.unlock();

            conn_ptr->add_work(NULL);
            conn_ptr->write_thread_ptr->join();
            string line_read_joined = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tSocket-reading thread has joined with socket-writing thread\n";
            //LogALine(line_read_joined);

            reaper_add_work(conn_ptr);
                
            string line_closed = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tConnection closed with client at " + client_ip_and_port + "\n";
            //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
            //LogALine(line_closed);

                
            return;
        }

        // Reached the end of the header
        if (line == "\r\n"){
            header_lines.push_back(line);
            break;
        }

        // Otherwise, append the line to header_lines
        header_lines.push_back(line);
    }

    if (!header_lines.empty()) {
        string from_line = header_lines[3];
        size_t end_pos = line.find("\r\n", 6);

        string NODEID = from_line.substr(6, end_pos-6); // From?
        
        
        // Find the position of the first colon
        size_t colon_pos = NODEID.find(":");

        // Check if a colon exists and extract the part after it
        if (colon_pos != string::npos && colon_pos + 1 < NODEID.size()) {
            NODEID = NODEID.substr(colon_pos + 1); // Get everything after the colon
        }

        // Remove all whitespace from NODEID
        NODEID.erase(std::remove_if(NODEID.begin(), NODEID.end(), ::isspace), NODEID.end());


        string msg_received =  "["+get_timestamp_now()+"] " + "r SAYHELLO :" + NODEID + " 1 - 0\n";
        
        LogALine(msg_received, "H");
        bool is_duplicate;

        if (conn_ptr->neighbor_nodeid == ""){
            /* connection c was created in main thread */
            m.lock();
            

            is_duplicate = false;

            // Check if the message is a duplicate connection
            for (auto& d : connection_list) {
                
                if (d != conn_ptr && d->neighbor_nodeid != "" && d->neighbor_nodeid == NODEID) {
                    is_duplicate = true;
                    
                }
            }

            m.unlock();
            

            if (!is_duplicate){

                conn_ptr->m->lock();
                
                
                conn_ptr->neighbor_nodeid = NODEID;

                shared_ptr<Message> w = std::make_shared<Message>("SAYHELLO", "353NET/1.0", conn_ptr->nodeid);
                
                conn_ptr->add_work(w);
                conn_ptr->m->unlock();


            }

        } else {
            /* connection c was created in neighbors thread */
            m.lock();
            
            is_duplicate = false;

            // Check if the message is a duplicate connection
            for (auto& d : connection_list) {
                if (d != conn_ptr && d->neighbor_nodeid != "" && d->neighbor_nodeid == NODEID) {
                    is_duplicate = true;
                    //std::cout << "Duplicate connection found." << std::endl;
                }
            }
            
            m.unlock();
        }

        if (!is_duplicate){
            /* BEGIN THE LAB 11 CODE */
            
            // initiate LSUPDATE flood (reason = 1)
            initiate_LSUPDATE_flood(1, conn_ptr);
            while (true){
                //cout << "Namaste" << endl;
                header_lines.clear();
                for (;;) {
                    //cout << "Do I come here?" << endl;
                    bytes_received = read_a_line(conn_ptr->socket_fd, line);
                    
                    // If read_a_line returns a value <= 0: socket is dead
                    //conn_ptr->m->lock();
                    if (listen_socket_fd == -1 || bytes_received <= 0){
                        
                        m.unlock();
                        m.lock();
                        conn_ptr->m->unlock();
                        conn_ptr->m->lock();
                        if (conn_ptr->socket_fd >= 0){
                            shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                        }
                        conn_ptr->socket_fd = -1;
                        //close(conn_ptr->orig_socket_fd);
                        conn_ptr->m->unlock();
                        m.unlock();

                        conn_ptr->add_work(NULL);
                        initiate_LSUPDATE_flood(2, conn_ptr);
                        conn_ptr->write_thread_ptr->join();

                        reaper_add_work(conn_ptr);
                            
                        string line_closed = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tConnection closed with client at " + client_ip_and_port + "\n";
                        //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                        //LogALine(line_closed);

                            
                        return;
                    }
                    //conn_ptr->m->unlock();

                    // Reached the end of the header
                    if (line == "\r\n"){
                        header_lines.push_back(line);
                        //cout << "Broke here: " << endl;
                        break;
                    }

                    // Otherwise, append the line to header_lines
                    header_lines.push_back(line);
                }
                
                
                conn_ptr->m->lock();
                if (listen_socket_fd == -1 || header_lines.empty()){
                    conn_ptr->m->unlock();
                    break;
                }

                //cout << "Do I come?" << endl;
                
                /* process UCASTAPP message */
                bool isUCASTAPP = false; 
                for (const auto& line : header_lines) {
                    if (line.find("UCASTAPP") != -1) {
                        isUCASTAPP = true;
                    } 
                }

                if (isUCASTAPP){
                    int ttl;
                    string msg_id;
                    string from;
                    string target;
                    int content_length;
                    string nextLayer;

                    for (const auto& line : header_lines) {
                            if (line.find("TTL:") == 0) {
                                ttl = stoi(line.substr(5));  
                            }
                            else if (line.find("To:") == 0) {
                                target = line.substr(4);  
                                target.erase(
                                    find_if(target.rbegin(), target.rend(), [](unsigned char ch) {
                                        return !isspace(ch);
                                    }).base(),
                                    target.end()
                                );
                            }
                            else if (line.find("MessageID:") == 0) {
                                msg_id = line.substr(11);  
                                msg_id.erase(
                                    find_if(msg_id.rbegin(), msg_id.rend(), [](unsigned char ch) {
                                        return !isspace(ch);
                                    }).base(),
                                    msg_id.end()
                                );
                            }
                            else if (line.find("From:") == 0) {
                                from = line.substr(6); 
                                from.erase(
                                    find_if(from.rbegin(), from.rend(), [](unsigned char ch) {
                                        return !isspace(ch);
                                    }).base(),
                                    from.end()
                                ); 
                            }
                            else if (line.find("Next-Layer:") == 0) {
                                nextLayer = line.substr(12);  
                                nextLayer.erase(
                                    find_if(nextLayer.rbegin(), nextLayer.rend(), [](unsigned char ch) {
                                        return !isspace(ch);
                                    }).base(),
                                    nextLayer.end()
                                );
                            } else if (line.find("Content-Length:") == 0){
                                content_length = stoi(line.substr(16));
                            }
                    }

                    int bytes_received = 0;
                        
                    string body;
                    char buffer[content_length];



                    //cout << content_length << endl;
                    while (bytes_received < content_length){
                            
                            int to_read = std::min(content_length - bytes_received, (int)sizeof(buffer));
                            
                            int chunk_length = read(conn_ptr->socket_fd, buffer, to_read);
                            
                            //conn_ptr->m->lock();
                            if (chunk_length <= 0) {
                                m.unlock();
                                m.lock();
                                conn_ptr->m->unlock();
                                conn_ptr->m->lock();
                                if (conn_ptr->socket_fd >= 0){
                                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                                }
                                conn_ptr->socket_fd = -1;
                                //close(conn_ptr->orig_socket_fd);
                                conn_ptr->m->unlock();
                                m.unlock();

                                conn_ptr->add_work(NULL);
                                initiate_LSUPDATE_flood(2, conn_ptr);
                                conn_ptr->write_thread_ptr->join();

                                reaper_add_work(conn_ptr);
                                    
                                string line_closed = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tConnection closed with client at " + client_ip_and_port + "\n";
                                //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                                //LogALine(line_closed);

                                    
                                return;
                            } 
                            //conn_ptr->m->unlock();

                            body.append(buffer, chunk_length);
                            //cout << "Body: " << body << endl;

                            bytes_received += chunk_length;

                    }
                    

                    /************* PROCESSING RDT ******************/
                    if (body.find("RDTPKT") != -1){
                        if (conn_ptr->nodeid == target){


                            string msg_rcvd = "";
                            size_t first_colon = body.find(':');
                            size_t second_colon = body.find(':', first_colon + 1);
                            string seq_str, msg; 

                            if (first_colon != std::string::npos && second_colon != std::string::npos) {
                                seq_str = body.substr(first_colon + 1, second_colon - first_colon - 1);
                                msg = body.substr(second_colon + 1);
                            } else {
                                std::cerr << "Invalid packet format.\n";
                            }



                            int seq_num;
                            if (seq_str.find('0') != -1){
                                seq_num = 0;
                            } else {
                                seq_num = 1; 
                            }

                            int data_seq_num = seq_num;

                            int expected_seq_num;
                            bool found = false;
                            string ack_packet;

                            string msg_print = msg;
                            if (msg == "\n"){
                                msg_print = "\\n";
                            }

                            string received = "["+get_timestamp_now()+"] " + "r UCASTAPP :" + conn_ptr->neighbor_nodeid +
                                             " " + to_string(ttl) + " - " + to_string(content_length) + " :" + 
                                             msg_id + " " + " :" + from + " :" +  target + " 2 " + to_string(seq_num) + " - 1 " + msg_print + "\n";
                            //cout << received;
                            LogALine(received, "U");
                            

                            m.lock();
                            for (int i = 0; i < rdt_sessions.size(); i++){
                                
                                if (rdt_sessions[i].peer_nodeid == from){
                                    expected_seq_num = rdt_sessions[i].seq_no;
                                    msg_rcvd = rdt_sessions[i].msg_received;
                                    found = true;
                                    


                                    break;

                                }
                            }
                            m.unlock();

                            if (found){
                                
                                if (expected_seq_num == data_seq_num){
                                        if (msg != "\n"){
                                            msg_rcvd += msg;
                                        }

                                        ack_packet = "353UDT/1.0 RDTACK "+seq_str+" 0";

                                        udt_send(conn_ptr->nodeid, ack_packet, from, conn_ptr);

                                        if (msg != "\n"){
                                            if (seq_num == 0){
                                                seq_num = 1;
                                            } else {
                                                seq_num = 0;
                                            }
                                        }

                                        m.lock();
                                        for (int i = 0; i < rdt_sessions.size(); i++){
                                            if (rdt_sessions[i].peer_nodeid == from){
                                                rdt_sessions[i].msg_received = msg_rcvd;
                                                rdt_sessions[i].seq_no = seq_num;
                                                break;
                                            }
                                        }
                                        
                                        m.unlock();

                                    } else {
                                        if (seq_num == 0){
                                            seq_num = 1;
                                        } else {
                                            seq_num = 0;
                                        }
                                        ack_packet = "353UDT/1.0 RDTACK "+to_string(seq_num)+" 0";
                                        udt_send (conn_ptr->nodeid, ack_packet, from, conn_ptr);
                                    }
                            }

                            if (!found){
                                
                                RDT30_State recses;
                                recses.peer_nodeid = from;
                                recses.seq_no = seq_num;
                                recses.app_no = 0;
                                recses.msg_received = "";

                                m.lock();
                                rdt_sessions.push_back(recses);
                                m.unlock();

                                expected_seq_num = seq_num;
                                //cout << expected_seq_num << " " << data_seq_num << endl;
                                
                                if (expected_seq_num == data_seq_num){
                                    
                                        if (msg != "\n"){
                                            msg_rcvd += msg;
                                        }

                                        ack_packet = "353UDT/1.0 RDTACK "+seq_str+" 0";

                                        udt_send(conn_ptr->nodeid, ack_packet, from, conn_ptr);
                                        

                                        if (msg != "\n"){
                                            if (expected_seq_num == 0){
                                                expected_seq_num = 1;
                                            } else {
                                                expected_seq_num = 0;
                                            }
                                        }
                                        

                                        m.lock();
                                        for (int i = 0; i < rdt_sessions.size(); i++){
                                            if (rdt_sessions[i].peer_nodeid == from){
                                                rdt_sessions[i].msg_received = msg_rcvd;
                                                rdt_sessions[i].seq_no = expected_seq_num;
                                            }
                                        }
                                        
                                        m.unlock();

                                    } else {
                                        if (seq_num == 0){
                                            seq_num = 1;
                                        } else {
                                            seq_num = 0;
                                        }
                                        ack_packet = "353UDT/1.0 RDTACK "+to_string(seq_num)+" 0";
                                        udt_send (conn_ptr->nodeid, ack_packet, from, conn_ptr);
                                    }
                                   
                                }
                                
                            

                            if (msg == "\n" && expected_seq_num == seq_num){
                                cout << "RDT message \'" << msg_rcvd << "\' received from :" << from << "\n";
                                m.lock();
                                for (auto it = rdt_sessions.begin(); it != rdt_sessions.end(); ++it) {
                                    if (it->peer_nodeid == from) {
                                        rdt_sessions.erase(it);
                                        break;
                                    }
                                }
                                m.unlock();
                            }
                        } // end of if found the right node 
                        else {
                                string destination;
                                vector<string> neighbor_node_ids;

                                m.lock();

                                for (const auto& conn : connection_list) {
                                    //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                                    if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0) {  
                                        neighbor_node_ids.push_back(conn->neighbor_nodeid);
                                    }
                                }
                                m.unlock();
                                
                                if (neighbor_node_ids.size() == 0){
                                    cout << target << "is not reachable\n";
                                } else {
                                    m.lock();
                                    
                                    struct timeval blank = {0, 0};

                                    shared_ptr<Node> start_node = make_shared<Node>(conn_ptr->nodeid, blank, blank, "", neighbor_node_ids);
                                    unordered_set<string> visited;
                                    queue<shared_ptr<Node>> BFS_q;
                                    BFS_q.push(start_node);
                                    visited.insert(conn_ptr->nodeid);

                                    while(!BFS_q.empty()){
                                        shared_ptr<Node> current = BFS_q.front();
                                        BFS_q.pop();
                                        for (const string& neighborID: current->neighbors){
                                            //cout << neighborID << endl;
                                            if (visited.find(neighborID) == visited.end()) {
                                                visited.insert(neighborID);
                                                adjacency_list[neighborID]->pred = current; // Set pred
                                                BFS_q.push(adjacency_list[neighborID]);
                                            }
                                        }
                                        
                                    }
                                    
                                    for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                                        if (visited.find(it->first) == visited.end()) {
                                            it = adjacency_list.erase(it);  
                                        } else {
                                            ++it;
                                        }
                                    }

                                    bool isIn = false;
                                    
                                    for (const auto& entry : adjacency_list) {
                                        const auto& node_id = entry.first;
                                        const auto& node_ptr = entry.second;
                                        if (node_id == conn_ptr->nodeid) continue; // skip SELF

                                        shared_ptr<Node> current = node_ptr;

                                        
                                        while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != conn_ptr->nodeid) {
                                            
                                            current = adjacency_list[current->nodeid]->pred;
                                        }
                                        //cout << target << " " << node_id << " " << adjacency_list[current->nodeid]->pred->nodeid << endl;
                                        if (node_id == target && adjacency_list[current->nodeid]->pred->nodeid == conn_ptr->nodeid) { // Should always be the case
                                            //cout << "Should route here: "<< current->nodeid << "\n";
                                            destination = current->nodeid;
                                            //cout << "I set destination to " << current->nodeid << endl;
                                            isIn = true;
                                        }
                                    }

                                    m.unlock();

                                    
                                    if (isIn = false){
                                        cout << target << "is not reachable\n";
                                    } else{
                                        shared_ptr<Message> msg = make_shared<Message>("UCASTAPP", "353NET/1.0", from);
                                        msg->msg_id = msg_id;
                                        msg->target = target;
                                        msg->number = ttl;
                                        msg->body = body;
                                        msg->nextLayer = nextLayer;
                                        msg->destination = destination;

                                        size_t first_colon = body.find(':');
                                        size_t second_colon = body.find(':', first_colon + 1);
                                        string seq_str, msg_str; 

                                        if (first_colon != std::string::npos && second_colon != std::string::npos) {
                                            seq_str = body.substr(first_colon + 1, second_colon - first_colon - 1);
                                            msg_str = body.substr(second_colon + 1);
                                        } else {
                                            std::cerr << "Invalid packet format.\n";
                                        }



                                        int seq_num;
                                        if (seq_str.find('0') != -1){
                                            seq_num = 0;
                                        } else {
                                            seq_num = 1; 
                                        }

                                        int data_seq_num = seq_num;

                                        m.lock();
                                        for (auto& conn : connection_list){
                                            
                                            if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
                                                
                                                 conn->add_work(msg);

                                                 string msg_print = msg_str;
                                                 if (msg_print == "\n"){
                                                    msg_print = "\\n";
                                                 }
                                                 string route_rdt = "["+get_timestamp_now()+"] " + "f UCASTAPP :" + conn->neighbor_nodeid +
                                                 " " + to_string(ttl) + " - " + to_string(content_length) + " :" + 
                                                 msg->msg_id + " " + " :" + msg->nodeid + " :" +  target + " 2 " + to_string(data_seq_num) + " - 1 "+ " " + msg_print + "\n";
                                                 //cout << route_utp;
                                                 LogALine(route_rdt, "U");
                                            }
                                        }
                                        m.unlock();

                                          
                                    }   
                            } 
                        } // end for forwarding
                        


                    } else {
                        string received = "["+get_timestamp_now()+"] " + "r UCASTAPP :" + conn_ptr->neighbor_nodeid +
                                             " " + to_string(ttl) + " - " + to_string(content_length) + " :" + 
                                             msg_id + " " + " :" + from + " :" +  target + " " + nextLayer + " " + body + "\n";
                        //cout << received;
                        LogALine(received, "U");

                        if (conn_ptr->nodeid == target){
                            string NL;
                            if (nextLayer == "1"){
                                NL = "UDT";
                            } else {
                                NL = "RDT";
                            }

                            //cout << "No receiver for " << NL << " message '"<< body << "' from :" << from << ".\n";
                            // UDT application message: 
                            bool isPing = false;
                            bool isTTLZERO = false;
                            bool isPong = false; 
                            if (body.find("PING") != -1){
                                //cout << "Was I here?" << endl;
                                isPing = true;
                                std::istringstream iss(body);
                                std::string v, command, sesid;

                                iss >> v >> command >> sesid;
                                string msg = "353UDT/1.0 PONG " + sesid;


                                // Event ev;
                                // ev.type = EVENT_PONG;
                                // ev.sesid = sesid;
                                // cout << sesid;
                                // gettimeofday(&ev.timestamp, NULL);
                                     
                                // ev.from_node = conn_ptr->nodeid;

                                // cout << "Adding pong" << endl;
                                // traceroute_add_work(ev);


                                //udt_send(string src_nodeid, string message, string target, shared_ptr<Connection> conn_ptr)
                                udt_send(conn_ptr->nodeid, msg, from, conn_ptr);
                            } else if (body.find("TTLZERO") != -1){
                                //string sesid, time;
                                //GetObjID(conn_ptr->nodeid, "ses", sesid, time);

                                std::istringstream iss(body);
                                std::string v, command, msgsesid;

                                iss >> v >> command >> msgsesid;
                                if (tracerouteSesID == msgsesid){
                                    Event ev;
                                    ev.type = EVENT_TTLZERO;
                                    ev.sesid = msgsesid;
                                    gettimeofday(&ev.timestamp, NULL);
                                     
                                    ev.from_node = from;

                                    traceroute_add_work(ev); 
                                }


                                isTTLZERO = true;

                            } else if (body.find("RDTACK") != -1){
                                std::istringstream iss(body);
                                std::string protocol, command, seq_str, app_no;

                                iss >> protocol >> command >> seq_str >> app_no;

                                Event ev;
                                ev.type = EVENT_RDTACK;

                                ev.seq_num = stoi(seq_str);
                                gettimeofday(&ev.timestamp, NULL);
                                ev.from_node = from;

                                
                                rdt_add_work(ev);
                                

                            } else {

                                //string sesid, time;
                                //GetObjID(conn_ptr->nodeid, "ses", sesid, time);

                                std::istringstream iss(body);
                                std::string v, command, msgsesid;

                                iss >> v >> command >> msgsesid;
                                //cout << tracerouteSesID << " " << msgsesid << endl;
                                if (tracerouteSesID == msgsesid){
                                    //cout << "I got a PONG" << endl;
                                    Event ev;
                                    ev.type = EVENT_PONG;
                                    ev.sesid = msgsesid;
                                     
                                    gettimeofday(&ev.timestamp, NULL);
                                    ev.from_node = from;

                                    traceroute_add_work(ev); 
                                }
                                isPong = true;
                            }
                        } else {
                            
                            string destination; 
                            ttl -= 1;

                            if (ttl == 0){
                                bool isPing = false;
                                if (body.find("PING") != -1) {
                                    isPing = true;
                                }

                                string ttlTarget = from;
            

                                if (isPing){
                                    vector<string> neighbor_node_ids;

                                    m.lock();

                                    for (const auto& conn : connection_list) {
                                        //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                                        if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0) {  
                                            neighbor_node_ids.push_back(conn->neighbor_nodeid);
                                        }
                                    }
                                    m.unlock();
                                    
                                    if (neighbor_node_ids.size() == 0){
                                        cout << ttlTarget << "is not reachable\n";
                                    } else {
                                        m.lock();
                                        
                                        struct timeval blank = {0, 0};

                                        shared_ptr<Node> start_node = make_shared<Node>(conn_ptr->nodeid, blank, blank, "", neighbor_node_ids);
                                        unordered_set<string> visited;
                                        queue<shared_ptr<Node>> BFS_q;
                                        BFS_q.push(start_node);
                                        visited.insert(conn_ptr->nodeid);

                                        while(!BFS_q.empty()){
                                            shared_ptr<Node> current = BFS_q.front();
                                            BFS_q.pop();
                                            for (const string& neighborID: current->neighbors){
                                                //cout << neighborID << endl;
                                                if (visited.find(neighborID) == visited.end()) {
                                                    visited.insert(neighborID);
                                                    adjacency_list[neighborID]->pred = current; // Set pred
                                                    BFS_q.push(adjacency_list[neighborID]);
                                                }
                                            }
                                            
                                        }
                                        
                                        for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                                            if (visited.find(it->first) == visited.end()) {
                                                it = adjacency_list.erase(it);  
                                            } else {
                                                ++it;
                                            }
                                        }

                                        bool isIn = false;
                                        
                                        for (const auto& entry : adjacency_list) {
                                            const auto& node_id = entry.first;
                                            const auto& node_ptr = entry.second;
                                            if (node_id == conn_ptr->nodeid) continue; // skip SELF

                                            shared_ptr<Node> current = node_ptr;

                                            
                                            while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != conn_ptr->nodeid) {
                                                
                                                current = adjacency_list[current->nodeid]->pred;
                                            }
                                            //cout << target << " " << node_id << " " << adjacency_list[current->nodeid]->pred->nodeid << endl;
                                            if (node_id == ttlTarget && adjacency_list[current->nodeid]->pred->nodeid == conn_ptr->nodeid) { // Should always be the case
                                                //cout << "Should route here: "<< current->nodeid << "\n";
                                                destination = current->nodeid;
                                                //cout << "I set destination to " << current->nodeid << endl;
                                                isIn = true;
                                            }
                                        }

                                        m.unlock();
                                    }
                                    shared_ptr<Message> msg = make_shared<Message>("UCASTAPP", "353NET/1.0", conn_ptr->nodeid);;
                                    msg->target = from;
                                    msg->number = max_ttl;
                                    msg->nextLayer = "1";
                                    std::istringstream iss(body);
                                    std::string v, command, sesid;

                                    iss >> v >> command >> sesid;
                                    sesid.erase(0, sesid.find_first_not_of(" \t\r\n"));
                                    sesid.erase(sesid.find_last_not_of(" \t\r\n") + 1);
                                    msg->body = "353UDT/1.0 TTLZERO " + sesid;
                                    msg->destination = destination;
                                    m.lock();
        
                                    string id, origin_time;
                                    GetObjID(msg->nodeid, "msg", id, origin_time);
                                    m.unlock();
                                    msg->msg_id = id;

                                    m.lock();
                                    for (auto& conn : connection_list){
                                            
                                            if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
                                                
                                                 conn->add_work(msg);
                                                 //cout << msg->msg_id << "hi" << endl;

                                                 string send_utp = "["+get_timestamp_now()+"] " + "i UCASTAPP :" + conn->neighbor_nodeid +
                                                 " " + to_string(max_ttl) + " - " + to_string(content_length) + " :" + 
                                                 msg->msg_id + " " + ":" + msg->nodeid + " :" +  target + " 1 " + msg->body + "\n";
                                                 //cout << send_utp;
                                                 LogALine(send_utp, "U");

                                            }
                                    }
                                    m.unlock(); 
                                    
                                }
                            } else if (ttl != 0){
                                vector<string> neighbor_node_ids;

                                m.lock();

                                for (const auto& conn : connection_list) {
                                    //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                                    if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0) {  
                                        neighbor_node_ids.push_back(conn->neighbor_nodeid);
                                    }
                                }
                                m.unlock();
                                
                                if (neighbor_node_ids.size() == 0){
                                    cout << target << "is not reachable\n";
                                } else {
                                    m.lock();
                                    
                                    struct timeval blank = {0, 0};

                                    shared_ptr<Node> start_node = make_shared<Node>(conn_ptr->nodeid, blank, blank, "", neighbor_node_ids);
                                    unordered_set<string> visited;
                                    queue<shared_ptr<Node>> BFS_q;
                                    BFS_q.push(start_node);
                                    visited.insert(conn_ptr->nodeid);

                                    while(!BFS_q.empty()){
                                        shared_ptr<Node> current = BFS_q.front();
                                        BFS_q.pop();
                                        for (const string& neighborID: current->neighbors){
                                            //cout << neighborID << endl;
                                            if (visited.find(neighborID) == visited.end()) {
                                                visited.insert(neighborID);
                                                adjacency_list[neighborID]->pred = current; // Set pred
                                                BFS_q.push(adjacency_list[neighborID]);
                                            }
                                        }
                                        
                                    }
                                    
                                    for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                                        if (visited.find(it->first) == visited.end()) {
                                            it = adjacency_list.erase(it);  
                                        } else {
                                            ++it;
                                        }
                                    }

                                    bool isIn = false;
                                    
                                    for (const auto& entry : adjacency_list) {
                                        const auto& node_id = entry.first;
                                        const auto& node_ptr = entry.second;
                                        if (node_id == conn_ptr->nodeid) continue; // skip SELF

                                        shared_ptr<Node> current = node_ptr;

                                        
                                        while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != conn_ptr->nodeid) {
                                            
                                            current = adjacency_list[current->nodeid]->pred;
                                        }
                                        //cout << target << " " << node_id << " " << adjacency_list[current->nodeid]->pred->nodeid << endl;
                                        if (node_id == target && adjacency_list[current->nodeid]->pred->nodeid == conn_ptr->nodeid) { // Should always be the case
                                            //cout << "Should route here: "<< current->nodeid << "\n";
                                            destination = current->nodeid;
                                            //cout << "I set destination to " << current->nodeid << endl;
                                            isIn = true;
                                        }
                                    }

                                    m.unlock();

                                    
                                    if (isIn = false){
                                        cout << target << "is not reachable\n";
                                    } else{
                                        shared_ptr<Message> msg = make_shared<Message>("UCASTAPP", "353NET/1.0", from);
                                        msg->msg_id = msg_id;
                                        msg->target = target;
                                        msg->number = ttl;
                                        msg->body = body;
                                        msg->nextLayer = nextLayer;
                                        msg->destination = destination;

                                        m.lock();
                                        for (auto& conn : connection_list){
                                            
                                            if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
                                                
                                                 conn->add_work(msg);

                                                
                                                 string route_utp = "["+get_timestamp_now()+"] " + "f UCASTAPP :" + conn->neighbor_nodeid +
                                                 " " + to_string(ttl) + " - " + to_string(content_length) + " :" + 
                                                 msg->msg_id + " " + " :" + msg->nodeid + " :" +  target + " " + msg->nextLayer + " " + msg->body + "\n";
                                                 cout << route_utp;
                                                 LogALine(route_utp, "U");
                                            }
                                        }
                                        m.unlock();

                                          
                                    }
                                }
                            }
                        }
                    } // END OF PROCESSING NON_RDT UCSTAPP MESSAGES

                    
                    conn_ptr->m->unlock(); // Added this now for unlocking

                } else { // beginning of processing LSUPDATE messages 
                    int ttl;
                        int flood;
                        string msg_id;
                        string from;
                        string origin_start_time;
                        int content_length;

                        for (const auto& line : header_lines) {
                            if (line.find("TTL:") == 0) {
                                ttl = stoi(line.substr(5));  
                            }
                            else if (line.find("Flood:") == 0) {
                                string flood_value = line.substr(7);
                                size_t reason_pos = flood_value.find(";");
                                if (reason_pos != string::npos) {
                                    flood_value = flood_value.substr(0, reason_pos);  
                                }
                                flood = stoi(flood_value);  
                            }
                            else if (line.find("MessageID:") == 0) {
                                msg_id = line.substr(11);  
                            }
                            else if (line.find("From:") == 0) {
                                from = line.substr(6);  
                            }
                            else if (line.find("OriginStartTime:") == 0) {
                                origin_start_time = line.substr(17);  
                            } else if (line.find("Content-Length:") == 0){
                                content_length = stoi(line.substr(16));
                            }
                        }

                        int bytes_received = 0;
                        
                        string body;
                        char buffer[content_length + 1];



                        //cout << content_length << endl;
                        while (bytes_received < content_length){
                            
                            int to_read = std::min(content_length - bytes_received, (int)sizeof(buffer));
                            
                            int chunk_length = read(conn_ptr->socket_fd, buffer, to_read);
                            
                            //conn_ptr->m->lock();
                            if (chunk_length <= 0) {
                                m.unlock();
                                m.lock();
                                conn_ptr->m->unlock();
                                conn_ptr->m->lock();
                                if (conn_ptr->socket_fd >= 0){
                                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                                }
                                conn_ptr->socket_fd = -1;
                                //close(conn_ptr->orig_socket_fd);
                                conn_ptr->m->unlock();
                                m.unlock();

                                conn_ptr->add_work(NULL);
                                initiate_LSUPDATE_flood(2, conn_ptr);
                                conn_ptr->write_thread_ptr->join();

                                reaper_add_work(conn_ptr);
                                    
                                string line_closed = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tConnection closed with client at " + client_ip_and_port + "\n";
                                //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                                //LogALine(line_closed);

                                    
                                return;
                            } 
                            //conn_ptr->m->unlock();

                            body.append(buffer, chunk_length);

                            bytes_received += chunk_length;

                        }

                        body += "\0";

                        

                        vector<string> neighbor_node_ids;
                        
                        size_t start = 0;
                        size_t end = body.find(',');

                        while (end != string::npos) {
                                string node_id = body.substr(start, end - start);
                                node_id.erase(remove_if(node_id.begin(), node_id.end(), ::isspace), node_id.end());
                                neighbor_node_ids.push_back(node_id);
                                start = end + 1;
                                end = body.find(',', start);
                                
                        }

                        if (start < body.size()) {
                                string node_id = body.substr(start);
                                node_id.erase(remove_if(node_id.begin(), node_id.end(), ::isspace), node_id.end());
                                neighbor_node_ids.push_back(node_id);
                                
                        }
                        

                        
                        msg_id.erase(msg_id.find_last_not_of(" \t\n\r\f\v") + 1);
                        origin_start_time.erase(origin_start_time.find_last_not_of(" \t\n\r\f\v") + 1);
                        from.erase(from.find_last_not_of(" \t\n\r\f\v") + 1);

                        //int printCL2 = content_length + 1;
                        string printBody2 = "";
                        istringstream iss(body);
                        vector<string> parts2;
                        string segment2;

                        while (getline(iss, segment2, ',')) {
                                            parts2.push_back(segment2);
                        }
                        for (size_t i = 0; i < parts2.size(); ++i) {
                                            printBody2 += ":"+parts2[i];
                                            if (i != parts2.size() - 1) {
                                                printBody2 += ",";
                                            }
                        }
                        int printCL2 = content_length + parts2.size();
                        string receive_lsupdate = "["+get_timestamp_now()+"] " + "r LSUPDATE :" + conn_ptr->neighbor_nodeid + " " + to_string(ttl) + " F " + to_string(printCL2) + " :" + 
                        msg_id + " " + origin_start_time + " :" + from + " " + "(" + printBody2 + ")\n";
                        LogALine(receive_lsupdate, "L");
                                

                        m.lock();
                        //cout << "Message in question: " << from << "; " << msg_id << endl;
                        if (msg_cache.find(msg_id) == msg_cache.end()){
                            
                            msg_cache[msg_id] = from; // Add this message from this node to the msg_cache
                            m.unlock();
                            //cout << "Message flooded: " << from << msg_id << endl;
                            if (flood == 1){
                                ttl -= 1;
                                
                                
                                m.lock();
                                
                                //cout << conn_ptr->nodeid << " now owns the mutex. 8" << endl;
                                for (auto& d : connection_list) {
                                    
                                    //cout << d->neighbor_nodeid << endl;
                                    if (d->neighbor_nodeid != "" && d != conn_ptr && d->socket_fd >= 0) {
                                        
                                        shared_ptr<Message> msg = make_shared<Message>("LSUPDATE", "353NET/1.0", from);
                                        //string send_flood = "["+get_timestamp_now()+"] " + "d LSUPDATE " + conn_ptr->neighbor_nodeid +
                                                    //" " + to_string(ttl) + " F " + to_string(content_length) + " " + 
                                                    //msg_id + " " + origin_start_time + " " + from + " " + "(" + body + ")\n";
                                        //int printCL = content_length + 1;
                                        string printBody = "";
                                        istringstream iss(body);
                                        vector<string> parts;
                                        string segment;

                                        while (getline(iss, segment, ',')) {
                                            parts.push_back(segment);
                                        }
                                        for (size_t i = 0; i < parts.size(); ++i) {
                                            printBody += ":"+parts[i];
                                            if (i != parts.size() - 1) {
                                                printBody += ",";
                                            }
                                        }
                                        int printCL = content_length + parts.size();

                                        string send_flood = "["+get_timestamp_now()+"] " + "d LSUPDATE :" + d->neighbor_nodeid +
                                                    " " + to_string(ttl) + " F " + to_string(printCL) + " :" + 
                                                    msg_id + " " + origin_start_time + " :" + from + " " + "(" + printBody + ")\n";
                                        LogALine(send_flood, "L");
               
                                        
                                        msg->msg_id = msg_id;
                                        msg->origin_start_time = origin_start_time;
                                        msg->number = ttl;
                                        msg->nodeid = from;
                                        msg->body = body;
                                        d->add_work(msg);

                                        //conn_ptr->m->lock();


                                    }
                                }
                                m.unlock();
                                //cout << conn_ptr->nodeid << " now releases the mutex. 8" << endl;

                            }


                        }
                        m.unlock();

                        string origin_node = from; 
                        int reason = -1;


                        
        /*                cout << origin_node << endl;
                        cout << "adjacency_list" << endl;
                        for (const auto& pair : adjacency_list) {
                            cout << "Key (origin_node): " << pair.first << endl;
                            shared_ptr<Node> node3 = pair.second;
                            cout << "  Node ID: " << node3->nodeid << endl;
                            cout << "  Neighbors: " << node3->link_state << endl;
                            cout << endl << "--------------------------------" << endl;
                        } */
                        
                        m.lock(); // If not already locked
                        bool present = (adjacency_list.find(origin_node) != adjacency_list.end());
                        
                        //cout << "Result: " << (present ? "FOUND (should go to 7.0)" : "NOT FOUND (should go to 6.0)") << endl;
                        
                        if (present){
                            /* node and origin_node refer to the same remote node */
                            
                            shared_ptr<Node> node = adjacency_list[origin_node];
                            m.unlock();
                            
                            struct timeval tv2; // msg_timestamp 
                            size_t first_underscore = msg_id.find('_');
                            if (first_underscore != std::string::npos) {
                                size_t second_underscore = msg_id.find('_', first_underscore + 1);
                                if (second_underscore != std::string::npos) {

                                        std::string timestamp = msg_id.substr(second_underscore + 1);

                                        if (sscanf(timestamp.c_str(), "%ld.%6ld", &tv2.tv_sec, &tv2.tv_usec) != 2) {
                                            printf("Error parsing time string into timestamp\n");
                                            return;
                                        }
                                } 
                            }

                            if (timestamp_diff_in_seconds(&node->timestamp, &tv2) > 0){

                                struct timeval m_origin_start_time;
                                
                                if (sscanf(origin_start_time.c_str(), "%ld.%6ld", &m_origin_start_time.tv_sec, &m_origin_start_time.tv_usec) != 2) {
                                    //cout << origin_start_time << endl;
                                    //cout << "Here: " << origin_start_time << endl; 
                                    printf("Error parsing time string into timestamp\n");
                                    return;
                                }

                                if (timestamp_diff_in_seconds(&node->origin_start_time, &m_origin_start_time) > 0){
                                    m.lock();
                                    bool isNeighbor = false;
                                    for (auto& c : connection_list) {
                                        //cout << "Neighbor: " << c->neighbor_nodeid << endl;
                                        if (c->neighbor_nodeid == origin_node && c->socket_fd >= 0){
                                            isNeighbor = true;
                                        }
                                        //cout << "For reason 3, origin node is: " << origin_node << endl;
                                        //reason = 3;
                                    }
                                    m.unlock();

                                    if (!isNeighbor){
                                        reason = 4;
                                    }
                                }

                                
                                node->timestamp = tv2;
                                node->link_state = body;
                                node->neighbors = neighbor_node_ids;
                                m.lock();
                                adjacency_list[origin_node] = node;
                                m.unlock();

                            

                            
                        } 
                    } else {
                            //Adding code for reason 3
                            
                            //cout << conn_ptr->nodeid << " now owns the mutex. 9" << endl;
                            string node_id, origin_time;
                            GetObjID(origin_node, "node", node_id, origin_time);
                            m.unlock();
                            //cout << conn_ptr->nodeid << " now releases the mutex. 9" << endl;

                            struct timeval tv;
                            struct timeval tv2;
                            
                            //cout << origin_time << "Heheh" << endl;
                            if (sscanf(origin_time.c_str(), "%ld.%6ld", &tv.tv_sec, &tv.tv_usec) != 2) {
                                printf("Error parsing time string into timestamp\n");
                                return;
                            }

                            size_t first_underscore = msg_id.find('_');
                            if (first_underscore != std::string::npos) {
                                size_t second_underscore = msg_id.find('_', first_underscore + 1);
                                if (second_underscore != std::string::npos) {

                                    std::string timestamp = msg_id.substr(second_underscore + 1);


                                    if (sscanf(timestamp.c_str(), "%ld.%6ld", &tv2.tv_sec, &tv2.tv_usec) != 2) {
                                        printf("Error parsing time string into timestamp\n");
                                        return;
                                    }
                                } 


                            shared_ptr<Node> node = make_shared<Node>(origin_node, tv2, tv, body, neighbor_node_ids);
                            //cout << "AM I segfualting here?" << endl;
                            m.lock();
                            //cout << "--- Adding " << origin_node << " to adjacency_list with message " << msg_id << endl;
                            adjacency_list[origin_node] = node;
                            m.unlock();

                            //auto it = std::find(neighbor_node_ids.begin(), neighbor_node_ids.end(), origin_node);
                            //cout << "Origin node is " << origin_node << endl;

                            m.lock();
                            bool isNeighbor = false;
                            for (auto& c : connection_list) {
                                //cout << "Neighbor: " << c->neighbor_nodeid << endl;
                                if (c->neighbor_nodeid == origin_node && c->socket_fd >= 0){
                                    isNeighbor = true;
                                }
                                //cout << "For reason 3, origin node is: " << origin_node << endl;
                                //reason = 3;
                            }
                            m.unlock();

                            if (!isNeighbor){
                                reason = 3;
                            }
                            
                            







            
                            
                        }


                        


                    }
                    conn_ptr->m->unlock();
                    
                    if (reason != -1){
                        initiate_LSUPDATE_flood(reason, conn_ptr);
                        //cout << "Came here 4 " << endl;
                    }
                }
                /* process UCASTAPP message */
                
                        
            //cout << "Came here 5 " << endl;
                

            }
            conn_ptr->m->unlock(); // Added this now for unlocking
        }

        if (conn_ptr->socket_fd >= 0){
            shutdown(conn_ptr->socket_fd, SHUT_RDWR);
        }
        conn_ptr->socket_fd = -1;
        //close(conn_ptr->orig_socket_fd);
        conn_ptr->m->unlock();

        conn_ptr->add_work(NULL);

        initiate_LSUPDATE_flood(2, conn_ptr);
        conn_ptr->write_thread_ptr->join();
                      

        reaper_add_work(conn_ptr);

        
    }

}

    


static void write_from_client(shared_ptr<Connection> conn_ptr) {
    conn_ptr->m->lock();
    conn_ptr->m->unlock();
    bool exit = false;

    while(!exit){
        //cout << "I was called" << endl;
        shared_ptr<Message> msg = conn_ptr->wait_for_work();
        //cout << conn_ptr->nodeid << endl;
        if (!msg){
            string line_write_dead = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tSocket-writing thread has terminated\n";
            //LogALine(line_write_dead);
            break;
        }


        if (msg->method == "SAYHELLO"){
            //std::cout << "i SAYHELLO sent from " << conn_ptr->nodeid << std::endl;

            string message_string = "353NET/1.0 SAYHELLO\r\nTTL: 1\r\nFlood: 0\r\nFrom: "+conn_ptr->nodeid+"\r\nContent-Length: 0\r\n\r\n";
            int return_code = better_write_header(conn_ptr->socket_fd, message_string.c_str(), static_cast<int>(message_string.length()));
            if (return_code == -1){
                m.unlock();
                m.lock();
                conn_ptr->m->unlock();
                conn_ptr->m->lock();
                if (conn_ptr->socket_fd >= 0){
                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                }
                conn_ptr->socket_fd = -1;
                
                conn_ptr->m->unlock();
                m.unlock();
                
                break;
            } 

            string msg_sent =  "["+get_timestamp_now()+"] i SAYHELLO :" + conn_ptr->neighbor_nodeid + " 1 - 0\n";
            LogALine(msg_sent, "H");
            //cout << msg_sent;

            
            
        } else if (msg->method == "LSUPDATE"){
            //cout << conn_ptr->socket_fd << endl;
            //cout << "Got work from " << msg->nodeid << endl;
            msg->body.erase(msg->body.find_last_not_of(" \t\r\n") + 1);
            string message_string = "353NET/1.0 LSUPDATE\r\nTTL: " + to_string(msg->number)+"\r\nFlood: 1; reason=REASON\r\nMessageID: " + msg->msg_id + "\r\nFrom: " + msg->nodeid + "\r\nOriginStartTime: "+ 
            msg->origin_start_time + "\r\nContent-Length: " + to_string(msg->body.size()) + "\r\n\r\n" + msg->body;
            //cout << "I'm writing a message " << message_string << endl;
            int return_code = better_write(conn_ptr->socket_fd, message_string.c_str(), static_cast<int>(message_string.length()));
            if (return_code == -1){
                m.unlock();
                m.lock();
                conn_ptr->m->unlock();
                conn_ptr->m->lock();
                if (conn_ptr->socket_fd >= 0){
                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                }
                conn_ptr->socket_fd = -1;
                
                conn_ptr->m->unlock();
                m.unlock();
                
                break;
            } 

        } else if (msg->method == "UCASTAPP"){
            string message_string = "353NET/1.0 UCASTAPP\r\nTTL: " + to_string(msg->number)+"\r\nFlood: 0\r\nMessageID: " + msg->msg_id + "\r\nFrom: " + msg->nodeid + "\r\nTo: "+ 
            msg->target + "\r\nNext-Layer: " + msg->nextLayer +"\r\nContent-Length: " + to_string(msg->body.size()) + "\r\n\r\n" + msg->body;
            
            int return_code = better_write(conn_ptr->socket_fd, message_string.c_str(), static_cast<int>(message_string.length()));
            if (return_code == -1){
                m.unlock();
                m.lock();
                conn_ptr->m->unlock();
                conn_ptr->m->lock();
                if (conn_ptr->socket_fd >= 0){
                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                }
                conn_ptr->socket_fd = -1;
                
                conn_ptr->m->unlock();
                m.unlock();
                
                break;
            } 

            

        }
        
    }
}

class TracerouteCallback : public TimerCallback {
public:
    void add_work(Event& ev) override {
        traceroute_add_work(ev); // You define this function elsewhere
    }

    void wait_for_work() override {
        traceroute_wait_for_work();
    }
};

class RDTCallback : public TimerCallback {
public:
    void add_work(Event& ev) override {
        rdt_add_work(ev); // You define this function elsewhere
    }

    void wait_for_work() override {
        rdt_wait_for_work();
    }
};

// static void timer_thread(shared_ptr<Timer> t){
//     int ticks_remaining = t->expiration_time * 10;

//     while(true){
//         usleep(100000);
//         m.lock();
//         if (t->cancelled == true){
//             m.unlock();
//             return;
//         }
//         m.unlock();
//         ticks_remaining = ticks_remaining - 1; 
//         if (ticks_remaining <= 0){
//             break;
//         }
//     }

//     Event ev;
//     ev.sesid = t->sesid;
//     ev.type = EVENT_TIMEOUT;

//     t->callback->add_work(ev);





// }

static void neighbors_thread(vector<string>& file_lines, string my_port, string host){
    int neighbor_retry_interval; 

    for (const auto& line : file_lines) {
        if (line.empty() || line[0] == ';') {
            continue;
        }

        
        size_t pos = line.find("neighbor_retry_interval");
        if (pos != string::npos) {
            size_t equals_pos = line.find("=", pos);
            if (equals_pos != string::npos) {
                string value = line.substr(equals_pos + 1);
                neighbor_retry_interval = stoi(value);
            }
            break; 
        }
    }

    vector<string> potential_neighbors;

    int topologyLoc = -1;
    for (const auto& section : getSections(file_lines)) {
        if (section.first == "topology") {
            topologyLoc = section.second;
            break;
        }
    }

    
    int i = topologyLoc + 1;
    string target = ":" + my_port + "=";
    while (i < file_lines.size() && file_lines[i][0] != '[') { 
        string line = file_lines[i];

        if (line.find(target) == 0) {
            size_t equals_pos = line.find("=");
            if (equals_pos != string::npos) {
                string neighbors_str = line.substr(equals_pos + 1); 

                
                stringstream ss(neighbors_str);
                string neighbor;
                while (getline(ss, neighbor, ',')) {
                    string neighborToAdd = neighbor.substr(1);
                    neighborToAdd.erase(std::remove_if(neighborToAdd.begin(), neighborToAdd.end(), ::isspace), neighborToAdd.end());
                    potential_neighbors.push_back(neighborToAdd);
                    //cout << "Hi " << neighborToAdd << endl;
                }
            }
            break; 
        }
        i++;
    }

    while (true){
        vector<string> l = potential_neighbors;
        m.lock();

        if (listen_socket_fd == -1){
            m.unlock();
            break;
        } else {
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->neighbor_nodeid != "") {
                    
                    auto it = find(l.begin(), l.end(), conn_ptr->neighbor_nodeid);
                    //cout << "Checking if " << conn_ptr->neighbor_nodeid << " is in l" << conn_ptr->nodeid << endl;

                    if (it != l.end()) {
                        l.erase(it);
                        //cout << "Removed " << conn_ptr->neighbor_nodeid << endl;
                    }
                }
            } 
        }

        m.unlock();
        
        /* l is now a list of inactive neighbors */
        for (const auto& n : l) {
            //cout << n << endl;
            std::stringstream ss(n);
            std::string stripped;
            ss >> stripped;
            //cout << "Creating for " << stripped << endl;
            int socket_fd = create_client_socket_and_connect(host, stripped);


            if (socket_fd >= 0){
                m.lock();
                
                // Create a shared pointer to the Connection object
                shared_ptr<Connection> conn_ptr = make_shared<Connection>(Connection(next_conn_number++, socket_fd, NULL, NULL));

                // Create two threads: one for reading and one for writing
                shared_ptr<thread> read_thr_ptr = make_shared<thread>(read_from_client, conn_ptr);  // Thread to handle reading from the client
                shared_ptr<thread> write_thr_ptr = make_shared<thread>(write_from_client, conn_ptr);  // Thread to handle writing to the client

                // Assign the threads to the connection object
                conn_ptr->read_thread_ptr = read_thr_ptr;
                conn_ptr->write_thread_ptr = write_thr_ptr;

                conn_ptr->neighbor_nodeid = stripped; 

                conn_ptr->nodeid = my_port;
                //cout << "Setting " << conn_ptr->nodeid << "neighbor to " << stripped << endl;

                // Push the connection object into the connection list
                connection_list.push_back(conn_ptr);
                //cout << "push(b) " << conn_ptr->nodeid << " with neighbor " << conn_ptr->neighbor_nodeid << endl;

                string nodeid = my_port;
                
                shared_ptr<Message> w = make_shared<Message>("SAYHELLO", "353NET/1.0", nodeid);
                conn_ptr->add_work(w);

                m.unlock();
            }
        }

        sleep(neighbor_retry_interval);
    }
    


}

/**
 * This function is not used since we are using getline().
 *
 * Note: just like getline(), the returned string does not contain a trailing '\n'.
 */
string readline()
{
    string message;
    for (;;) {
        char ch=(char)(cin.get() & 0xff);
        if (cin.fail()) {
            break;
        }
        if (ch == '\n') {
            break;
        }
        message += ch;
    }
    return message;
}

void rdt_send(string port, string destination, string message, string target){
    int seq_num = 0; 
    int N = message.size();

    for (int i = 0; i < N; i ++){
        struct timeval starttime;
        gettimeofday(&starttime, NULL);

        string packet = make_data_pkt(seq_num, message[i]);
        //cout << packet << endl;

        // Relevant part of udt_send //
        shared_ptr<Message> msg = make_shared<Message>("UCASTAPP", "353NET/1.0", port);
        msg->body = packet;
        int content_length = message.size();

        m.lock();
        
        string msg_id, origin_time;
        GetObjID(msg->nodeid, "msg", msg_id, origin_time);
        m.unlock();
        

        msg->msg_id = msg_id;
        msg->origin_start_time = origin_time;
        msg->number = max_ttl;
        
        msg->target = target;
        msg->nextLayer = "1";
        msg->destination = destination;
        m.lock();
        for (auto& conn : connection_list){
            
            if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
                
                 conn->add_work(msg);

                 string m_print(1, message[i]);
                 if (message[i] == '\n'){
                    m_print = "\\n";
                 }

                 string send_rdt = "["+get_timestamp_now()+"] " + "i UCASTAPP :" + conn->neighbor_nodeid +
                 " " + to_string(max_ttl) + " - " + to_string(content_length) + " :" + 
                 msg->msg_id + " " + " :" + msg->nodeid + " :" +  target + " 2 " + to_string(seq_num) + " - 1 " + m_print + "\n";
                 //cout << send_utp;
                 LogALine(send_rdt, "U");
            }
        }
        m.unlock();

        RDT30_State sendState;
        sendState.peer_nodeid = port;
        sendState.seq_no = seq_num;
        sendState.app_no = 0;
        sendState.sndpkt = packet;
        m.lock();
        rdt_sessions.push_back(sendState);
        m.unlock();



        // End relevant part of udt_send //

        shared_ptr<TimerCallback> cb = make_shared<RDTCallback>();
        shared_ptr<Timer> timer = make_shared<Timer>(msg_lifetime, cb, "dummy");

        timer->start();

        bool done_waiting = false; 

        while (!done_waiting){
            Event ev = rdt_wait_for_work();


            if (ev.type == EVENT_RDTACK){
                if (ev.seq_num == seq_num){
                    timer->stop();
                    done_waiting = true;
                }

                
            } else if (ev.type == EVENT_TIMEOUT){
                cout << "RDT sender timeout for " + to_string(seq_num) + " and RDT-App-Number: 0\n";
                timer->thread_ptr->join();

                // RELEVANT PORTION OF UDT_SEND //
                m.lock();
                for (auto& conn : connection_list){
                    
                    if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
                        
                         conn->add_work(msg);

                         string send_rdt = "["+get_timestamp_now()+"] " + "i UCASTAPP :" + conn->neighbor_nodeid +
                         " " + to_string(max_ttl) + " - " + to_string(content_length) + " :" + 
                         msg->msg_id + " " + " :" + msg->nodeid + " :" +  target + " 2 " + to_string(seq_num) + " - 1 " + msg->body + "\n";
                         //cout << send_utp;
                         LogALine(send_rdt, "U");
                    }
                }
                m.unlock();

                // END RELEVANT PORTION OF UDT_SEND //

                timer->start();

            }
        } // End while

        timer->thread_ptr->join();

        struct timeval now;
        gettimeofday(&now, NULL);

        double elapsed_time = timestamp_diff_in_seconds(&starttime, &now);
        if (elapsed_time < 1.0){
            int difference = (int)(1000000 - elapsed_time * 1000000);
            usleep(difference);
        }

        if (seq_num == 0){
            seq_num = 1; 
        } else if (seq_num == 1){
            seq_num = 0; 
        }

    }

    if (!message.empty() && message.back() == '\n') {
        message.pop_back();
    }

    cout << "rdtsend: '" << message << "' to :" << target << " have been sent and acknowledged\n";
}



void console_thread(string port) {
    string command;

    while (true) {
        cout << ":" + port + "> ";
        getline(cin, command);
        if (command == "neighbors"){
            m.lock();

            bool has_active_connections = false;
            //cout << connection_list.size() << endl;
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->neighbor_nodeid != "" && conn_ptr->socket_fd >= 0) {  
                    has_active_connections = true;
                    break;
                }
            }

            if (!has_active_connections) {
                cout << ":" << port << " has no active neighbors" << endl;
            } else {
                cout << "Active neighbors of :" << port << ":" << endl;
                vector<string> valid_neighbors;
                for (const auto& conn_ptr : connection_list) {
                    
                    //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                    if (conn_ptr->neighbor_nodeid != "" && conn_ptr->socket_fd >= 0) {  
                        valid_neighbors.push_back(conn_ptr->neighbor_nodeid);
                        //cout << "\t:" << conn_ptr->neighbor_nodeid << "," ;
                    }
                }

                cout << "\t";
                for (size_t i = 0; i < valid_neighbors.size(); i++){
                    cout << ":" << valid_neighbors[i];
                    if (i != valid_neighbors.size() - 1){
                        cout << ",";
                    }
                }
                cout << "\n";
            }
            m.unlock();

        } else if (command == "quit") {
            cout << "Console thread terminated\n" << endl;
            /*
            m.lock();
          
            shutdown(listen_socket_fd, SHUT_RDWR);
            close(listen_socket_fd);
            listen_socket_fd = -1;
                
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->socket_fd >= 0) {
                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                    conn_ptr->socket_fd = -2;
                }
            }
                
            

            m.unlock();
            if (listen_socket_fd == -1){
                break;
            }
            */
            break;
        } else if (command == "netgraph"){
            
            vector<string> neighbor_node_ids;

            m.lock();

            for (const auto& conn_ptr : connection_list) {
                //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                if (conn_ptr->neighbor_nodeid != "" && conn_ptr->socket_fd >= 0) {  
                    neighbor_node_ids.push_back(conn_ptr->neighbor_nodeid);
                }
            }
            m.unlock();

            if (neighbor_node_ids.size() == 0){
                cout << ":" << port << " has no active neighbors" << endl;
            } else {
                m.lock();
                struct timeval blank = {0, 0};

                shared_ptr<Node> start_node = make_shared<Node>(port, blank, blank, "", neighbor_node_ids);
                unordered_set<string> visited;
                queue<shared_ptr<Node>> BFS_q;
                BFS_q.push(start_node);
                visited.insert(port);

                while(!BFS_q.empty()){
                    shared_ptr<Node> current = BFS_q.front();
                    BFS_q.pop();

                    for (const string& neighborID: current->neighbors){
                        //cout << neighborID << endl;
                        if (visited.find(neighborID) == visited.end()) {
                            visited.insert(neighborID);
                            BFS_q.push(adjacency_list[neighborID]);
                        }
                    }
                    
                }

                for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                    if (visited.find(it->first) == visited.end()) {
                        it = adjacency_list.erase(it);  
                    } else {
                        ++it;
                    }
                }

                cout << ":" << port << ": ";
                for (size_t i = 0; i < neighbor_node_ids.size(); ++i) {
                    cout << ":" << neighbor_node_ids[i];
                    if (i != neighbor_node_ids.size() - 1) {
                        cout << ",";
                    }
                }
                cout << "\n";

                for (const auto& pair : adjacency_list) {
                    //cout << pair.first << "is in adjacency_list" << endl;
                    if (pair.first != port){
                        cout << ":" << pair.first << ": ";
                        for (size_t j = 0; j < pair.second->neighbors.size(); j++){
                            cout << ":" << pair.second->neighbors[j];
                            if (j != pair.second->neighbors.size()-1){
                                cout << ",";
                            }
                        }
                        cout << "\n";
                        //cout << pair.first << ": " << pair.second->link_state << endl;
                    }
                    
                }
                m.unlock();
            }
            

        } else if (command == "forwarding"){
            vector<string> neighbor_node_ids;

            m.lock();

            for (const auto& conn_ptr : connection_list) {
                //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                if (conn_ptr->neighbor_nodeid != "" && conn_ptr->socket_fd >= 0) {  
                    neighbor_node_ids.push_back(conn_ptr->neighbor_nodeid);
                }
            }
            m.unlock();

            if (neighbor_node_ids.size() == 0){
                cout << ":" << port << " has an empty forwarding table\n";
            } else {
                m.lock();
                
                struct timeval blank = {0, 0};

                shared_ptr<Node> start_node = make_shared<Node>(port, blank, blank, "", neighbor_node_ids);
                unordered_set<string> visited;
                queue<shared_ptr<Node>> BFS_q;
                BFS_q.push(start_node);
                visited.insert(port);

                while(!BFS_q.empty()){
                    shared_ptr<Node> current = BFS_q.front();
                    BFS_q.pop();
                    for (const string& neighborID: current->neighbors){
                        //cout << neighborID << endl;
                        if (visited.find(neighborID) == visited.end()) {
                            visited.insert(neighborID);
                            adjacency_list[neighborID]->pred = current; // Set pred
                            BFS_q.push(adjacency_list[neighborID]);
                        }
                    }
                    
                }
                
                for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                    if (visited.find(it->first) == visited.end()) {
                        it = adjacency_list.erase(it);  
                    } else {
                        ++it;
                    }
                }

                for (const auto& entry : adjacency_list) {
                    const auto& node_id = entry.first;
                    const auto& node_ptr = entry.second;
                    if (node_id == port) continue; // skip SELF

                    shared_ptr<Node> current = node_ptr;
                    
                    while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != port) {
                        
                        current = adjacency_list[current->nodeid]->pred;
                    }

                    if (adjacency_list[current->nodeid]->pred->nodeid == port) { // Should always be the case
                        cout << ":" << node_id << ": :" << current->nodeid << "\n";
                    }
                }

                m.unlock();
            }

        } else if (command.find("traceroute") != string::npos){
            istringstream iss(command);
            string cmd, targetWithColon;
            iss >> cmd >> targetWithColon;

            string target = targetWithColon.substr(1);
            string time;
            bool isSuccess = false;
            
            GetObjID(port, "ses", tracerouteSesID, time);

            //cout << tracerouteSesID << endl;

            if (target == port){
                cout << "Cannot traceroute to yourself\n";
                continue;
            }

            for (int ttl = 1; ttl <= max_ttl; ttl++){
                /* RUN BFS */
                vector<string> neighbor_node_ids;
                m.lock();

                for (const auto& conn : connection_list) {
                                                //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                    if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0) {  
                        neighbor_node_ids.push_back(conn->neighbor_nodeid);
                    }
                }
                m.unlock();
                                            
                if (neighbor_node_ids.size() == 0){
                    cout << "Error clearing adjacency list\n";
                } else {
                    m.lock();
                                                
                    struct timeval blank = {0, 0};

                    shared_ptr<Node> start_node = make_shared<Node>(port, blank, blank, "", neighbor_node_ids);
                    unordered_set<string> visited;
                    queue<shared_ptr<Node>> BFS_q;
                    BFS_q.push(start_node);
                    visited.insert(port);

                    while(!BFS_q.empty()){
                        shared_ptr<Node> current = BFS_q.front();
                        BFS_q.pop();
                        for (const string& neighborID: current->neighbors){
                            //cout << neighborID << endl;
                            if (visited.find(neighborID) == visited.end()) {
                                visited.insert(neighborID);
                                adjacency_list[neighborID]->pred = current; // Set pred
                                BFS_q.push(adjacency_list[neighborID]);
                            }
                        }
                                                    
                    }
                                                
                    for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                        if (visited.find(it->first) == visited.end()) {
                            it = adjacency_list.erase(it);  
                        } else {
                             ++it;
                        }
                    }


                    struct timeval starttime;
                    gettimeofday(&starttime, NULL);

                    shared_ptr<TimerCallback> cb = make_shared<TracerouteCallback>();
                    shared_ptr<Timer> timer = make_shared<Timer>(msg_lifetime, cb, tracerouteSesID);

                    timer->start();

                    bool isIn = false;
                    string destination;
                                
                    for (const auto& entry : adjacency_list) {
                        const auto& node_id = entry.first;
                        const auto& node_ptr = entry.second;
                        if (node_id == port) continue; // skip SELF

                        shared_ptr<Node> current = node_ptr;

                                    
                        while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != port) {
                                        
                            current = adjacency_list[current->nodeid]->pred;
                        }
                                    //cout << target << " " << node_id << " " << adjacency_list[current->nodeid]->pred->nodeid << endl;
                        if (node_id == target && adjacency_list[current->nodeid]->pred->nodeid == port) { // Should always be the case
                            //cout << "Should route here: "<< current->nodeid << "\n";
                            destination = current->nodeid;
                            //cout << "I set destination to " << current->nodeid << endl;
                            isIn = true;
                        }
                    }

                    m.unlock();

                                
                    if (isIn == false){
                        //cout << target << "is not reachable\n";
                    } else{
                        shared_ptr<Message> msg = make_shared<Message>("UCASTAPP", "353NET/1.0", port);
                        

                        m.lock();
                        
                        string msg_id, origin_time;
                        GetObjID(msg->nodeid, "msg", msg_id, origin_time);
                        m.unlock();
                        

                        msg->msg_id = msg_id;
                        msg->origin_start_time = origin_time;
                        msg->number = ttl;
                        
                        msg->target = target;
                        msg->nextLayer = "1";
                        string body = "353UDT/1.0 PING "+tracerouteSesID;
                        msg->body = body;
                        msg->destination = destination;
                        int content_length = body.size();
                        msg->sesid = tracerouteSesID;

                        

                        m.lock();
                        for (auto& conn : connection_list){
                            
                            if (conn->neighbor_nodeid == destination && conn->socket_fd >= 0){
                                
                                 conn->add_work(msg);

                                 string send_utp = "["+get_timestamp_now()+"] " + "i UCASTAPP :" + conn->neighbor_nodeid +
                                 " " + to_string(ttl) + " - " + to_string(content_length) + " :" + 
                                 msg->msg_id + " " + " :" + msg->nodeid + " :" +  target + " 1 " + msg->body + "\n";
                                 //cout << send_utp;
                                 LogALine(send_utp, "U");
                            }
                        }
                        m.unlock();

                        //cout << "I'm here now" << endl;
                    }
                        Event ev = traceroute_wait_for_work();
                        //cout << "type: " << ev.type << endl;


                        if (ev.type == EVENT_TTLZERO && ev.sesid == tracerouteSesID){
                            timer->stop();
                            struct timeval now;
                            gettimeofday(&now, NULL);
                            double rtt = timestamp_diff_in_seconds(&starttime, &now);
                            cout << ttl << " - :" << ev.from_node << ", " << rtt << endl;

                        } else if (ev.type == EVENT_PONG && ev.sesid == tracerouteSesID){
                            timer->stop();
                            struct timeval now;
                            gettimeofday(&now, NULL);
                            double rtt = timestamp_diff_in_seconds(&starttime, &now);
                            cout << ttl << " - :" << target << ", " << rtt << endl;
                            cout << ":" << target << " is reached in " << ttl << " steps\n";
                            timer->thread_ptr->join();
                            isSuccess = true;
                            break;

                        } else if (ev.type == EVENT_TIMEOUT){
                            cout << ttl << " - *\n";
                        }


                        if (!isSuccess){
                            timer->thread_ptr->join();
                            sleep(3);
                        }
                        

                    

                    

                    


                }


            }
            //cout << "Am I coming here?" << endl;

            if (!isSuccess){
                cout << "traceroute: :" << target << " not reached after " << max_ttl << " steps\n";
            }


        } else if (command.find("rdtsend") != string::npos){
            istringstream iss(command);
            string cmd, target;
            iss >> cmd >> target;

            // Get the rest of the line as the message
            string message;
            getline(iss, message);

            
            message.erase(message.begin(), find_if(message.begin(), message.end(), [](unsigned char ch) {
                return !isspace(ch);
            }));

            string targetWithoutColon = target.substr(1);
            if (targetWithoutColon == port){
                cout << "Cannot use rdtsend command to send message to yourself.\n";
                continue;
            }

            vector<string> neighbor_node_ids;

            m.lock();

            for (const auto& conn_ptr : connection_list) {
                //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                if (conn_ptr->neighbor_nodeid != "" && conn_ptr->socket_fd >= 0) {  
                    neighbor_node_ids.push_back(conn_ptr->neighbor_nodeid);
                }
            }
            m.unlock();

            if (neighbor_node_ids.size() == 0){
                cout << target << " is not reachable\n";
            } else {
                m.lock();
                
                struct timeval blank = {0, 0};

                shared_ptr<Node> start_node = make_shared<Node>(port, blank, blank, "", neighbor_node_ids);
                unordered_set<string> visited;
                queue<shared_ptr<Node>> BFS_q;
                BFS_q.push(start_node);
                visited.insert(port);

                while(!BFS_q.empty()){
                    shared_ptr<Node> current = BFS_q.front();
                    BFS_q.pop();
                    for (const string& neighborID: current->neighbors){
                        //cout << neighborID << endl;
                        if (visited.find(neighborID) == visited.end()) {
                            visited.insert(neighborID);
                            adjacency_list[neighborID]->pred = current; // Set pred
                            BFS_q.push(adjacency_list[neighborID]);
                        }
                    }
                    
                }
                
                for (auto it = adjacency_list.begin(); it != adjacency_list.end(); ) {
                    if (visited.find(it->first) == visited.end()) {
                        it = adjacency_list.erase(it);  
                    } else {
                        ++it;
                    }
                }

                bool isIn = false;
                string destination = "";
                for (const auto& entry : adjacency_list) {
                    const auto& node_id = entry.first;
                    const auto& node_ptr = entry.second;
                    if (node_id == port) continue; // skip SELF

                    shared_ptr<Node> current = node_ptr;
                    
                    while (adjacency_list[current->nodeid]->pred != NULL && adjacency_list[current->nodeid]->pred->nodeid != port) {
                        
                        current = adjacency_list[current->nodeid]->pred;
                    }

                    if (node_id == targetWithoutColon && adjacency_list[current->nodeid]->pred->nodeid == port) { // Should always be the case
                        //cout << "Should route here: "<< current->nodeid << "\n";
                        destination = current->nodeid;
                        isIn = true;
                    }
                }

                m.unlock();
                

                if (isIn == false){
                    cout << target << " is not reachable\n";
                } else {
                    if (message.back() != '\n') {
                        message += '\n';
                    }
                    rdt_send(port, destination, message, targetWithoutColon);
                }
            }

        } else {
            cout << "Command not recognized. Valid commands are:\n\techoapp target\n\tforwarding\n\tneighbors\n\tnetgraph\n\trdtsend target message\n\ttraceroute target\n\tquit\n";
        }

    }

    m.unlock();
    m.lock();
          
    shutdown(listen_socket_fd, SHUT_RDWR);
    close(listen_socket_fd);
    listen_socket_fd = -1;
                
    for (const auto& conn_ptr : connection_list) {
        if (conn_ptr->socket_fd >= 0) {
            //cout << conn_ptr->conn_number << endl;
            m.unlock();
            conn_ptr->m->unlock();
            conn_ptr->m->lock();
            shutdown(conn_ptr->socket_fd, SHUT_RDWR);
            conn_ptr->socket_fd = -2;
            conn_ptr->m->unlock();
            m.lock();
        }
    }
                
            

    m.unlock();
}

void reaper_thread(){
    while (true){
        shared_ptr<Connection> c = reaper_wait_for_work();
        if (c == NULL){
            break;
        } else {
            if (c->read_thread_ptr) {
                c->read_thread_ptr->join();  // Wait for the read thread to finish
            }
            

            m.lock();

            string line_join = "[" + get_timestamp_now() + "] [" + to_string(c->conn_number) + "]\tReaper has joined with connection thread\n";
            //LogALine(line_join);
            close(c->orig_socket_fd);

            connection_list.erase(std::remove(connection_list.begin(), connection_list.end(), c), connection_list.end());
            m.unlock();
            //cout << "here" << endl;
        }

        /*
        usleep(250000);
        m.lock();
      
        if (listen_socket_fd == -1 && connection_list.empty()){
            m.unlock();
            break;
        } else {
            for (vector<shared_ptr<Connection>>::iterator itr = connection_list.begin(); itr != connection_list.end(); ) {
                shared_ptr<Connection> connection_ptr = (*itr);
                if (connection_ptr->socket_fd == -1) {
                    connection_ptr->thread_ptr.get()->join();
                    string line_join = "[" + get_timestamp_now() + "] [" + to_string(connection_ptr->conn_number) + "]\tReaper has joined with connection thread\n";
                    LogALine(line_join);
                    itr = connection_list.erase(itr);
                } else {
                    itr++;
                }
            }
        }
        m.unlock();  
        */      
    }

    while (true){
        m.lock();
        if (connection_list.empty()){
            m.unlock();
            break;
        }
        shared_ptr<Connection> c2 = connection_list.front();
        m.unlock();
        if (c2->read_thread_ptr) {
            c2->read_thread_ptr->join();  // Wait for the read thread to finish
        }
        
        m.lock();

        string line_join = "[" + get_timestamp_now() + "] [" + to_string(c2->conn_number) + "]\tReaper has joined with connection thread\n";
        //LogALine(line_join);
        close(c2->orig_socket_fd);

        connection_list.erase(connection_list.begin());  

        m.unlock();
       



    }

}

int main(int argc, char *argv[])
{
        
        string configfile = argv[1];

        // Read all lines of file into a single vector
        int fd = open(configfile.c_str(), O_RDONLY);
    
        if (fd < 0){ // If open failed, fd is negative:
            perror("open");
            exit(1);
        }

        vector<string> file_lines;

        for (;;) {
            string line;
            if (read_a_line(fd, line) <= 0) {
                break;
            }
            file_lines.push_back(line);
        }

        vector<pair<string, int>> sectionsAndLocation = getSections(file_lines);

        int startupLoc = -1;

        for (const auto& section : sectionsAndLocation) {
            if (section.first == "startup"){
                startupLoc = section.second;
            }
        }

        string port;
        string logfile = "";
        string host;
        bool logToFile = false;

        int i = startupLoc + 1;
        while (i < file_lines.size() && file_lines[i][0] != '['){
            if (file_lines[i][0] != ';' && file_lines[i] != "\n"){
                size_t equals_pos = file_lines[i].find("=");
                string key = file_lines[i].substr(0, equals_pos);
                if (key == "port"){
                    port = file_lines[i].substr(equals_pos+1, file_lines[i].length() - equals_pos - 2);   
                } else if (key == "logfile"){
                    logToFile = true;
                    logfile = file_lines[i].substr(equals_pos+1, file_lines[i].length() - equals_pos - 2);
                } else if (key == "host") {
                    host = file_lines[i].substr(equals_pos+1, file_lines[i].length() - equals_pos - 2);
                    if (host == ""){
                        host = "localhost";
                    } 
                } 
            }
            i = i + 1;
        }

        int loggingLoc = -1;
        string logging = "0"; 
        string loggingLS = "0";
        string loggingU = "0";
        bool logLSUPDATE;
        bool logU;
        for (const auto& section : sectionsAndLocation) {
            if (section.first == "logging"){
                loggingLoc = section.second;
            }
        }

        int j = loggingLoc + 1; 
        while (j < file_lines.size() && file_lines[j][0] != '['){
            if (file_lines[j][0] != ';' && file_lines[j] != "\n"){
                size_t equals_pos = file_lines[j].find("=");
                string key = file_lines[j].substr(0, equals_pos);
                if (key == "SAYHELLO"){
                    logging = file_lines[j].substr(equals_pos+1, file_lines[j].length() - equals_pos - 2);   
                } else if (key == "LSUPDATE"){
                    loggingLS = file_lines[j].substr(equals_pos+1, file_lines[j].length() - equals_pos - 2);
                } else if (key == "UCASTAPP"){
                    loggingU = file_lines[j].substr(equals_pos+1, file_lines[j].length() - equals_pos - 2);
                }
            }
            j += 1;
        }

        if (logging == "0"){
            logToFile = false;
        } else {
            logToFile = true;
        }

        if (loggingLS == "0"){
            logLSUPDATE = false;
            //cout << "Came here" << endl;
        } else {
            logLSUPDATE = true;
        }

        if (loggingU == "0"){
            logU = false;
            
        } else {
            logU = true;
        }
        
        Init(logToFile, logfile, "H"); // Sets up mylog
        Init(logLSUPDATE, logfile, "L");
        Init(logU, logfile, "U");
        //cout << "Initializing logfie with logfile = " << logU << endl;
        
        int paramsLoc = -1;
        string max_ttl_string = "";
        string msg_lifetime_string = "";
        for (const auto& section : sectionsAndLocation) {
            if (section.first == "params"){
                paramsLoc = section.second;
            }
        }

        int k = paramsLoc + 1; 
        while (k < file_lines.size() && file_lines[k][0] != '['){
            if (file_lines[k][0] != ';' && file_lines[k] != "\n"){
                size_t equals_pos = file_lines[k].find("=");
                string key = file_lines[k].substr(0, equals_pos);
                if (key == "max_ttl"){
                    max_ttl_string = file_lines[k].substr(equals_pos+1, file_lines[k].length() - equals_pos - 2);   
                } else if (key == "msg_lifetime"){
                    msg_lifetime_string = file_lines[k].substr(equals_pos+1, file_lines[k].length() - equals_pos - 2);   
                
                }
            }
            k += 1;
        }
        //cout << "Hi: " << max_ttl_string;
        max_ttl = stoi(max_ttl_string);
        msg_lifetime = stoi(msg_lifetime_string);



        listen_socket_fd = create_listening_socket(port);

        string server_ip_and_port;
        if (listen_socket_fd != (-1)) {
            server_ip_and_port = get_ip_and_port_for_server(listen_socket_fd, 1);
            string line_start = "["+get_timestamp_now()+"] " + "Server " + server_ip_and_port + " started\n";
            //LogALine(line_start);


            if (gnDebug) {
                string s = get_ip_and_port_for_server(listen_socket_fd, 1);
                cout << "[SERVER]\tlistening at " << s << endl;
            }



            thread console(console_thread, port);
            
            thread reaper(reaper_thread);

            thread neighbors(neighbors_thread, ref(file_lines), port, host);
            //cout << "Neighbor thread started." << endl;
            for (;;) {
                
                int newsockfd = my_accept(listen_socket_fd);
                
                if (newsockfd == (-1)) break;


                m.lock();
                if (listen_socket_fd == -1){
                    shutdown(newsockfd, SHUT_RDWR);
                    close(newsockfd);
                    m.unlock();
                    break;
                }
                
                // Create a shared pointer to the Connection object
                shared_ptr<Connection> conn_ptr = make_shared<Connection>(Connection(next_conn_number++, newsockfd, NULL, NULL));

                // Create two threads: one for reading and one for writing
                shared_ptr<thread> read_thr_ptr = make_shared<thread>(read_from_client, conn_ptr);  // Thread to handle reading from the client
                shared_ptr<thread> write_thr_ptr = make_shared<thread>(write_from_client, conn_ptr);  // Thread to handle writing to the client

                // Assign the threads to the connection object
                conn_ptr->read_thread_ptr = read_thr_ptr;
                conn_ptr->write_thread_ptr = write_thr_ptr;

                conn_ptr->neighbor_nodeid = ""; /* don't know the NodeID of this neighbor yet */
                conn_ptr->nodeid = port;

                // Push the connection object into the connection list
                connection_list.push_back(conn_ptr);
                //cout << "push(a) " << conn_ptr->nodeid << " with neighbor " << conn_ptr->neighbor_nodeid << endl;
                
                m.unlock();
            }

            console.join();
            //cout << "Is this dead 1" << endl;
            reaper_add_work(NULL);
            reaper.join();
            //cout << "Is this dead 3" << endl;
            neighbors.join();
            //cout << "Is this dead 4" << endl;
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->read_thread_ptr) {
                    conn_ptr->read_thread_ptr->join();  // Wait for the read thread to finish
                }
                if (conn_ptr->write_thread_ptr) {
                    conn_ptr->write_thread_ptr->join();  // Wait for the write thread to finish
                }
            }

            
            string line_die = "["+get_timestamp_now()+"] " + "Server " + server_ip_and_port + " stopped\n";
            //LogALine(line_die);
            
        }
        return 0;
}