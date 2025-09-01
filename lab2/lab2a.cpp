/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

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
int get_int(string& line)
{
	return stoi(line);
	}

pair<int, int> get_two_ints (string& line)
{
	int n1, n2;
	size_t pos_of_space = line.find(' ');
	n1 = stoi(line.substr(0, pos_of_space));
	n2 = stoi(line.substr(pos_of_space));
	return make_pair(n1, n2);
	}

int main(int argc, char *argv[])
{
	int fd = open(argv[1], O_RDONLY);

	if (fd < 0){ // If open failed, fd is negative:
		perror("open");
		exit(1);
	}


	string beg_line;

	// Read the first line and convert the # of nodes to an int
	read_a_line(fd, beg_line);
	int N = get_int(beg_line);

	// Create the nodes vector 
	vector<vector<int>> nodes(N);

	// Read the second line and convert the # of edges to an int
	read_a_line(fd, beg_line);
	int E = get_int(beg_line);
	
	// Add the appropriate edges
	for (int i = 1; i <= E; i++) {
    	string line;
    	read_a_line(fd, line);
    	pair<int, int> edge = get_two_ints(line);
    	int n1 = edge.first;
    	int n2 = edge.second;
    	
    	nodes[n1].push_back(n2);
    	nodes[n2].push_back(n1);
    	 
	}

	close(fd); // Close the file b/c done using the file

	// Print adjacency list
	for (int i = 0; i < N; i++){
		cout << "neighbors of node " << i << ": ";
		for (int element: nodes[i]){
			cout << element << " ";
		}
		cout << "\n";
	}

	return 0;
}