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
void usage()
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
        usage();
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

int main(int argc, char *argv[])
{
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
