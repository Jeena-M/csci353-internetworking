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
	int start_node = stoi(argv[1]);
	int fd = open(argv[2], O_RDONLY);

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
	vector<int> levels(N);
	vector<int> preds (N);

	// Read the second line and convert the # of edges to an int
	read_a_line(fd, beg_line);
	int E = get_int(beg_line);

	// Initialize the vectors
	for (int i = 0; i < N; i ++){
		levels[i] = -1;
		preds[i] = -1;
	}
	
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

	// BFS
	vector<int> queue;
	levels[start_node] = 0;
	queue.push_back(start_node);

	while (queue.size() != 0){
		int v = queue[0]; // Access earliest (first) node
		queue.erase(queue.begin()); // Remove earliest (first) node
		cout << "level " << levels[v] <<": " << v << endl;
		for (int u: nodes[v]){ // For each of v's neighbors u
			if (levels[u] == (-1)){
				levels[u] = levels[v] + 1;
				preds[u] = v;
				queue.push_back(u);
			}
		}
	}


	return 0;
}