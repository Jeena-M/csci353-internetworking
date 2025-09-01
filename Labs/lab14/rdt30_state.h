#ifndef _RDT30_STATE_H_
#define _RDT30_STATE_H_

using namespace std;


class RDT30_State {
    public:
        string peer_nodeid; /* who you are running RDT-3.0 with */
        int seq_no; /* 0 or 1 */
        int app_no; /* 0 for now */
        string sndpkt; /* return from make_pkt() */
        string msg_received; /* received from peer so far */
    };

#endif