/*
 * ExecCompleteEvent.cpp
 *
 *  Created on: Oct 23, 2010
 *      Author: ehsan
 */

#include "ExecCompleteEvent.h"

ExecCompleteEvent::ExecCompleteEvent(unsigned long long _time, int _pe) : Event(_time)
{
	pe = _pe;
}

void ExecCompleteEvent::eventFunction()
{
	//Engine::engine.pes[pe].completeExec(time);
}

Event* ExecCompleteEvent::clone()
{
	return (Event*) new ExecCompleteEvent(time, pe);
}

void ExecCompleteEvent::print()
{
	printf("(comp pe:%d time:%llu)\t ", pe, time);
}
