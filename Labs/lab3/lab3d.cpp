/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>

using namespace std;

/* C system include files next */
#include <arpa/inet.h>
#include <netdb.h>

/* C standard include files next */
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

/* your own include last */
#include "my_socket.h"

static int listen_socket_fd = (-1); /* there is nothing wrong with using a global variable */
static int gnDebug = 1; /* change it to 0 if you don't want debugging messages */

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
    if (argc != 2) {
        usage_server();
    }
}

/**
 * This is the function you need to change to change the behavior of your server!
 *
 * @param newsockfd - socket that can be used to "talk" (i.e., read/write) to the client.
 */
static
void talk_to_client(int newsockfd)
{
    int bytes_received;
    string line;

    while ((bytes_received = read_a_line(newsockfd, line)) > 0) {
        if (gnDebug) {
            cerr << "[DBG-SVR] " << dec << bytes_received << " bytes received from " << get_ip_and_port_for_server(newsockfd, 0) << " (data displayed in next line, <TAB>-indented):\n\t";
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
        better_write(newsockfd, line.c_str(), bytes_received);

    }

    shutdown(newsockfd, SHUT_RDWR);
    close(newsockfd);
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
void talk_to_user_and_server(int client_socket_fd)
{
    for (;;){
        cout << "Enter a string to send to the echo server: ";
        string message;
        getline(cin, message);
        message += '\n'; // Append the newline (that getline removes) before sending to server
        if (message.length() > 0) {
            int bytes_sent = better_write(client_socket_fd, message.c_str(), message.length());
            if (bytes_sent == -1){
                cerr << "Error: Failure in sending message to server" << endl;
                break;
            }
            cout << bytes_sent << " bytes sent from " << get_ip_and_port_for_client(client_socket_fd, 1) << endl;

            string line;
            int bytes_received = read_a_line(client_socket_fd, line);
            if (bytes_received == -1){
                cerr << "Error: Failure in reading line from server" << endl;
                break;
            }
            cout << bytes_received << " bytes received, see next line for data..." << endl << line;
        }
        // Break out of the infinite loop if message is simply '\n'
        if (message == "\n"){
            break;
        }
    }
    
    shutdown(client_socket_fd, SHUT_RDWR);
    close(client_socket_fd);
}




int main(int argc, char *argv[])
{
    if (argc == 3){
        int client_socket_fd = create_client_socket_and_connect(LOCALHOST, argv[2]);
        if (client_socket_fd == (-1)) {
            cerr << "Cannot connect to " << LOCALHOST << ":" << argv[2] << endl;
            exit(-1);
        } else {
            string client_ip_and_port = get_ip_and_port_for_client(client_socket_fd, 1);
            string server_ip_and_port = get_ip_and_port_for_client(client_socket_fd, 0);
            cerr << "echo-client at " << client_ip_and_port << " is connected to server at " << server_ip_and_port << endl;
        }
        talk_to_user_and_server(client_socket_fd);

        return 0;
    } else {
        process_options(argc, argv);
        listen_socket_fd = create_listening_socket(argv[1]);
        if (listen_socket_fd != (-1)) {
            if (gnDebug) {
                string s = get_ip_and_port_for_server(listen_socket_fd, 1);
                cout << "[SERVER]\tlistening at " << s << endl;
            }
            for (;;) {
                int newsockfd = my_accept(listen_socket_fd);
                if (newsockfd == (-1)) break;
                talk_to_client(newsockfd);
            }
            shutdown(listen_socket_fd, SHUT_RDWR);
            close(listen_socket_fd);
        }
        return 0;
    }
    
}
