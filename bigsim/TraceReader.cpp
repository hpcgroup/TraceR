/*
 * TraceFileReader.cpp
 *
 *  Created on: Oct 18, 2010
 *  Author: Ehsan
 *  Heavily modified by Nikhil Jain and Bilge Acun
 */

#include "TraceReader.h"
#include <cstdio>

#include "blue.h"
#include "blue_impl.h"
#include <cmath>

// global variables of bigsim
extern BgTimeLineRec* currTline;
extern int currTlineIdx;

TraceReader::TraceReader() {
  totalTlineLength=0;
}

//void TraceReader::readTrace(int &tot, int& totn, int& emPes, int& nwth, PE* pe, int penum, unsigned long long& startTime/*, int**& msgDestLogs*/)
void TraceReader::readTrace(int* tot, int* totn, int* emPes, int* nwth, PE* pe, int penum, unsigned long long* startTime, int** msgDestLogs)
{
  int numX, numY, numZ, numCth;
  BgLoadTraceSummary("bgTrace", totalWorkerProcs, numX, numY, numZ, numCth, numWth, numEmPes);
  printf("totalRanks:%d, numEmPes:%d\n",totalWorkerProcs, numWth,numEmPes);
  totalNodes= totalWorkerProcs/numWth;
  allNodeOffsets = BgLoadOffsets(totalWorkerProcs,numEmPes);

  *nwth = numWth;
  *tot = totalWorkerProcs;
  *totn= totalNodes;
  *emPes = numEmPes;

  fileLoc=0;
  firstLog=0;
  totalTlineLength=0;

  /* create message map arrays for each emulating PE*/
  int emPeMaxMsgs = 0;
  
  int nodeNum = penum/numWth;
  int myEmulPe = nodeNum%numEmPes;

  BgTimeLineRec tlinerec; // Time line (list of logs)
  currTline = &tlinerec;  // set global variable
  currTlineIdx = penum;               // set global variable
  int status = BgReadProc( penum, numWth , numEmPes, totalWorkerProcs, allNodeOffsets, tlinerec);
  assert(status!=-1);

 // find number of messages per PE
  for(int j=tlinerec.length()-1; j>=0; j--)
  {
    BgTimeLog *bglog=tlinerec[j];
    if(bglog->msgs.length()>0)
    {
      if(bglog->msgs[bglog->msgs.length()-1]->msgID > emPeMaxMsgs)
      {
        emPeMaxMsgs = bglog->msgs[bglog->msgs.length()-1]->msgID;
      }
      break;
    }
  }
  *msgDestLogs = new int[emPeMaxMsgs+1];
  for(int j=0; j<emPeMaxMsgs+1; j++)
    *msgDestLogs[j] = -1;

  // update fileLoc
  // call to update fileLoc
  status = BgReadProcWindow( penum, numWth , numEmPes, totalWorkerProcs, allNodeOffsets, tlinerec, fileLoc, totalTlineLength, 0, firstLog);
  assert(status!=-1);
  
   // read tasks
  // read the window
  status = BgReadProcWindow( penum, numWth , numEmPes, totalWorkerProcs, allNodeOffsets, tlinerec, fileLoc, totalTlineLength, firstLog, totalTlineLength);
  assert(status!=-1);
  pe->myNum = penum;
  pe->myEmPE = (penum/numWth)%numEmPes;
  pe->myTasks= new Task[tlinerec.length()];
  pe->tasksCount = tlinerec.length();
  pe->totalTasksCount = totalTlineLength;

  for(int logInd=0; logInd<tlinerec.length(); logInd++)
  {
    BgTimeLog *bglog=tlinerec[logInd];
    if(penum==0 && logInd==0)
    {
      *startTime = (unsigned long long)(((double)TIME_MULT) * bglog->startTime);
    }

    // first job's index is zero
    setTaskFromLog(&(pe->myTasks[logInd]), bglog, penum, 0, msgDestLogs);

    int sPe = bglog->msgId.pe();
    int smsgID = bglog->msgId.msgID();
    if(sPe >= 0) {
      // some task set it before so it is a broadcast
      if( msgDestLogs[(sPe/numWth)%numEmPes][smsgID]==-1){
        msgDestLogs[(sPe/numWth)%numEmPes][smsgID] = logInd + firstLog;
          } else // it may be a broadcast
        msgDestLogs[(sPe/numWth)%numEmPes][smsgID] = -100;
    }
  }
  firstLog += tlinerec.length();

}

void TraceReader::setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, int jobPEindex, int** msgDestLogs)
{
  t->execTime=(unsigned long long)(((double)TIME_MULT * bglog->execTime));
  t->myMsgId.pe = bglog->msgId.pe() + jobPEindex;
  if(t->myMsgId.pe < 0)
    t->myMsgId.pe = -1;
  else
    t->myMsgId.pe++; //can't use 0 since a completed receive is indicated by negative value.

  t->myMsgId.id = bglog->msgId.msgID();
  t->charmEP = bglog->charm_ep;
  t->msgEntCount = bglog->msgs.length();
  t->myEntries = new MsgEntry[t->msgEntCount];

  for(int i=0; i<bglog->msgs.length(); i++)
  {
    t->myEntries[i].msgId.id = bglog->msgs[i]->msgID;
    t->myEntries[i].msgId.pe = taskPE;

    t->myEntries[i].node = bglog->msgs[i]->dstNode;
    t->myEntries[i].thread = bglog->msgs[i]->tID;

    // mark broadcast
    if(bglog->msgs[i]->dstNode < 0 || bglog->msgs[i]->tID < 0)
    {
      *msgDestLogs[bglog->msgs[i]->msgID] = -100;
      //Engine::engine.msgDestTask[Engine::engine.pes[taskPE].myEmPE][bglog->msgs[i]->msgID] = -100;
    }

    // sendTime is absolute
    t->myEntries[i].sendOffset = (long long)(((double)TIME_MULT * (bglog->msgs[i]->sendTime - bglog->startTime)));
    assert(t->myEntries[i].sendOffset >= 0);
    t->myEntries[i].msgId.size = bglog->msgs[i]->msgsize;
  }

  t->backwDepSize = bglog->backwardDeps.length();
  t->backwardDep=new int[t->backwDepSize];
  for(int i=0; i< t->backwDepSize; i++)
  {
    t->backwardDep[i]= abs(bglog->backwardDeps[i]->seqno);
  }

  t->forwDepSize=bglog->forwardDeps.length();
  t->forwardDep=new int[t->forwDepSize];
  for(int i=0; i<t->forwDepSize; i++){
    t->forwardDep[i]= abs(bglog->forwardDeps[i]->seqno);
  }

  t->bgPrintCount=0;
  for(int i=0; i< bglog->evts.length();i++){
    if (bglog->evts[i]->eType == BG_EVENT_PRINT)  t->bgPrintCount++;
  }
  if (t->bgPrintCount)  t->myBgPrints = new BgPrint[t->bgPrintCount];
  if(t->bgPrintCount > 200)
    printf("AAA:%d\n",t->bgPrintCount);
  int pInd=0;
  for(int i=0; i< bglog->evts.length(); i++){
    if (bglog->evts[i]->eType == BG_EVENT_PRINT) {
      t->myBgPrints[pInd].msg = new char[strlen((char *)bglog->evts[i]->data)+1];
      strcpy(t->myBgPrints[pInd].msg, (char *)bglog->evts[i]->data);
      t->myBgPrints[pInd].time = (bglog->evts[i]->rTime);
      strcpy(t->myBgPrints[pInd].taskName, bglog->name);
      pInd++;
    }
  }
}
