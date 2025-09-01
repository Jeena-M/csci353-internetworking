/*
 * g++ -g -Wall -std=c++11 -DLOCALHOST=\"localhost\" -o copyfile copyfile.cpp
 * usage: copyfile infile outfile
 *        diff infile outfile
 */

/* C++ standard include files first */
#include <iostream>

using namespace std;

/* C system include files next */
#include <fcntl.h>
#include <unistd.h>

/* C standard include files next */
/* your own include last - none for this program */

/**
 * Open in_filename_string for reading.
 *
 * @param in_filename_string - file name to open for reading.
 * @return (-1) if file cannot be opened; otherwise, return a file descriptor.
 */
int open_file_for_reading(string in_filename_string)
{
    return open(in_filename_string.c_str(), O_RDONLY);
}

/**
 * Open out_filename_string for writing (create the file if it doesn't already exist).
 *
 * @param out_filename_string - file name to open for writing.
 * @return (-1) if file cannot be opened; otherwise, return a file descriptor.
 */
int open_file_for_writing(string out_filename_string)
{
    int fd = open_file_for_reading(out_filename_string);
    if (fd == (-1)) {
        fd = open(out_filename_string.c_str(), O_WRONLY|O_CREAT, 0600);
    } else {
        close(fd);
        fd = open(out_filename_string.c_str(), O_WRONLY|O_TRUNC);
    }   
    return fd;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        cerr << "usage: copyfile infile outfile" << endl;
        exit(-1);
    }
    string in_filename(argv[1]);
    string out_filename(argv[2]);
    int in_fd = open_file_for_reading(in_filename);
    if (in_fd == (-1)) {
        cout << "Cannot open '" << in_filename << "' for reading." << endl;
        exit(-1);
    }
    int out_fd = open_file_for_writing(out_filename);
    if (out_fd == (-1)) {
        cout << "Cannot open '" << out_filename << "' for writing." << endl;
        exit(-1);
    }
    int total = 0;
    for (;;) {
        char buf[1024];
        int bytes_read = read(in_fd, buf, sizeof(buf));
        if (bytes_read <= 0) {
            break;
        }
        if (write(out_fd, buf, bytes_read) != bytes_read) {
            cout << "Cannot write " << bytes_read << " bytes into '" << out_filename << "'." << endl;
            exit(-1);
        }
        total += bytes_read;
    }
    close(in_fd);
    close(out_fd);
    cout << total << " bytes written into '" << out_filename << "'." << endl;
    return 0;
}
