/* C++ standard include files first */
#include <iostream>
#include <string>

using namespace std;

/* C system include files next */
#include <arpa/inet.h>
#include <netdb.h>

/* C standard include files next */
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* your own include last */
#include "my_socket.h"

static
void usage()
{
    fprintf(stderr, "./echo-client port_number\n");
    exit(-1);
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
    if (argc != 2) {
        usage();
    }
    int client_socket_fd = create_client_socket_and_connect(LOCALHOST, argv[1]);
    if (client_socket_fd == (-1)) {
        cerr << "Cannot connect to " << LOCALHOST << ":" << argv[1] << endl;
        exit(-1);
    } else {
        string client_ip_and_port = get_ip_and_port_for_client(client_socket_fd, 1);
        string server_ip_and_port = get_ip_and_port_for_client(client_socket_fd, 0);
        cerr << "echo-client at " << client_ip_and_port << " is connected to server at " << server_ip_and_port << endl;
    }
    talk_to_user_and_server(client_socket_fd);

    return 0;
}
