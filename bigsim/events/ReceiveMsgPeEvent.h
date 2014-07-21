/*
 * RecieveMsgPeEvent.h
 *
 *  Created on: Oct 20, 2010
 *      Author: ehsan
 */

#ifndef RECEIVEMSGPEEVENT_H_
#define RECEIVEMSGPEEVENT_H_
#include "Event.h"
#include "../entities/MsgEntry.h"

class ReceiveMsgPeEvent : public Event{
  public:
    ReceiveMsgPeEvent(unsigned long long time, int dstPE, MsgID msgId);
    void eventFunction();
    Event* clone();
    void print();
    bool isForPE(int peNum){return (peNum == dstPE);}
    int dstPE;
    MsgID msgId;
};

#endif /* RECEIVEMSGPEEVENT_H_ */
