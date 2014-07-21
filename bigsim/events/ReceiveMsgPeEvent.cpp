/*
 * RecieveMsgPeEvent.cpp
	 *
 *  Created on: Oct 20, 2010
 *      Author: ehsan
 */

#include "ReceiveMsgPeEvent.h"
#include <cstdio>

ReceiveMsgPeEvent::ReceiveMsgPeEvent(unsigned long long time, int _dstPE, MsgID _msgId) : Event(time)
{
	//assert(_dstPE >= 0 && _dstPE < Engine::engine.totalWorkPes);

	dstPE=_dstPE;
	msgId=_msgId;
}

void ReceiveMsgPeEvent::eventFunction()
{
//	Engine::engine.pes[dstPE].receiveMsg(msgId, time);
}

Event* ReceiveMsgPeEvent::clone()
{
	return (Event*) new ReceiveMsgPeEvent(time, dstPE, msgId);
}

void ReceiveMsgPeEvent::print()
{
	printf("(recMsgPE pe:%d id:%d time:%llu)\t ", dstPE, msgId.id, time);
}
