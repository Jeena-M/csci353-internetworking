/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>

using namespace std;
    
/* C system include files next */
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

/* C standard include files next */
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

/* your local include files next */
/* nothing to to #include for this program */

int main(int argc, char *argv[])
{
    /* this is how to iterate through commandline arguments */
    for (int i=1; i < argc; i++) cout << argv[i] << endl;

    cout << "Hello World!" << endl;
    return 0;
}

