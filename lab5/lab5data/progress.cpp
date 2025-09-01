/*
 * g++ -g -Wall -std=c++11 progress.cpp
 * ./a.out
 */

/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

/* C system include files next */
#include <sys/time.h>

/* C standard include files next */
#include <string.h>
#include <unistd.h>

/* your own include last - none for this program */
#include "my_timestamp.h"

int main(int argc, char *argv[])
{
    cout << "Progress bar:" << endl;
    for (int i=0; i < 100; i++) {
        usleep(100000);
        cout << "\r[" << get_timestamp_now() << "] " << i+1;
        cout.flush();
    }
    cout << endl;
    cout << endl << "Wait for 0.1 seconds to pass 20 times:" << endl;
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    srandom((unsigned int)(start_time.tv_sec));
    for (int i=0; i < 20; i++) {
        for (;;) {
            /* sleep any where between 0 and 0.05 seconds */
            usleep(random() % 50000);
            struct timeval now;
            gettimeofday(&now, NULL);
            double elapsed_time = timestamp_diff_in_seconds(&start_time, &now);
            if (elapsed_time >= ((double)0.1)) {
                cout << "[" << format_timestamp(&now) << "]" << endl;
                start_time.tv_sec = now.tv_sec;
                start_time.tv_usec = now.tv_usec;
                break;
            }
        }
    }
    return 0;
}
