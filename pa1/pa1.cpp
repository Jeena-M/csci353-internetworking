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
void print_usage() {
	cerr << "usage: pa1 [r c] filename" << endl;
}
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
	if (argc != 4 && argc != 2){
		cerr << "malformed commandline - incorrect number of commandline arguments" << endl;
		print_usage();
		return 0;
	}

	int r, c;
	int fd;

	if (argc == 4){
		r = stoi(argv[1]);
		c = stoi(argv[2]);
		fd = open(argv[3], O_RDONLY);
	} else {
		r = 0;
		c = 0;
		fd = open(argv[1], O_RDONLY);
	}


	if (fd < 0){ // If open failed, fd is negative:
		perror("open");
		exit(1);
	}

	vector<string> maze;

	for (;;) {
    	string line;
    	if (read_a_line(fd, line) <= 0) {
        	break;
    	}
    	maze.push_back(line);
    }

    bool isTypeTwoMaze = false;

    for (string line: maze){
    	for (char c : line){
    		if (c >= '1' && c <= '9') {
    			isTypeTwoMaze = true;
    			break;
    		}
    	}
    }

    int numRows, numCols;
    numRows = (maze.size() - 1) / 2;

    // Verify the length of each line is the same
    int lineLength = maze[0].length();
    for (string line: maze){
    	if (line.length() != lineLength){
    		cerr << "Not all rows of the maze are the same width" << endl;
    		return 0; 
    	}
    }

    numCols = (lineLength - 2) / 2;

    // Verify root's location 
    if (r >= numRows || c >= numCols){
    	cerr << "Root is out of bounds" << endl;
    	return 0; 
    }

    // Verify valid maze
    if (numRows > 32 || numCols > 64){
    	cerr << "Invalid maze - numRows must be <= 32 and numCols must be <= 64" << endl;
    	return 0;
    }
    
    
    if (!isTypeTwoMaze){
    	int numNodes = numRows * numCols;
    	vector<vector<int>> nodes(numNodes);

    	// First process horizontal neighbors:
    	for (int row = 1; row < maze.size(); row+=2){
    		for (int pos = 2; pos < lineLength; pos+=2){
    			if (maze[row][pos] == ' '){
    				int neighbor1, neighbor2;
    				int row_of_neighbors = row / 2;
    				neighbor1 = (row_of_neighbors)*numCols + ((pos / 2)-1); // Left node to that wall
    				neighbor2 = (row_of_neighbors)*numCols + (pos / 2); // Right node to that wall
    				nodes[neighbor1].push_back(neighbor2);
    				nodes[neighbor2].push_back(neighbor1);
    			}
    		}
    	} 

    	// Next, process vertical neighbors:
    	for (int row = 0; row < maze.size()-1; row += 2){
    		for (int pos = 1; pos < lineLength; pos+=2){
    			if (maze[row][pos] == ' '){
    				int neighbor1, neighbor2;
    				int col_of_neighbors = pos / 2;
    				neighbor1 = ((row / 2)-1)*numCols + col_of_neighbors;
    				neighbor2 = (row / 2)*numCols + col_of_neighbors;
    				nodes[neighbor1].push_back(neighbor2);
    				nodes[neighbor2].push_back(neighbor1);
    			}
    		}
    	}

    	/*
    	// Print adjacency list
    	for (int i = 0; i < nodes.size(); i++){
			cout << "neighbors of node " << i << ": ";
			for (int element: nodes[i]){
				cout << element << " ";
			}
			cout << "\n";
		}
		*/

		// BFS
		vector<int> levels (numNodes); // True costs
		vector<int> costs (numNodes); // For Display
		for (int i = 0; i < levels.size(); i++){
			levels[i] = -1;
		}
		int start_node = numCols*r + c;

		vector<int> queue;
		levels[start_node] = 0;
		queue.push_back(start_node);

		while (queue.size() != 0){
			int v = queue[0]; // Access earliest (first) node
			queue.erase(queue.begin()); // Remove earliest (first) node
			costs[v] = levels[v] % 10; // Cost mod 10 (for display)
			for (int u: nodes[v]){ // For each of v's neighbors u
				if (levels[u] == (-1)){
					levels[u] = levels[v] + 1;
					queue.push_back(u);
				}
			}
		}

		for (int i = 0; i < numNodes; i++){
			int nodeRow = i / numCols;
			int nodeCol = i % numCols;
			char costNode = '0' + costs[i];
			maze[nodeRow + (nodeRow+1)][nodeCol + (nodeCol + 1)] = costNode;
		}

		for (string line: maze){
			cout << line;
		}


    } else { // is TypeTwoMaze
    	int numNodes = numRows * numCols;
    	vector<vector<pair<int, int>>> nodes(numNodes);

    	// First process horizontal neighbors:
    	for (int row = 1; row < maze.size(); row+=2){
    		for (int pos = 2; pos < lineLength; pos+=2){
    			if (maze[row][pos] != '|'){
    				int neighbor1, neighbor2;
    				int row_of_neighbors = row / 2;
    				neighbor1 = (row_of_neighbors)*numCols + ((pos / 2)-1); // Left node to that wall
    				neighbor2 = (row_of_neighbors)*numCols + (pos / 2); // Right node to that wall
    				int cost = maze[row][pos] - '0';
    				nodes[neighbor1].push_back(make_pair(neighbor2, cost));
    				nodes[neighbor2].push_back(make_pair(neighbor1, cost));
    			}
    		}
    	} 

    	// Next, process vertical neighbors:
    	for (int row = 0; row < maze.size()-1; row += 2){
    		for (int pos = 1; pos < lineLength-1; pos+=2){
    			if (maze[row][pos] != '-'){
    				int neighbor1, neighbor2;
    				int col_of_neighbors = pos / 2;
    				neighbor1 = ((row / 2)-1)*numCols + col_of_neighbors;
    				neighbor2 = (row / 2)*numCols + col_of_neighbors;
    				int cost = maze[row][pos] - '0';
    				nodes[neighbor1].push_back(make_pair(neighbor2, cost));
    				nodes[neighbor2].push_back(make_pair(neighbor1, cost));

    			}
    		}
    	}

    	/*
    	// Adjacency List
    	for (int i = 0; i < nodes.size(); i++){
			cout << "neighbors of node " << i << ": ";
			for (pair<int, int> element: nodes[i]){
				cout << element.first << " with cost " << element.second << "; ";
			}
			cout << "\n";
		}
		*/

    	// Additional Vectors for Dijkstra
    	vector<int> distance(numNodes);
		vector<bool> in_solution(numNodes);
		vector<int> preds(numNodes);

		// Initialize the vectors
		for (int i = 0; i < numNodes; i++){
			distance[i] = 999999999;
			in_solution[i] = false;
			preds[i] = -1;
		}

		// Dijkstra initialization 
		int start_node = numCols*r + c;
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
		for (int i = 0; i < numNodes; i++){
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

		// Create costs vector (distance mod 10 for display)
		vector<int> costs (numNodes);
		for (int i = 0; i < numNodes; i++){
			costs[i] = distance[i] % 10;
		}

		for (int i = 0; i < numNodes; i++){
			int nodeRow = i / numCols;
			int nodeCol = i % numCols;
			char costNode = '0' + costs[i];
			maze[nodeRow + (nodeRow+1)][nodeCol + (nodeCol + 1)] = costNode;
		}

		for (string line: maze){
			cout << line;
		}


    }

	return 0;


	}