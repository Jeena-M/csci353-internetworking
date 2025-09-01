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
    int bytes_received = 0;
    char buf[1024];

    if (gnDebug) {
        for (unsigned int i=0; i < sizeof(buf); i++) {
            buf[i] = (char)(256 - (i % 256));
        }
    }
    bytes_received = read(newsockfd, buf, sizeof(buf));
    if (gnDebug) {
        cerr << "[DBG-SVR] " << dec << bytes_received << " bytes received from " << get_ip_and_port_for_server(newsockfd, 0) << " (data displayed in next line, <TAB>-indented):\n\t";
        write(2, buf, bytes_received);
        if (!(bytes_received > 0 && buf[bytes_received-1] == '\n')) cerr << endl;
        if (bytes_received > 0) {
            cerr << "\t";
            for (int i=0; i < bytes_received; i++) {
                cerr << "0x" << setfill('0') << setw(2) << hex << (int)buf[i] << " ";
            }
            cerr << endl;
        }
    }
    write(newsockfd, buf, bytes_received);

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
    cout << "Enter a string to send to the echo server: ";
    string message;
    getline(cin, message);
    if (message.length() > 0) {
        message += '\n';
        int bytes_sent = write(client_socket_fd, message.c_str(), message.length());
        cout << bytes_sent << " bytes sent from " << get_ip_and_port_for_client(client_socket_fd, 1) << endl;

        char buf[1024];
        int bytes_received = read(client_socket_fd, buf, sizeof(buf));
        buf[bytes_received] = '\0';
        cout << bytes_received << " bytes received, see next line for data..." << endl << buf;
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
