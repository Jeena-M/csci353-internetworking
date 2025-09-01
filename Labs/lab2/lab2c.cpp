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

tuple<int, int, int> get_three_ints (string& line)
{
	int n1, n2, edge_cost;
	size_t pos_of_first_space = line.find(' ');
	n1 = stoi(line.substr(0, pos_of_first_space));

	size_t pos_of_second_space = line.find(' ', pos_of_first_space + 1);
	n2 = stoi(line.substr(pos_of_first_space, pos_of_second_space));

	edge_cost = stoi(line.substr(pos_of_second_space));
	return make_tuple(n1, n2, edge_cost);
	}

int delete_min (vector<int>& list, vector<int>& distance)
{
	if (list.size() == 0){
		return -1;
	}
	int min = distance[list[0]];
	int index = 0;
	for (size_t i = 0; i < list.size(); i++){
		if (distance[list[i]] < min) {
			min = distance[list[i]];
			index = i;
		}
	}
	int node_min = list[index];
	list.erase(list.begin() + index); // Delete min element
	return node_min; // Return min element
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
	vector<vector<pair<int, int>>> nodes(N);
	vector<int> distance(N);
	vector<bool> in_solution(N);
	vector<int> preds(N);

	// Read the second line and convert the # of edges to an int
	read_a_line(fd, beg_line);
	int E = get_int(beg_line);

	// Initialize the vectors
	for (int i = 0; i < N; i++){
		distance[i] = 999999999;
		in_solution[i] = false;
		preds[i] = -1;
	}
	
	// Add the appropriate edges
	for (int i = 1; i <= E; i++) {
    	string line;
    	read_a_line(fd, line);

    	int n1, n2, c;

    	tie(n1, n2, c) = get_three_ints(line);
    	
    	nodes[n1].push_back(make_pair(n2, c));
    	nodes[n2].push_back(make_pair(n1, c));
    	 
	}

	close(fd); // Close the file b/c done using the file

	// Dijkstra initialization 
	distance[start_node] = 0;
	in_solution[start_node] = true;
	preds[start_node] = start_node;
	for (pair<int, int> neighbor: nodes[start_node]){
		int neighbor_node = neighbor.first;
		int edge_cost = neighbor.second;
		distance[neighbor_node] = edge_cost;
		preds[neighbor_node] = start_node;
	}


	
	vector<int> list; // Will contain a list of all node indices
	for (int i = 0; i < N; i++){
		list.push_back(i);
	}

	// Dijkstra

	int itr = 0;

	while (list.size() != 0){
		int w = delete_min(list, distance);

		if (w == -1 || distance[w] == 999999999){
			break;
		}

		in_solution[w] = true;

		cout << "itr " << itr << ": add node " << w << ", distance " << distance[w] << ", predecessor " << preds[w] << endl;
	
		for (pair<int, int> v: nodes[w]){ // For each of w's neighbors v
			int neighbor_node = v.first;
			int neighbor_c = v.second;

			if (in_solution[neighbor_node] == false){
				if (distance[w] + neighbor_c < distance[neighbor_node]){
					distance[neighbor_node] = distance[w] + neighbor_c;
					preds[neighbor_node] = w;
				}
			}
		}
		itr += 1;
	}


	return 0;
}