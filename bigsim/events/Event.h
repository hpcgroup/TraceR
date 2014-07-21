#ifndef _EVENT_
#define _EVENT_

#include <cstdlib>
#include <cstdio>

class Event{
public:
	unsigned long long time;
	Event(unsigned long long t) : time(t) {}
	virtual void eventFunction(){}
	virtual Event* clone(){ return NULL; }
	virtual void print(){}
	virtual bool isForPE(int peNum){ return false; }
};

class NullEvent: public Event{
public:
	NullEvent(unsigned long long t):Event(t){}
	void eventFunction(){} // do nothing
	void print(){}
	bool isForPE(int p){return false;}
};
#endif
