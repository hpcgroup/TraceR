/*
 * ExecCompleteEvent.h
 *
 *  Created on: Oct 23, 2010
 *      Author: ehsan
 */

#ifndef EXECCOMPLETEEVENT_H_
#define EXECCOMPLETEEVENT_H_
#include "Event.h"
#include "../entities/PE.h"

class ExecCompleteEvent :public Event {
public:
	ExecCompleteEvent(unsigned long long time, int pe);
	void eventFunction();
	Event* clone();
	void print();
	bool isForPE(int peNum){return (peNum == pe);}
	int pe;
};

#endif /* EXECCOMPLETEEVENT_H_ */
