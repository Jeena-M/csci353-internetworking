/* C++ standard include files first */
#include <iostream>
#include <fstream>
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
	ifstream file(argv[1]); // Open the user-given file

    string line;
    int count = 0;

    while (true){
    	getline(file, line); // Read a file line 

    	if (file.fail()) {
    		break;
    	}

    	cout << line.length() << endl;

    	count++;
    }

    cout << "number of lines read: " << count << endl;

    file.close(); // Close the file

    return 0;
}