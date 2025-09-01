/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>

using namespace std;
    
/* C system include files next */
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

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

int read_a_line(int fd, string& line)
{
	string s = "";
	int idx = 0;
	char ch = '\0';

	for (;;) {
		int bytes_read = read(fd, &ch, 1);
		if (bytes_read < 0) {
			if (errno == EINTR) {
				continue;
			}
			return (-1);
		} else if (bytes_read == 0) {
			if (idx == 0) return (-1);
				break;
			} else {
				s += ch;
				idx++;
				if (ch == '\n') {
					break;
				}
			}
		}
		line = s;
		return idx;
	}
int main(int argc, char *argv[])
{
	int fd = open(argv[1], O_RDONLY);

	if (fd < 0){ // If open failed, fd is negative:
		perror("open");
		exit(1);
	}

	int count = 0;

	for (;;) {
    	string line;
    	if (read_a_line(fd, line) <= 0) {
        	break;
    	}
    	// your code to print line length
    	if (line.length() > 0 and line[line.length() - 1] == '\n'){ // Exclude newline from line length
            int true_line_length = line.length()-1;
            cout << true_line_length << endl;
        } else {
            cout << line.length() << endl;

        }
    	count ++;
	}
	// your code to print number of lines read
	cout << "number of lines read: " << count << endl;

	close(fd); // Close the file b/c done using the file

	return 0;
}
