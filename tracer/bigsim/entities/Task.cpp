/*
 * Task.cpp
 *
 */

#include "Task.h"
#include "PE.h"

Task::Task() {
  backwDepSize = 0;
  forwDepSize = 0;
  backwardDep = 0;
  forwardDep = 0;
  myEntries = 0;
  done = false;
  execTime = -1;
}

void Task::printEvt(tw_lp * lp, unsigned long long startTime, int PEno, int jobNo)
{
  for(int i = 0; i < bgPrintCount; i++) {
    myBgPrints[i].print(lp, startTime, PEno, jobNo);
  }
}

Task::~Task()
{
  delete[] forwardDep;
  delete[] backwardDep;
  delete[] myBgPrints;
  delete[] myEntries;
}

void Task::copyFromTask(Task* t)
{
  execTime= t->execTime;
  done = t->done;
  myMsgId.pe = t->myMsgId.pe;
  myMsgId.id = t->myMsgId.id;
  charmEP = t->charmEP;
  msgEntCount=t->msgEntCount;
  myEntries=new MsgEntry[msgEntCount];

  for(int i=0; i< msgEntCount; i++)
  {
    myEntries[i].msgId.id = t->myEntries[i].msgId.id;
    myEntries[i].msgId.pe = t->myEntries[i].msgId.pe;
    myEntries[i].msgId.size = t->myEntries[i].msgId.size;
    myEntries[i].node = t->myEntries[i].node;
    myEntries[i].thread = t->myEntries[i].thread;
    // sendTime is absolute
    myEntries[i].sendOffset = t->myEntries[i].sendOffset;

  }

  backwDepSize = t->backwDepSize;
  backwardDep=new int[backwDepSize];
  for(int i=0; i< backwDepSize; i++)
  {
    backwardDep[i] = t->backwardDep[i];
  }

  forwDepSize = t->forwDepSize;
  forwardDep=new int[forwDepSize];
  for(int i=0; i<forwDepSize; i++){
    forwardDep[i] = t->forwardDep[i];
  }

  bgPrintCount = t->bgPrintCount;
  myBgPrints = new BgPrint[bgPrintCount];
  for(int i=0; i<bgPrintCount; i++) {
    myBgPrints[i].copy(&t->myBgPrints[i]);
  }
}
