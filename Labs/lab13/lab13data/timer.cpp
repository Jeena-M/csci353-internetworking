/* g++ -g -Wall -std=c++11 timer.cpp -pthread */

/* C++ standard include files first */
#include <iostream>
#include <thread>

using namespace std;

/* C system include files next */
/* C standard include files next */
#include <unistd.h>

/* your own include last */

class Timer {
public:
    int expiration_time; // in seconds
    int ticks_remaining;
    bool cancelled;
    bool expired;
    shared_ptr<thread> thread_ptr;
    Timer(int s) {
        expiration_time = s;
        ticks_remaining = s * 10; // 10 timer ticks per second
        cancelled = expired = false;
        thread_ptr = nullptr;
    }
    void start() { thread_ptr = make_shared<thread>(thread(timer_proc, this)); }
    void stop() { cancelled = true; }
    static void timer_proc(Timer *t) {
        while (t->ticks_remaining-- > 0) {
            cout << "."; cout.flush();
            usleep(100000);
            if (t->cancelled) {
                cout << " (cancelled)" << endl;
                return;
            }
        }
        t->expired = true;
        cout << endl;
    }
};

int main()
{
    Timer t1(4);
    cout << "Timer 1, expire in 4 seconds:    ";
    t1.start();
    t1.thread_ptr->join();

    Timer t2(4);
    cout << "Timer 2, cancelled in 2+ seconds: ";
    t2.start();
    usleep(2500000);
    t2.stop();
    t2.thread_ptr->join();
}
