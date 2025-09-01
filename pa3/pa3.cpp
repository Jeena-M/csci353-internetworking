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

using namespace std;

/* C system include files next */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/md5.h>

/* C standard include files next */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <cstring>

/* your own include last */
#include "my_socket.h"
#include "my_timestamp.h"
#include "connection.h"

static int listen_socket_fd = (-1); /* there is nothing wrong with using a global variable */
static int gnDebug = 0; /* change it to 0 if you don't want debugging messages */
static vector<pair<int,shared_ptr<thread>>> connection_handling_threads;
static vector<shared_ptr<Connection>> connection_list;
static mutex m;
static int next_conn_number = 1; // Global variable; everytime you assign the value here, must increment
int N = 1; // speed
static string rootdir;

static ofstream file;
ostream *mylog=NULL;
static map<string, vector<int>> config; // (sectionName, [p, maxr, dial])
static map<int, int> tempDialMap;
static map<int, bool> isDialSetMap;
static shared_ptr<Connection> tempTargetConnection;
static int tempConnNumber;

bool containsChar(const string& str, char target) {
    return find(str.begin(), str.end(), target) != str.end();
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

static
string HexDump(unsigned char *buf, int len)
{
    string s;
    static char hexchar[]="0123456789abcdef";

    for (int i=0; i < len; i++) {
        unsigned char ch=buf[i];
        unsigned int hi_nibble=(unsigned int)((ch>>4)&0x0f);
        unsigned int lo_nibble=(unsigned int)(ch&0x0f);

        s += hexchar[hi_nibble];
        s += hexchar[lo_nibble];
    }
    return s;
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

/**
 * Open in_filename_string for reading.
 *
 * @param in_filename_string - file name to open for reading.
 * @return (-1) if file cannot be opened; otherwise, return a file descriptor.
 */
int open_file_for_reading(string in_filename_string)
{
    return open(in_filename_string.c_str(), O_RDONLY);
}

int open_file_for_writing_trunc(string out_filename_string)
{
    int fd = open_file_for_reading(out_filename_string);
    if (fd == (-1)) {
        fd = open(out_filename_string.c_str(), O_WRONLY|O_CREAT, 0600);
    } else {
        close(fd);
        fd = open(out_filename_string.c_str(), O_WRONLY|O_TRUNC); // Overwrites if file already exists
    }   
    return fd;
}


/**
 * Open out_filename_string for writing (create the file if it doesn't already exist).
 *
 * @param out_filename_string - file name to open for writing.
 * @return (-1) if file cannot be opened; otherwise, return a file descriptor.
 */
int open_file_for_writing(string out_filename_string)
{
    int fd = open_file_for_reading(out_filename_string);
    if (fd == (-1)) {
        fd = open(out_filename_string.c_str(), O_WRONLY|O_CREAT, 0600);
    } else {
        close(fd);
        fd = open(out_filename_string.c_str(), O_WRONLY|O_APPEND); // Currently, appends to existing contents if file already exists
    }   
    return fd;
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
/**
 * This is the function you need to change to change the behavior of your server!
 * Returns non-zero if succeeds.
 * Otherwise, return 0;
 *
 * @param argc - number of arguments in argv.
 * @param argv - array of argc C-strings, must only use array index >= 0 and < argc.
 */
static
void process_options(int argc, char *argv[])
{
    if (gnDebug) {
        for (int i=0; i < argc; i++) {
            cerr << "[DBG-SVR]\targv[" << i << "]: '" << argv[i] << "'" << endl;
        }
    }
    /* incomplete, just demonstrate how to use argc */
    if (argc != 3) {
        usage_server();
    }
}

/**
 * This is the function you need to change to change the behavior of your server!
 *
 * @param newsockfd - socket that can be used to "talk" (i.e., read/write) to the client.
 */
static
void talk_to_client(shared_ptr<Connection> conn_ptr)
{ 
    /* Connection is incomplete */
    m.lock();
    m.unlock();
    /* Connection is now complete */


    int bytes_received;
    string line;
    vector<string> header_lines;
    string client_ip_and_port = get_ip_and_port_for_server(conn_ptr->socket_fd, 0);
    bool not_closed = true;

    while(not_closed){
        header_lines.clear();
        for (;;) {
            bytes_received = read_a_line(conn_ptr->socket_fd, line);

            // If read_a_line returns a value <= 0: socket is dead
            if (bytes_received <= 0){
                m.lock();
                if (conn_ptr->socket_fd >= 0){
                    shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                }
                conn_ptr->socket_fd = -1;
                close(conn_ptr->orig_socket_fd);
                
                string line_closed = "["+get_timestamp_now()+"] " + "CLOSE[" + to_string(conn_ptr->conn_number) + "]: (done) " + client_ip_and_port + "\n";
                //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                LogALine(line_closed);
                if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]){
                    isDialSetMap[conn_ptr->conn_number] = false; // After current download, set isDialSet to false
                    tempDialMap.erase(conn_ptr->conn_number);
                    isDialSetMap.erase(conn_ptr->conn_number);
                }

                m.unlock();
                return;
            }

            if (gnDebug) {
                cerr << "[DBG-SVR] " << dec << bytes_received << " bytes received from " << get_ip_and_port_for_server(conn_ptr->socket_fd, 0) << " (data displayed in next line, <TAB>-indented):\n\t";
                cerr << line;
                if (!(bytes_received > 0 && line.back() == '\n')) cerr << endl;
                if (bytes_received > 0) {
                    cerr << "\t";
                    for (char c: line) {
                        cerr << "0x" << setfill('0') << setw(2) << hex << (int)c << " ";
                    }
                    cerr << endl;
                }
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

            // Step 1: Process request header: 
            string request_line = header_lines[0];
            string method, uri, version;
            size_t space1 = request_line.find(' ');
            size_t space2 = request_line.find(' ', space1 + 1);

            // Parse the method, uri, version
            method = request_line.substr(0, space1);
            uri = request_line.substr(space1 + 1, space2 - space1 - 1);
            version = request_line.substr(space2 + 1);


            // After obtaining the URI:
            
            

            string line_request = "["+get_timestamp_now()+"]" + " REQUEST[" + to_string(conn_ptr->conn_number) + "]: " + get_ip_and_port_for_server(conn_ptr->socket_fd, 0) + ", uri=" + uri + "\n";
            LogALine(line_request);

            conn_ptr->last_uri = uri;

            


            // Log the HTTP request header 
            for (string& header_line : header_lines){
                string line = "\t[" + to_string(conn_ptr->conn_number) + "]\t" + header_line;
                LogALine(line);
            }
            

            /* EXTRA 404 ERROR HANDLING */

             // Error Case 1: Malformed request line
            int spaceCount = count(header_lines[0].begin(), header_lines[0].end(), ' ');
            if (spaceCount != 2){
                string response = "HTTP/1.1 404 Not Found\r\nServer: pa3 (jeenamah@usc.edu)\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";

                better_write_header(conn_ptr->socket_fd, response.c_str(), static_cast<int>(response.length()));
                string line_response = "["+get_timestamp_now()+"] RESPONSE[" + to_string(conn_ptr->conn_number) + "]: " + get_ip_and_port_for_server(conn_ptr->socket_fd, 0) + ", status=" + "404\n";
                LogALine(line_response);
                string connection_number_string = "[" + to_string(conn_ptr->conn_number) + "]";
                LogALine("\t" + connection_number_string + "HTTP/1.1 404 Not Found\r\n\t" + connection_number_string + 
                    "\tServer: pa3 (jeenamah@usc.edu)\r\n\t" + connection_number_string + "\tContent-Type: text/html\r\n\t" + connection_number_string +"\tContent-Length: 63\r\n\t" + connection_number_string + "\tContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\t"
                    + connection_number_string + "\t\r\n");
                continue;

            }

            

            // Error Case 2: Method is not GET
            if (method != "GET") {
                string response = "HTTP/1.1 404 Not Found\r\nServer: pa3 (jeenamah@usc.edu)\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";

                better_write_header(conn_ptr->socket_fd, response.c_str(), static_cast<int>(response.length()));
                string line_response = "["+get_timestamp_now()+"] RESPONSE[" + to_string(conn_ptr->conn_number) + "]: " + get_ip_and_port_for_server(conn_ptr->socket_fd, 0) + ", status=" + "404\n";
                LogALine(line_response);
                string connection_number_string = "[" + to_string(conn_ptr->conn_number) + "]";
                LogALine("\t" + connection_number_string + "HTTP/1.1 404 Not Found\r\n\t" + connection_number_string + 
                    "\tServer: pa3 (jeenamah@usc.edu)\r\n\t" + connection_number_string + "\tContent-Type: text/html\r\n\t" + connection_number_string +"\tContent-Length: 63\r\n\t" + connection_number_string + "\tContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\t"
                    + connection_number_string + "\t\r\n");
                continue;            }

            // Error Case 3: URI's characters
            if (uri[0] != '/' || uri[uri.length() -1] == '/' || containsChar(uri, '?') || containsChar(uri, '#')){
                string response = "HTTP/1.1 404 Not Found\r\nServer: pa3 (jeenamah@usc.edu)\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";

                better_write_header(conn_ptr->socket_fd, response.c_str(), static_cast<int>(response.length()));
                string line_response = "["+get_timestamp_now()+"] RESPONSE[" + to_string(conn_ptr->conn_number) + "]: " + get_ip_and_port_for_server(conn_ptr->socket_fd, 0) + ", status=" + "404\n";
                LogALine(line_response);
                string connection_number_string = "[" + to_string(conn_ptr->conn_number) + "]";
                LogALine("\t" + connection_number_string + "HTTP/1.1 404 Not Found\r\n\t" + connection_number_string + 
                    "\tServer: pa3 (jeenamah@usc.edu)\r\n\t" + connection_number_string + "\tContent-Type: text/html\r\n\t" + connection_number_string +"\tContent-Length: 63\r\n\t" + connection_number_string + "\tContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\t"
                    + connection_number_string + "\t\r\n");
                continue;
            }


            // Error Case 4: Path specified in URI
            struct stat stat_buf;
            string check_path = rootdir + "/" + uri;
            if (stat(check_path.c_str(), &stat_buf) != 0) {
                string response = "HTTP/1.1 404 Not Found\r\nServer: pa3 (jeenamah@usc.edu)\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";

                better_write_header(conn_ptr->socket_fd, response.c_str(), static_cast<int>(response.length()));
                string line_response = "["+get_timestamp_now()+"] RESPONSE[" + to_string(conn_ptr->conn_number) + "]: " + get_ip_and_port_for_server(conn_ptr->socket_fd, 0) + ", status=" + "404\n";
                LogALine(line_response);
                string connection_number_string = "[" + to_string(conn_ptr->conn_number) + "]";
                LogALine("\t" + connection_number_string + "HTTP/1.1 404 Not Found\r\n\t" + connection_number_string + 
                    "\tServer: pa3 (jeenamah@usc.edu)\r\n\t" + connection_number_string + "\tContent-Type: text/html\r\n\t" + connection_number_string +"\tContent-Length: 63\r\n\t" + connection_number_string + "\tContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\t"
                    + connection_number_string + "\t\r\n");
                continue;
            }






            // Step 2: Create response

            string path = rootdir + uri; // Create a file system path
            
            

            // Call get_file_size()
            int size = get_file_size(path);

            conn_ptr->last_content_length = (size);
            

            if (size == -1){
                //string response = "HTTP/1.1 404 Not Found\r\nServer: pa3\r\nContent-Type: text/html\r\nContent-Length: 63\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
                string response = "HTTP/1.1 404 Not Found\r\nServer: pa3 (jeenamah@usc.edu)\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";

                better_write_header(conn_ptr->socket_fd, response.c_str(), static_cast<int>(response.length()));
                string line_response = "["+get_timestamp_now()+"] RESPONSE[" + to_string(conn_ptr->conn_number) + "]: " + get_ip_and_port_for_server(conn_ptr->socket_fd, 0) + ", status=" + "404\n";
                LogALine(line_response);
                string connection_number_string = "[" + to_string(conn_ptr->conn_number) + "]";
                LogALine("\t" + connection_number_string + "HTTP/1.1 404 Not Found\r\n\t" + connection_number_string + 
                    "\tServer: pa3 (jeenamah@usc.edu)\r\n\t" + connection_number_string + "\tContent-Type: text/html\r\n\t" + connection_number_string +"\tContent-Length: 63\r\n\t" + connection_number_string + "\tContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\t"
                    + connection_number_string + "\t\r\n");
                continue;
                //cout << "\tHTTP/1.1 404 Not Found\r\n\tServer: lab4a\r\n\tContent-Type: text/html\r\n\tContent-Length: 63\r\n\t\r\n\t<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
            } else {

                // Determine true speed value
                size_t lastSlash = uri.find_last_of('/');
                string fileName = (lastSlash != string::npos) ? uri.substr(lastSlash + 1) : uri;
                string extension;
                size_t lastDot = fileName.find_last_of('.');
                if (lastDot != string::npos && lastDot != fileName.length() - 1) {
                    extension = fileName.substr(lastDot + 1);  // Return the extension
                } else {
                    extension = "*";
                }



                string response_header;
                string content_type;
                if (extension == "html") {
                    //response_header = "HTTP/1.1 200 OK\r\nServer: lab4a\r\nContent-Type: text/html\r\nContent-Length: " + to_string(size) + "\r\n\r\n";
                    content_type = "text/html";
                    //cout << "\tHTTP/1.1 200 OK\r\n\tServer: lab4a\r\n\tContent-Type: text/html\r\n\tContent-Length: " + to_string(size) + "\r\n\t\r\n";
            
                } else {
                    //response_header = "HTTP/1.1 200 OK\r\nServer: lab4a\r\nContent-Type: application/octet-stream\r\nContent-Length: " + to_string(size) + "\r\n\r\n";
                    content_type = "application/octet-stream";
                    //cout << "\tHTTP/1.1 200 OK\r\n\tServer: lab4a\r\n\tContent-Type: application/octet-stream\r\n\tContent-Length: " + to_string(size) + "\r\n\t\r\n";
                }



                // Generate MD5HEXVALUE:



                /* Start code for MD5 */
                ifstream myfile;

                myfile.open(path, ifstream::in|ios::binary);
                if (myfile.fail()) {
                    cerr << "Cannot open '" << path << "' for reading." << endl;
                    exit(-1);
                }
                int bytes_remaining = get_file_size(path);
                MD5_CTX md5_ctx;

                MD5_Init(&md5_ctx);
                while (bytes_remaining > 0) {
                    char buf[0x1000]; /* 4KB buffer */

                    int bytes_to_read = ((bytes_remaining > (int)sizeof(buf)) ? sizeof(buf) : bytes_remaining);
                    myfile.read(buf, bytes_to_read);
                    if (myfile.fail()) {
                        break;
                    }
                    MD5_Update(&md5_ctx, buf, bytes_to_read);
                    bytes_remaining -= bytes_to_read;
                }
                myfile.close();
                unsigned char md5_buf[MD5_DIGEST_LENGTH];

                MD5_Final(md5_buf, &md5_ctx);

                string md5 = HexDump(md5_buf, sizeof(md5_buf));
                /* End code for MD5 */

                //LogALine("\tHTTP/1.1 200 OK\r\n\tServer: pa2 (jeenamah@usc.edu)\r\n\tContent-Type: " + content_type + "\r\n\tContent-Length: " + to_string(size) + "\r\n\tContent-MD5: " + md5 + "\r\n\t\r\n");

               

                // Read contents of the file

                // Open file: 
                int fd = open(path.c_str(), O_RDONLY);
                if (fd == -1) {
                    cerr << "Could not open file" << endl; 
                } else {

                    // Determine true speed value
                    size_t lastSlash = uri.find_last_of('/');
                    string fileName = (lastSlash != string::npos) ? uri.substr(lastSlash + 1) : uri;
                    string fileExtension;
                    size_t lastDot = fileName.find_last_of('.');
                    if (lastDot != string::npos && lastDot != fileName.length() - 1) {
                        fileExtension = fileName.substr(lastDot + 1);  // Return the extension
                    } else {
                        fileExtension = "*";
                    }
                    

                    vector<int> allInfo;
                    if (config.find(fileExtension) != config.end()){
                        allInfo = config[fileExtension];
                    } else {
                        allInfo = config["*"];
                    }
                    
                    int p = allInfo[0];
                    int maxr = allInfo[1];
                    int dial;
                    if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]){
                        dial = tempDialMap[conn_ptr->conn_number];
                    } else {

                        dial = allInfo[2];
                    }
                    
                    
                    double r = (maxr/(p*1.0)) * (dial/100.0); // R = Speed

                     response_header = "HTTP/1.1 200 OK\r\nServer: pa3 (jeenamah@usc.edu)\r\nContent-Type: " + content_type + "\r\nContent-Length: " + to_string(size) + "\r\nContent-MD5: " + md5 + "\r\n\r\n";
                    //LogALine("\tHTTP/1.1 200 OK\r\n\tServer: pa3 (jeenamah@usc.edu)\r\n\tContent-Type: " + content_type + "\r\n\tContent-Length: " + to_string(size) + "\r\n\tContent-MD5: " + md5 + "\r\n\t\r\n");
                    
                    better_write_header(conn_ptr->socket_fd, response_header.c_str(), static_cast<int>(response_header.length()));
                    
                    std::ostringstream oss;
                    oss << "[" << get_timestamp_now() << "] RESPONSE[" << conn_ptr->conn_number << "]: "
                        << get_ip_and_port_for_server(conn_ptr->socket_fd, 0)
                        << ", status=200, P=" << p
                        << ", MAXR=" << maxr << " tokens/s, DIAL=" << dial << "%"
                        << ", rate=" << std::fixed << std::setprecision(3) << r << " KB/s\n";

                    string line_response = oss.str();
                    LogALine(line_response);

                    string connection_number_string = "[" + to_string(conn_ptr->conn_number) + "]";
                    LogALine("\t"+connection_number_string + "\tHTTP/1.1 200 OK\r\n\t" +connection_number_string +
                        "\tServer: pa3 (jeenamah@usc.edu)\r\n\t" +connection_number_string +"\tContent-Type: " + content_type + "\r\n\t"
                        +connection_number_string + "\tContent-Length: " + to_string(size) + "\r\n\t" +connection_number_string +"\tContent-MD5: " + md5 + "\r\n\t" +connection_number_string +"\t\r\n");
                    
                    struct timeval t1;
                    gettimeofday(&t1, NULL);
                    int b1 = 1;

                    char buf[1024];
                    int X = size;
                    int numIterations = 0;
                    gettimeofday(&conn_ptr->start_time, NULL);
                    conn_ptr->kb_sent = 0;
                    while (X > 0) {
                        
                        if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]){
                            dial = tempDialMap[conn_ptr->conn_number];
                        } else {
                            dial = allInfo[2];
                            
                        }
                    

                        r = (maxr/(p*1.0)) * (dial/100.0); // R = Speed
                        int bytes_to_read = (X > 1024) ? 1024 : X; /* cannot read more than the size of the buffer */
                        int bytes_actually_read = read(fd, buf, bytes_to_read);

                        if (bytes_actually_read <= 0) {
                            cerr << "Error in reading file contents" << endl;
                            break; 
                        }
                        int check1 = better_write(conn_ptr->socket_fd, buf, bytes_actually_read);
                        if (check1 == -1){
                            m.lock();
                            if (conn_ptr->socket_fd >= 0){
                                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                            }
                            conn_ptr->socket_fd = -1;
                            close(conn_ptr->orig_socket_fd);
                            
                            string line_closed = "["+get_timestamp_now()+"] " + "CLOSE[" + to_string(conn_ptr->conn_number) + "]: (unexpectedly) " + client_ip_and_port + "\n";
                            //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                            LogALine(line_closed);
                            if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]){
                                isDialSetMap[conn_ptr->conn_number] = false; // After current download, set isDialSet to false
                                tempDialMap.erase(conn_ptr->conn_number);
                                isDialSetMap.erase(conn_ptr->conn_number);
                            }

                            m.unlock();
                            return;
                        }
                        m.lock();
                        conn_ptr->kb_sent += 1;
                        if (conn_ptr->socket_fd == -2 || listen_socket_fd == -1){
                            m.unlock();
                            not_closed = false;
                            m.lock();
                            if (conn_ptr->socket_fd >= 0){
                                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                            }
                            conn_ptr->socket_fd = -1;
                            close(conn_ptr->orig_socket_fd);
                            
                            string line_closed = "["+get_timestamp_now()+"] " + "CLOSE[" + to_string(conn_ptr->conn_number) + "]: " + client_ip_and_port + "\n";
                            //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                            LogALine(line_closed);
                            if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]) {
                                isDialSetMap[conn_ptr->conn_number] = false; // Reset isDialSet
                                tempDialMap.erase(conn_ptr->conn_number);   // Remove tempDial entry
                                isDialSetMap.erase(conn_ptr->conn_number);  // Remove isDialSet entry
                            }

                            m.unlock();
                            
                            return;
                        }
                        m.unlock();
                        //string line_sent = "["+get_timestamp_now()+"] " + "[" +to_string(conn_ptr->conn_number) + "]\tSent " + to_string(numIterations) + " KB to " + client_ip_and_port + "\n";
                        //cout << "[" << conn_ptr->conn_number << "]\tSent " << numIterations << " KB to " << client_ip_and_port << "\n";
                        //LogALine(line_sent);

                        X -= bytes_actually_read;

                        bool not_enough_tokens = true;
                        while (not_enough_tokens){
                            if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]){
                                dial = tempDialMap[conn_ptr->conn_number];
                            } else {
                                dial = allInfo[2];
                                //dial = 1;
                            }
                    

                            r = (maxr/(p*1.0)) * (dial/100.0); // R = Speed
                            struct timeval t2;
                            gettimeofday(&t2, NULL);

                            int n = (int)(r * timestamp_diff_in_seconds(&t1, &t2)); // Must truncate and not round

                            if ((n > 1) || (b1 == 1 && b1-1+n >= 1) || (b1 < 1 && b1+n >= 1)){
                                add_seconds_to_timestamp(&t1, (((double)1.0)/((double)r)), &t1);
                                b1 = 1;
                                not_enough_tokens = false;
                            } else {
                                /* we must have n == 0 and b1 == 1 here */
                                b1 = 0       ;   /* b1 = b1-1+n */
                                struct timeval t3;

                                add_seconds_to_timestamp(&t1, (((double)1.0)/((double)r)), &t3); /* t3 = t1 + (1-b1)/r is the time to wake up */

                                double time_to_sleep = timestamp_diff_in_seconds(&t2, &t3);
                                double usec_to_sleep = time_to_sleep * 1000000;

                                if (usec_to_sleep > 0) {
                                    usleep(usec_to_sleep);
                                }
                            }

                            


                        }
                        m.lock();
                        if (conn_ptr->socket_fd == -2 || listen_socket_fd == -1){
                            m.unlock();
                            not_closed = false;
                            m.lock();
                            if (conn_ptr->socket_fd >= 0){
                                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                            }
                            conn_ptr->socket_fd = -1;
                            close(conn_ptr->orig_socket_fd);
                            
                            string line_closed = "["+get_timestamp_now()+"] " + "CLOSE[" + to_string(conn_ptr->conn_number) + "]: (at user's request) " + client_ip_and_port + "\n";
                            //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                            LogALine(line_closed);
                            if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]) {
                                isDialSetMap[conn_ptr->conn_number] = false; // Reset isDialSet
                                tempDialMap.erase(conn_ptr->conn_number);   // Remove tempDial entry
                                isDialSetMap.erase(conn_ptr->conn_number);  // Remove isDialSet entry
                            }

                            m.unlock();
                            return;
                        }
                        m.unlock();
                        

                    }
                    m.lock();
                    if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]) {
                        isDialSetMap[conn_ptr->conn_number] = false; // Reset isDialSet
                        tempDialMap.erase(conn_ptr->conn_number);   // Remove tempDial entry
                        isDialSetMap.erase(conn_ptr->conn_number);  // Remove isDialSet entry
                    }
                    m.unlock();
                    
                    

                    
                }
                

            }
        }

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

