/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
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

/* C standard include files next */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

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

static ofstream file;
ostream *mylog=NULL;

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

    for (;;){

        header_lines.clear();
        for (;;) {
            bytes_received = read_a_line(conn_ptr->socket_fd, line);

            // If read_a_line returns a value <= 0: socket is dead
            if (bytes_received <= 0){
                m.lock();
                string line_closed = "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tConnection closed with client at " + client_ip_and_port + "\n";
                //cout << "[" << conn_ptr->conn_number << "]\t Connection closed with client at " << client_ip_and_port << "\n";
                LogALine(line_closed);

                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                close(conn_ptr->socket_fd);
                conn_ptr->socket_fd = (-1);
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

            // Verify method and version
            if (method != "GET") {
                cerr << "Invalid HTTP Method - must be GET" << endl;
                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                close(conn_ptr->socket_fd);
                return;
            }

            if (version.substr(0, 7) != "HTTP/1.") {
                cerr << "Invalid version - must be HTTP/1." << endl;
                shutdown(conn_ptr->socket_fd, SHUT_RDWR);
                close(conn_ptr->socket_fd);
                return;
            }

            // After obtaining the URI:
            
            string line_connect =  "["+get_timestamp_now()+"] " + "[" + to_string(conn_ptr->conn_number) + "]\tClient connected from "+ client_ip_and_port + " and requesting " +uri + "\n";
            //cout << "[" << conn_ptr->conn_number << "]\tClient connected from " << client_ip_and_port << " and requesting " << uri << "\n";
            LogALine(line_connect);


            // Print the HTTP request header to cout
            //for (string& header_line : header_lines){
                //cout << "\t" << header_line;
            //}
            
            // Step 2: Create response

            string path = "lab4data/" + uri; // Create a file system path

            // Call get_file_size()
            int size = get_file_size(path);
            

            if (size == -1){
                string response = "HTTP/1.1 404 Not Found\r\nServer: lab4a\r\nContent-Type: text/html\r\nContent-Length: 63\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
                better_write_header(conn_ptr->socket_fd, response.c_str(), static_cast<int>(response.length()));
                //cout << "\tHTTP/1.1 404 Not Found\r\n\tServer: lab4a\r\n\tContent-Type: text/html\r\n\tContent-Length: 63\r\n\t\r\n\t<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
            } else {

                size_t lastPeriod = uri.rfind('.'); // Locate the last period in the uri
                string extension = uri.substr(lastPeriod + 1);
                transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                string response_header;
                if (extension == "html") {
                    response_header = "HTTP/1.1 200 OK\r\nServer: lab4a\r\nContent-Type: text/html\r\nContent-Length: " + to_string(size) + "\r\n\r\n";
                    //cout << "\tHTTP/1.1 200 OK\r\n\tServer: lab4a\r\n\tContent-Type: text/html\r\n\tContent-Length: " + to_string(size) + "\r\n\t\r\n";
            
                } else {
                    response_header = "HTTP/1.1 200 OK\r\nServer: lab4a\r\nContent-Type: application/octet-stream\r\nContent-Length: " + to_string(size) + "\r\n\r\n";
                    //cout << "\tHTTP/1.1 200 OK\r\n\tServer: lab4a\r\n\tContent-Type: application/octet-stream\r\n\tContent-Length: " + to_string(size) + "\r\n\t\r\n";
                }

                better_write_header(conn_ptr->socket_fd, response_header.c_str(), static_cast<int>(response_header.length()));
                
                // Read contents of the file

                // Open file: 
                int fd = open(path.c_str(), O_RDONLY);
                if (fd == -1) {
                    cerr << "Could not open file" << endl; 
                } else {

                    double r = N*1.0; // R = Speed
                    struct timeval t1;
                    gettimeofday(&t1, NULL);
                    int b1 = 1;

                    char buf[1024];
                    int X = size;
                    int numIterations = 0;

                    while (X > 0) {
                        int bytes_to_read = (X > 1024) ? 1024 : X; /* cannot read more than the size of the buffer */
                        int bytes_actually_read = read(fd, buf, bytes_to_read);

                        if (bytes_actually_read <= 0) {
                            cerr << "Error in reading file contents" << endl;
                            break; 
                        }
                        better_write(conn_ptr->socket_fd, buf, bytes_actually_read);
                        numIterations += 1;
                        //string line_sent = "["+get_timestamp_now()+"] " + "[" +to_string(conn_ptr->conn_number) + "]\tSent " + to_string(numIterations) + " KB to " + client_ip_and_port + "\n";
                        //cout << "[" << conn_ptr->conn_number << "]\tSent " << numIterations << " KB to " << client_ip_and_port << "\n";
                        //LogALine(line_sent);

                        X -= bytes_actually_read;

                        bool not_enough_tokens = true;
                        while (not_enough_tokens){
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
                    }

                    close(fd);
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
                cout << "Active connections:" << endl;
                for (const auto& conn_ptr : connection_list) {
                    if (conn_ptr->socket_fd != -1) {  // Skip inactive connections
                        cout << conn_ptr->conn_number << " ";
                    }
                }
                cout << "\n";
            }
            m.unlock();

        } else if (command == "quit") {
            m.lock();
            bool has_active_connections = false;
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->socket_fd != -1) {  
                    has_active_connections = true;
                    break;
                }
            }

            if (has_active_connections) {
                cout << "Cannot quit, the following connections are active: ";
                for (const auto& conn_ptr : connection_list) {
                    if (conn_ptr->socket_fd != -1) {  // Skip inactive connections
                        cout << conn_ptr->conn_number << " ";
                    }
                }
                cout << "\n";
                
            } else {
                shutdown(listen_socket_fd, SHUT_RDWR);
                close(listen_socket_fd);
                listen_socket_fd = -1;
                
                
            }

            m.unlock();
            if (listen_socket_fd == -1){
                break;
            }
        } else if (command == "help"){
            cout << "Available commands are:\n\thelp\n\tstatus\n\tquit\n";
        } else if (command == ""){
            continue;
        } else {
            cout << "Available commands are:\n\thelp\n\tstatus\n\tquit\n";
        }

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
        process_options(argc, argv);
        string logfile = argv[2];
        bool logToFile = true;
        Init(logToFile, logfile); // Sets up mylog
        
        listen_socket_fd = create_listening_socket(argv[1]);
        string server_ip_and_port;
        if (listen_socket_fd != (-1)) {
            server_ip_and_port = get_ip_and_port_for_server(listen_socket_fd, 1);
            string line_start = "["+get_timestamp_now()+"] " + "Server " + server_ip_and_port + " started\n";
            LogALine(line_start);


            if (gnDebug) {
                string s = get_ip_and_port_for_server(listen_socket_fd, 1);
                cout << "[SERVER]\tlistening at " << s << endl;
            }

            thread console(console_thread);

            for (;;) {
                int newsockfd = my_accept(listen_socket_fd);
                if (newsockfd == (-1)) break;
                m.lock();
                if (listen_socket_fd == -1){
                    shutdown(listen_socket_fd, SHUT_RDWR);
                    close(listen_socket_fd);
                    m.unlock();
                    break;
                }
                shared_ptr<Connection> conn_ptr = make_shared<Connection>(Connection(next_conn_number++, newsockfd, NULL));

                shared_ptr<thread> thr_ptr = make_shared<thread>(thread(talk_to_client, conn_ptr));
                conn_ptr->thread_ptr = thr_ptr;
                connection_list.push_back(conn_ptr);
                m.unlock();
            }

            console.join();
            for (const auto& conn_ptr : connection_list) {
                if (conn_ptr->thread_ptr) {
                    conn_ptr->thread_ptr->join();
                }
            }

            
            string line_die = "["+get_timestamp_now()+"] " + "Server " + server_ip_and_port + " stopped\n";
            LogALine(line_die);
            
        }
        return 0;
    }
    
}