#ifndef _TIMERCALLBACK_
#define _TIMERCALLBACK_

class TimerCallback {
public:
    virtual void add_work(Event& ev) = 0;           // to be called on timer expiry
    virtual void wait_for_work() = 0;      // optional, not needed in timer
    virtual ~TimerCallback() {}
};

#endif