/**
 * This is the function you need to change to change the behavior of your client!
 *
 * @param client_socket_fd - socket that can be used to "talk" (i.e., read/write) to the server.
 */
static
void talk_to_user_and_server(int client_socket_fd, vector<string>& uris, string host, int port, vector<string>& outputfiles)
{

    for (int j = 0; j < uris.size(); j++){
        string uri = uris[j];
        string outputfile = outputfiles[j];

        string request = "GET " + uri + " HTTP/1.1\r\nUser-Agent: lab4b\r\nAccept: */*\r\nHost: "+host+":"+to_string(port)+"\r\n\r\n";

        // Print request header to cout
        //cout << "\tGET " + uri + " HTTP/1.1\r\n\tUser-Agent: lab4b\r\n\tAccept: */*\r\n\tHost: "+host+":"+to_string(port)+"\r\n\t\r\n";
            
        int bytes_sent = better_write(client_socket_fd, request.c_str(), request.length());
                
        if (bytes_sent == -1){
            cerr << "Error: Failure in sending request to server" << endl;
            return;
        }
                
        int bytes_received;
        string line;
        vector<string> header_lines;

        for (;;) {
            bytes_received = read_a_line(client_socket_fd, line);

            // If read_a_line returns a value <= 0: socket is dead
            if (bytes_received <= 0){
                cerr << "Socket is dead, never read from this socket again." << endl;
                shutdown(client_socket_fd, SHUT_RDWR);
                close(client_socket_fd);
                break;
            }

            // Reached the end of the header
            if (line == "\r\n"){
                header_lines.push_back(line);
                break;
            }

            // Otherwise, append the line to header_lines
            header_lines.push_back(line);
        }

        // Print the HTTP response header to cout
        //for (string& header_line : header_lines){
            //cout << "\t" << header_line;
        //}

        // Step 1: Process response header: 
        string response_line = header_lines[0];
        string version, status;
        size_t space1 = response_line.find(' ');
        size_t space2 = response_line.find(' ', space1 + 1);

        // Parse the version, status
        version = response_line.substr(0, space1);
        status = response_line.substr(space1 + 1, space2 - space1 - 1);
            
        // Verify method and version
        if (status != "200" && status != "404") {
            cerr << "Invalid status - must be 200 or 404" << endl;
            shutdown(client_socket_fd, SHUT_RDWR);
            close(client_socket_fd);
            return;
        }

        if (version.substr(0, 7) != "HTTP/1.") {
            cerr << "Invalid version - must be HTTP/1." << endl;
            shutdown(client_socket_fd, SHUT_RDWR);
            close(client_socket_fd);
            return;
        }

        bool isContentLength = false;
        string value;
        for (string& header_line : header_lines){
            string lower_header = header_line;
            transform(lower_header.begin(), lower_header.end(), lower_header.begin(), ::tolower);
            if (lower_header.find("content-length") != string::npos) {
                isContentLength = true;
                size_t colon_pos = header_line.find(':');
                value = header_line.substr(colon_pos + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
                    return !isspace(ch); // Find first non-space character from the end
                }).base(), value.end());

            }
        } 

        if (isContentLength){ // If Content-Length was found:
            int fd = open(outputfile.c_str(), O_RDONLY);
            if (fd == (-1)) {
                fd = open(outputfile.c_str(), O_WRONLY|O_CREAT, 0600);
            } else {
                close(fd);
                fd = open(outputfile.c_str(), O_WRONLY|O_TRUNC);
            }   
            if (fd == -1) {
                cerr << "Could not open file" << endl; 
            } else {
                char buf[1024];
                int X = stoi(value);

                while (X > 0) {
                    int bytes_to_read = (X > 1024) ? 1024 : X; /* cannot read more than the size of the buffer */
                    int bytes_actually_read = read(client_socket_fd, buf, bytes_to_read);

                    if (bytes_actually_read <= 0) {
                        cerr << "Error in reading file contents" << endl;
                        break; 
                    }
                    better_write(fd, buf, bytes_actually_read);
                    X -= bytes_actually_read;
                }

                close(fd);
            }
        }

    }
    
    
    shutdown(client_socket_fd, SHUT_RDWR);
    close(client_socket_fd);
}

