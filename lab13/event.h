#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <sys/time.h>

enum EventType {
    EVENT_PONG,
    EVENT_TTLZERO,
    EVENT_TIMEOUT
};

struct Event {
    EventType type;
    std::string sesid;
    struct timeval timestamp;
    string from_node;
};

#endif