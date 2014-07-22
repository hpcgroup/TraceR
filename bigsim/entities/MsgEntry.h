#ifndef MSGENTRY_H_
#define MSGENTRY_H_

#include "../events/Event.h"
#include <climits>

class MsgID {
  public:
    MsgID() : pe(INT_MIN), id(0), size(0) {}
    MsgID(int size_) : pe(INT_MIN), id(0), size(size_) {}
    MsgID(int size_, int pe_, int id_) : pe(pe_), id(id_), size(size_) {}
    int pe;	// PE that sent it
    int id; // emulating PE increments with each msg it sends
    int size;
};

class MsgEntry {
  public:
    MsgEntry();
    void sendMsg(unsigned long long startTime);
    int node;	// node number in global order
    int thread;	// destination thread in node
    unsigned long long sendOffset;
    MsgID msgId;
};

#endif /* MSGENTRY_H_ */