void console_thread() {
    string command;

    while (true) {
        cout << "> ";
        getline(cin, command);
        if (command == "status"){
            m.lock();

            bool has_active_connections = false;
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->socket_fd != -1) {  
                    has_active_connections = true;
                    break;
                }
            }

            if (!has_active_connections) {
                cout << "No active connections." << endl;
            } else {
                for (const auto& conn_ptr : connection_list) {
                    if (conn_ptr->socket_fd >= 0) {
                        string last_uri = conn_ptr->last_uri;

                        // Determine true speed value
                        size_t lastSlash = last_uri.find_last_of('/');
                        string fileName = (lastSlash != string::npos) ? last_uri.substr(lastSlash + 1) : last_uri;
                        string fileExtension;
                        size_t lastDot = fileName.find_last_of('.');
                        if (lastDot != string::npos && lastDot != fileName.length() - 1) {

                            fileExtension = fileName.substr(lastDot + 1);  // Return the extension
                            if (config.find(fileExtension) == config.end()){
                                fileExtension = "*";
                            }
                        } else {
                            fileExtension = "*";
                        }

                        vector<int> allInfo = config[fileExtension];
                        int p = allInfo[0];
                        int maxr = allInfo[1];  // Skip inactive connections
                        int dial;
                        if (isDialSetMap.find(conn_ptr->conn_number) != isDialSetMap.end() && isDialSetMap[conn_ptr->conn_number]){
                            dial = tempDialMap[conn_ptr->conn_number];
                        } else {
                            dial = allInfo[2];
                        }
                        double rate = (maxr * dial)/(p*100.0);

                        double F = (100.0*conn_ptr->kb_sent*1024)/(conn_ptr->last_content_length);

                        struct timeval now;
                        gettimeofday(&now, NULL);
                        double time_elapsed = timestamp_diff_in_seconds(&conn_ptr->start_time, &now); 
                        
                        cout << "[" << conn_ptr->conn_number << "]\tClient: "
                             << get_ip_and_port_for_server(conn_ptr->socket_fd, 0)
                             << "\n\tPath: " << conn_ptr->last_uri 
                             << "\n\tContent-Length: " << conn_ptr->last_content_length
                             << "\n\tStart-Time: [" << format_timestamp(&conn_ptr->start_time) << "]"
                             << "\n\tShaper-Params: P=" << p << ", MAXR=" << maxr << " tokens/s, DIAL=" << dial << "%, rate=" << std::fixed << std::setprecision(3) << rate << " KB/s"
                             << "\n\tSent: " << conn_ptr->kb_sent*1024 << " bytes (" << std::fixed << std::setprecision(1) << F << "%), time elapsed: " << std::fixed << std::setprecision(3) << time_elapsed << "sec\n";
                    }
                }
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
        } else if (command.substr(0, 5) == "close"){
            m.lock();
            shared_ptr<Connection> targetConnection = nullptr;
            istringstream iss(command);
            string cmd, number;

            iss >> cmd >> number;

            if (number.empty()) {
                cout << "Malformed close command.  Correct syntax: \"close #\".\n";
                m.unlock();
                continue;
            }
            int conn = stoi(number);
            bool hasConnection = false;
            for (const auto& conn_ptr : connection_list) {
 
                if (conn_ptr->socket_fd >= 0) {  // Skip inactive connections
                        if (conn_ptr->conn_number  == conn) {
                            targetConnection = conn_ptr;
                            hasConnection = true;
                            break;
                        }
                }
            }

            if (hasConnection && targetConnection != nullptr && targetConnection->socket_fd >= 0){
                shutdown(targetConnection->socket_fd, SHUT_RDWR);
                targetConnection->socket_fd = -2;
                cout << "Closing connection " << conn << "\n";
            } else {
                cout << "No such connection: #" << number << "\n";
            }
            m.unlock();
        } else if (command.substr(0, 4) == "dial"){
            m.lock();
            istringstream iss(command);
            string cmd, numStr, percentStr;

            iss >> cmd >> numStr >> percentStr;
            //cout << percentStr << endl;

            if (cmd != "dial" || numStr.empty() || percentStr.empty()) {
                cout << "Malformed dial command.  Correct syntax: \"dial # percent\".\n";
                m.unlock();
                continue;
            }

            // Truncate decimal part from percent if present
            size_t decimalPos = percentStr.find('.');
            if (decimalPos != string::npos) {
                percentStr = percentStr.substr(0, decimalPos);
            }

            // Check if percent is a number
            for (char c : percentStr) {
                if (!isdigit(c)) {
                    cout << "Malformed dial command.  Correct syntax: \"dial # percent\".\n";
                    m.unlock();
                    continue;
                }
            }
            int conn = stoi(numStr);
            int percent = stoi(percentStr);
            if (percent < 1 || percent > 100){
                cout << "Dial value is out of range (it must be >=1 and <= 100)\n";
                m.unlock();
                continue;
            }

            bool hasConnection = false;
            shared_ptr<Connection> targetConnection = nullptr;
            for (const auto& conn_ptr : connection_list) {
 
                if (conn_ptr->socket_fd >= 0) {  // Skip inactive connections
                        if (conn_ptr->conn_number  == conn) {
                            targetConnection = conn_ptr;
                            hasConnection = true;
                            break;
                        }
                }
            }

            if (hasConnection && targetConnection != nullptr && targetConnection->socket_fd >= 0){
                tempTargetConnection = targetConnection;
                tempDialMap[targetConnection->conn_number] = percent;
                isDialSetMap[targetConnection->conn_number] = true;
                tempConnNumber = conn;
                string last_uri = targetConnection->last_uri;

                // Determine true speed value
                size_t lastSlash = last_uri.find_last_of('/');
                string fileName = (lastSlash != string::npos) ? last_uri.substr(lastSlash + 1) : last_uri;
                string fileExtension;
                size_t lastDot = fileName.find_last_of('.');
                if (lastDot != string::npos && lastDot != fileName.length() - 1) {
                    fileExtension = fileName.substr(lastDot + 1);  // Return the extension
                    if (config.find(fileExtension) == config.end()){
                        fileExtension = "*";
                    }
                } else {
                    fileExtension = "*";
                }

                vector<int> allInfo = config[fileExtension];
                int p = allInfo[0];
                int maxr = allInfo[1];

                double token_rate = maxr * (tempDialMap[targetConnection->conn_number]/100.0);
                double data_rate = (maxr/(p*1.0))*(tempDialMap[targetConnection->conn_number]/100.0);

                cout << std::fixed << std::setprecision(3) << "Dial for connection " << numStr << " at " << percentStr << "%.  Token rate at "<< token_rate << " tokens/s.  Data rate at " << data_rate << " KB/s.\n";
                std::ostringstream oss;
                oss << "[" << get_timestamp_now() << "] Shaper-Params[" << conn << "]: P=" 
                    << p << ", MAXR=" << maxr << " tokens/s, DIAL=" << tempDialMap[targetConnection->conn_number] 
                    << "%, rate=" << std::fixed << std::setprecision(3) << data_rate << " KB/s\n";

LogALine(oss.str());    
            } else {
                cout << "No such connection: " << numStr << "\n";
            }
            
            m.unlock();
        } else if (command == ""){
            continue;
        } else if (command == "help"){
            cout << "Available commands are:\n\tclose #\n\tdial # percent\n\tquit\n\tstatus\n";
        } else {
            cout << "Command not recognized.  Valid commands are:\n\tclose #\n\tdial # percent\n\tquit\n\tstatus\n";
        }

    }
    m.lock();
          
    shutdown(listen_socket_fd, SHUT_RDWR);
    close(listen_socket_fd);
    listen_socket_fd = -1;
                
    for (const auto& conn_ptr : connection_list) {
        if (conn_ptr->socket_fd >= 0) {
            //cout << conn_ptr->conn_number << endl;
            shutdown(conn_ptr->socket_fd, SHUT_RDWR);
            conn_ptr->socket_fd = -2;
        }
    }
                
            

    m.unlock();
}

