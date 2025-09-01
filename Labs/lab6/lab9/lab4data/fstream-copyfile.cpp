/*
 * g++ -g -Wall -std=c++11 -o fstream-copyfile fstream-copyfile.cpp
 * usage: fstream-copyfile infile outfile
 *        diff infile outfile
 */

/* C++ standard include files first */
#include <iostream>
#include <fstream>

using namespace std;

/* C system include files next */
/* C standard include files next */
/* your own include last - none for this program */

/**
 * Open in_filename_string for reading.
 *
 * @param infile - reference to an opened ifstream if file can be opened for reading.
 * @param in_filename_string - file name to open for reading.
 * @return (-1) if file cannot be opened; otherwise, return 0 to indicate success.
 */
int open_file_for_reading(ifstream& infile, string in_filename_string)
{
    infile.open(in_filename_string, ifstream::in|ios::binary);
    if (infile.fail()) {
        cout << "Cannot open '" << in_filename_string << "' for reading." << endl;
        return (-1);
    }
    return 0;
}

/**
 * Open out_filename_string for writing (create the file if it doesn't already exist).
 *
 * @param outfile - reference to an opened ofstream if file can be opened for writing.
 * @param out_filename_string - file name to open for writing.
 * @return (-1) if file cannot be opened; otherwise, return 0.
 */
int open_file_for_writing(ofstream& outfile, string out_filename_string)
{
    outfile.open(out_filename_string, ofstream::out|ios::binary);
    if (outfile.fail()) {
        cout << "Cannot open '" << out_filename_string << "' for writing." << endl;
        return (-1);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        cerr << "usage: fstream-copyfile infile outfile" << endl;
        exit(-1);
    }
    ifstream infile;
    ofstream outfile;
    string in_filename(argv[1]);
    string out_filename(argv[2]);
    if (open_file_for_reading(infile, in_filename) == (-1)) {
        cout << "Cannot open '" << in_filename << "' for reading." << endl;
        exit(-1);
    }
    if (open_file_for_writing(outfile, out_filename) == (-1)) {
        cout << "Cannot open '" << out_filename << "' for writing." << endl;
        exit(-1);
    }
    /* C++ is weird */
    infile.seekg(0, infile.end);
    int file_size = infile.tellg();
    infile.seekg(0, infile.beg);

    unsigned int bytes_left = file_size;
    while (bytes_left > 0) {
        char buf[1024];
        int bytes_to_read = (bytes_left > sizeof(buf)) ? sizeof(buf) : bytes_left;
        infile.read(buf, bytes_to_read);
        if (infile.fail()) {
            break;
        }
        outfile.write(buf, bytes_to_read);
        if (outfile.fail()) {
            cout << "Cannot write " << bytes_to_read << " bytes into '" << out_filename << "'." << endl;
            exit(-1);
        }
        bytes_left -= bytes_to_read;
    }
    infile.close();
    outfile.close();
    cout << file_size << " bytes written into '" << out_filename << "'." << endl;
    return 0;
}
