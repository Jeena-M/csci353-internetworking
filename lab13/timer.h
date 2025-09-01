#ifndef _TIMER_H_
#define _TIMER_H_

/* C++ standard include files first */
#include <iostream>
#include <thread>
#include "timercallback.h"

using namespace std;

/* C system include files next */
/* C standard include files next */
#include <unistd.h>


class Timer {
public:
    int expiration_time; // in seconds
    int ticks_remaining;
    bool cancelled;
    bool expired;
    shared_ptr<thread> thread_ptr;
    shared_ptr<TimerCallback> callback;  
    string sesid;

    Timer(int s, shared_ptr<TimerCallback> cb, string id) {
        expiration_time = s;
        ticks_remaining = s * 10; // 10 timer ticks per second
        cancelled = expired = false;
        thread_ptr = nullptr;
        callback = cb;
        sesid = id;
    }
    void start() { thread_ptr = make_shared<thread>(thread(timer_proc, this)); }
    void stop() { cancelled = true; }
    static void timer_proc(Timer *t) {
        while (t->ticks_remaining-- > 0) {
            cout << "."; cout.flush();
            usleep(100000);
            if (t->cancelled) {
                //cout << " (cancelled)" << endl;
                return;
            }
        }
        t->expired = true;
        cout << endl;



        Event ev;
        ev.sesid = t->sesid;
        ev.type = EVENT_TIMEOUT;

        t->callback->add_work(ev);

    }
};


#endif