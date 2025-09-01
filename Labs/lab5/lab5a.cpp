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

bool printSectionNames(vector<string>& file_lines){
	bool isSections = false;
	for (string line: file_lines){
		if (line[0] == '['){
			isSections = true;
			cout << line.substr(1, line.length()-3) << endl;
		}
	}

	return isSections;
}

void printAllKeys(string section_name, vector<string>& file_lines, string INIFILE){
	bool foundSection = false;
	int index = 0;

	// Find location of section_name in file_lines
	for (string line: file_lines){
		if (line[0] == '['){
			string curr_sec_name = line.substr(1, line.length()-3);
			if (curr_sec_name == section_name){
				foundSection = true;
				break;
			}
		}
		index += 1;
	}

	// Output err if section_name not found in file_lines
	if (!foundSection){
		cout << "Cannot find the [" << section_name << "] section in " << INIFILE << endl;
		return;
	}

	// Print keys:
	int i = index + 1;
	while (i < file_lines.size() && file_lines[i][0] != '['){
		if (file_lines[i][0] != ';' && file_lines[i] != "\n"){
			size_t equals_pos = file_lines[i].find("=");
			string key = file_lines[i].substr(0, equals_pos);
			cout << key << endl;
		}
		i = i + 1;
	}

}

void printValue(string section_name, string keyname, vector<string>& file_lines, string INIFILE){
	bool foundSection = false;
	int index = 0;

	// Find location of section_name in file_lines
	for (string line: file_lines){
		if (line[0] == '['){
			string curr_sec_name = line.substr(1, line.length()-3);
			if (curr_sec_name == section_name){
				foundSection = true;
				break;
			}
		}
		index += 1;
	}

	// Output err if section_name not found in file_lines
	if (!foundSection){
		cout << "Cannot find the [" << section_name << "] section in " << INIFILE << endl;
		return;
	}

	// Find corresponding key and output value:
	int i = index + 1;
	bool foundKey = false;
	while (i < file_lines.size() && file_lines[i][0] != '['){
		if (file_lines[i][0] != ';' && file_lines[i] != "\n"){
			size_t equals_pos = file_lines[i].find("=");
			string key = file_lines[i].substr(0, equals_pos);
			if (key == keyname){
				foundKey = true;
				string value = file_lines[i].substr(equals_pos+1, file_lines[i].length() - equals_pos - 2);
				if (value.empty()){
					cout << "There is no value for the \"" << keyname << "\" key in the [" << section_name << "] section in " << INIFILE << "." << endl;
					return;
				} else {
					cout << value << endl;
					return;
				}
			}
		}
		i = i + 1;
	}

	if (!foundKey){
		cout << "Cannot find the \"" << keyname << "\" key in the [" << section_name << "] section in " << INIFILE << "." << endl;
		return;
	}

}



int main(int argc, char *argv[])
{
    // Save command line arguments:

    if (argc == 3){
    	// command name = sections
    	string INIFILE = argv[2];

    	// Read all lines of file into a single vector
		int fd = open(INIFILE.c_str(), O_RDONLY);
	
		if (fd < 0){ // If open failed, fd is negative:
			perror("open");
			exit(1);
		}

		vector<string> file_lines;

		for (;;) {
	    	string line;
	    	if (read_a_line(fd, line) <= 0) {
	        	break;
	    	}
	    	file_lines.push_back(line);
	    }

	    // Execute the "sections" command
	    bool isSections = printSectionNames(file_lines);
	    if (!isSections){
	    	cout << "No sections in " << INIFILE << "." << endl;
	    }

    } else if (argc == 4){
    	// command name = keys
    	string section_name = argv[2];
    	string INIFILE = argv[3];

    	// Read all lines of file into a single vector
		int fd = open(INIFILE.c_str(), O_RDONLY);
	
		if (fd < 0){ // If open failed, fd is negative:
			perror("open");
			exit(1);
		}

		vector<string> file_lines;

		for (;;) {
	    	string line;
	    	if (read_a_line(fd, line) <= 0) {
	        	break;
	    	}
	    	file_lines.push_back(line);
	    }

    	printAllKeys(section_name, file_lines, INIFILE);

    } else {
    	// command name = value
    	string section_name = argv[2];
    	string keyname = argv[3];
    	string INIFILE = argv[4];

    	// Read all lines of file into a single vector
		int fd = open(INIFILE.c_str(), O_RDONLY);
	
		if (fd < 0){ // If open failed, fd is negative:
			perror("open");
			exit(1);
		}

		vector<string> file_lines;

		for (;;) {
	    	string line;
	    	if (read_a_line(fd, line) <= 0) {
	        	break;
	    	}
	    	file_lines.push_back(line);
	    }

    	printValue(section_name, keyname, file_lines, INIFILE);
    }

    return 0;
}