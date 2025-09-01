/*
 * g++ -g -Wall -std=c++11 md5-calc.cpp -lcrypto
 * ./a.out FILE
 * openssl md5 FILE
 */

/* C++ standard include files first */
#include <iostream>
#include <fstream>

using namespace std;

/* C system include files next */
#include <sys/stat.h>
#include <openssl/md5.h>

/* C standard include files next - none for this program*/
/* your own include last - none for this program */

static string HexDump(unsigned char *buf, int len);
static void Usage();
static int get_file_size(string path);

int main(int argc, char *argv[])
{
    if (argc != 2) Usage();

    ifstream myfile;

    myfile.open(argv[1], ifstream::in|ios::binary);
    if (myfile.fail()) {
        cerr << "Cannot open '" << argv[1] << "' for reading." << endl;
        exit(-1);
    }
    int bytes_remaining = get_file_size(argv[1]);
    MD5_CTX md5_ctx;

    MD5_Init(&md5_ctx);
    while (bytes_remaining > 0) {
        char buf[0x1000]; /* 4KB buffer */

        int bytes_to_read = ((bytes_remaining > (int)sizeof(buf)) ? sizeof(buf) : bytes_remaining);
        myfile.read(buf, bytes_to_read);
        if (myfile.fail()) {
            break;
        }
        MD5_Update(&md5_ctx, buf, bytes_to_read);
        bytes_remaining -= bytes_to_read;
    }
    myfile.close();
    unsigned char md5_buf[MD5_DIGEST_LENGTH];

    MD5_Final(md5_buf, &md5_ctx);

    string md5 = HexDump(md5_buf, sizeof(md5_buf));
    cout << md5 << endl;

    return 0;
}

static
string HexDump(unsigned char *buf, int len)
{
    string s;
    static char hexchar[]="0123456789abcdef";

    for (int i=0; i < len; i++) {
        unsigned char ch=buf[i];
        unsigned int hi_nibble=(unsigned int)((ch>>4)&0x0f);
        unsigned int lo_nibble=(unsigned int)(ch&0x0f);

        s += hexchar[hi_nibble];
        s += hexchar[lo_nibble];
    }
    return s;
}

static
void Usage()
{
    cerr << "usage: md5-calc filename" << endl;
    exit(1);
}

/**
 * Use this code to return the file size of path.
 *
 * You should be able to use this function as it.
 *
 * @param path - a file system path.
 * @return the file size of path, or (-1) if failure.
 */
static
int get_file_size(string path)
{
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        return (-1);
    }
    return (int)(stat_buf.st_size);
}
