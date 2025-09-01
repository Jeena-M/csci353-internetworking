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

static int listen_socket_fd = (-1); /* there is nothing wrong with using a global variable */
static int gnDebug = 0; /* change it to 0 if you don't want debugging messages */

static vector<shared_ptr<Connection>> connection_list;
static mutex m;
static int next_conn_number = 1; // Global variable; everytime you assign the value here, must increment
int max_ttl;
int N = 1; // speed
queue<shared_ptr<Connection>> q;
condition_variable cv;
int total = 0;
static map<string, string> msg_cache;

static map<string, shared_ptr<Node>> adjacency_list;

static ofstream file;
ostream *mylog=NULL;


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
void Init(bool logToFile, string logfile)
{
    if (logToFile) {
        file.open(logfile, ofstream::out|ofstream::app);
        mylog = &file;
    } else {
        mylog = &cout;
    }
}

/*
 * This function demonstrate that you can use the ostream to write to cout or a file.
 */
static
void LogALine(string a_line_of_msg)
{
    *mylog << a_line_of_msg;
    mylog->flush();
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


    m.lock();
    for (auto& conn : connection_list){
        
        if (conn->neighbor_nodeid != "" && conn->socket_fd >= 0){
            
             conn->add_work(msg);
        }
    }
    m.unlock();

    int content_length = body.size();

    string initiate_flood = "["+get_timestamp_now()+"] " + "i LSUPDATE " + conn_ptr->neighbor_nodeid +
     " " + to_string(max_ttl) + " F " + to_string(content_length) + " " + 
    msg->msg_id + " " + msg->origin_start_time + " " + msg->nodeid + " " + "(" + body + ")\n";
    cout << initiate_flood;

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


        string msg_received =  "["+get_timestamp_now()+"] " + "r SAYHELLO " + NODEID + " 1 - 0\n";
        
        LogALine(msg_received);
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
                
                header_lines.clear();
                for (;;) {

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

                
                
                /* process message, should never get here for this lab */
                
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

                
                string receive_lsupdate = "["+get_timestamp_now()+"] " + "r LSUPDATE " + conn_ptr->neighbor_nodeid + " " + to_string(ttl) + " F " + to_string(content_length) + " " + 
                msg_id + " " + origin_start_time + " " + from + " " + "(" + body + ")\n";
                cout << receive_lsupdate;
                        

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
                                string send_flood = "["+get_timestamp_now()+"] " + "d LSUPDATE " + d->neighbor_nodeid +
                                            " " + to_string(ttl) + " F " + to_string(content_length) + " " + 
                                            msg_id + " " + origin_start_time + " " + from + " " + "(" + body + ")\n";
                                cout << send_flood;
       
                                
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
            //cout << "Came here 5 " << endl;
                

            }
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

            string msg_sent =  "["+get_timestamp_now()+"] i SAYHELLO " + conn_ptr->neighbor_nodeid + " 1 - 0\n";
            LogALine(msg_sent);
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

        }
        
    }
}

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
                cout << "Active connections of :" << port << ":" << endl;
                for (const auto& conn_ptr : connection_list) {
                    //cout << ":" << conn_ptr->nodeid <<"," << " " << conn_ptr->neighbor_nodeid << "; " ;
                    if (conn_ptr->neighbor_nodeid != "" && conn_ptr->socket_fd >= 0) {  
                        cout << "\t:" << conn_ptr->neighbor_nodeid << "," ;
                    }
                }
                cout << "\n";
            }
            m.unlock();

        } else if (command == "quit") {
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

                cout << port << ": ";
                for (const string& neighborID: neighbor_node_ids){
                    cout << neighborID << ",";
                }
                cout << "\n";

                for (const auto& pair : adjacency_list) {
                    //cout << pair.first << "is in adjacency_list" << endl;
                    if (pair.first != port){
                        cout << pair.first << ": " << pair.second->link_state << endl;
                    }
                    
                }
                m.unlock();
            }
            

        } else if (command == "help"){
            cout << "Available commands are:\n\tneighbors\n\thelp\n\tquit\n";
        } else if (command == ""){
            continue;
        } else {
            cout << "Available commands are:\n\tneighbors\n\thelp\n\tquit\n";
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
                }
            }
            j += 1;
        }

        if (logging == "0"){
            logToFile = false;
        } else {
            logToFile = true;
        }
        
        Init(logToFile, logfile); // Sets up mylog
        
        int paramsLoc = -1;
        string max_ttl_string = "";
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
                }
            }
            k += 1;
        }
        //cout << "Hi: " << max_ttl_string;
        max_ttl = stoi(max_ttl_string);



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