void reaper_thread(){
    while (true){
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
                    //string line_join = "[" + get_timestamp_now() + "] [" + to_string(connection_ptr->conn_number) + "]\tReaper has joined with connection thread\n";
                    //LogALine(line_join);
                    itr = connection_list.erase(itr);
                } else {
                    itr++;
                }
            }
        }
        m.unlock();        
    }

}



int main(int argc, char *argv[])
{
    if (argc >= 6){
        string host = argv[2];

        vector<string> uris;
        vector<string> outputfiles;

        for (int j = 4; j < argc; j+= 2){
            string uri = argv[j];
            string outputfile = argv[j+1];

            if (uri.back() == '/'){
                cerr << "Invalid format for URI" << endl;
                exit(-1);
            }
            if (uri[0] != '/'){
                uri = "/" + uri;
            }
            uris.push_back(uri);
            outputfiles.push_back(outputfile);
        }
        
        
        int client_socket_fd = create_client_socket_and_connect(host, argv[3]);
        if (client_socket_fd == (-1)) {
            cerr << "Cannot connect to " << host << ":" << argv[3] << endl;
            exit(-1);
        } else {
            string client_ip_and_port = get_ip_and_port_for_client(client_socket_fd, 1);
            string server_ip_and_port = get_ip_and_port_for_client(client_socket_fd, 0);
            cerr << "echo-client at " << client_ip_and_port << " is connected to server at " << server_ip_and_port << endl;
        }

        talk_to_user_and_server(client_socket_fd, uris, host, stoi(argv[3]), outputfiles);

        return 0;
    } else {
        //process_options(argc, argv);
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
        string pidfile;
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
                } else if (key == "pidfile") {
                    pidfile = file_lines[i].substr(equals_pos+1, file_lines[i].length() - equals_pos - 2); 
                } else if (key == "rootdir") {
                    rootdir = file_lines[i].substr(equals_pos+1, file_lines[i].length() - equals_pos - 2); 
                }
            }
            i = i + 1;
        }

        

        
        for (const auto& section : sectionsAndLocation) {
            if (section.first == "startup"){
                continue;
            }
            int i = section.second;
            vector<int> info; 
            for (int j = i + 1; j <= i + 3; j++){
                size_t equals_pos = file_lines[j].find("=");
                string key = file_lines[j].substr(0, equals_pos);
                string val = file_lines[j].substr(equals_pos+1, file_lines[j].length() - equals_pos - 2);
                int value = stoi(val);
                info.push_back(value);
            }
            config[section.first] = info;
            
        }

        /*
        for (const auto& entry : config) {
            cout << "Section: " << entry.first << " -> [";
            for (size_t i = 0; i < entry.second.size(); i++) {
                cout << entry.second[i];
                if (i < entry.second.size() - 1) cout << ", "; // Avoid trailing comma
            }
            cout << "]" << endl;
        }
        */


        
        if (logToFile){
            int log_file_fd = open_file_for_writing(logfile);
            if (log_file_fd == -1){
                cerr << "Cannot open logfile" << endl;
                exit(-1);
            }
        }

        Init(logToFile, logfile); // Sets up mylog
        
        listen_socket_fd = create_listening_socket(port);

        /* Write to PID */
        int fd_for_pid = open_file_for_writing_trunc(pidfile);
        if (fd_for_pid == -1){
            cerr << "Cannot open pidfile" << endl;
        }


        pid_t pid = getpid();
    
        
        string pid_str = to_string(pid) + "\n"; 

    
        if (write(fd_for_pid, pid_str.c_str(), pid_str.size()) == -1) {
            cerr << "Failed to write PID to file" << endl;
        }

    
        close(fd_for_pid);
        /* Write to PID */

        string server_ip_and_port;
        if (listen_socket_fd != (-1)) {
            server_ip_and_port = get_ip_and_port_for_server(listen_socket_fd, 1);
            string line_start = "["+get_timestamp_now()+"]" + " START: port="+port+", rootdir=\'"+rootdir+"\'\n";
            LogALine(line_start);


            if (gnDebug) {
                string s = get_ip_and_port_for_server(listen_socket_fd, 1);
                cout << "[SERVER]\tlistening at " << s << endl;
            }

            thread console(console_thread);
            thread reaper(reaper_thread);

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
                string line_connect = "["+get_timestamp_now()+"]" + " CONNECT["+ to_string(next_conn_number) +"]: " + get_ip_and_port_for_server(newsockfd, 0) + "\n";
                LogALine(line_connect);
                shared_ptr<Connection> conn_ptr = make_shared<Connection>(Connection(next_conn_number++, newsockfd, NULL));

                shared_ptr<thread> thr_ptr = make_shared<thread>(thread(talk_to_client, conn_ptr));
                conn_ptr->thread_ptr = thr_ptr;
                connection_list.push_back(conn_ptr);
                m.unlock();
            }

            console.join();
            reaper.join();
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->thread_ptr) {
                    conn_ptr->thread_ptr->join();
                }
            }

            
            string line_die = "["+get_timestamp_now()+"] " + "STOP: port="+port+"\n";
            LogALine(line_die);
            
        }
        
        return 0;
    }
    
}