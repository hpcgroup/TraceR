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
#include "datatypes.h"
#include <cmath>

// global variables of bigsim
extern char* traceFileName;
extern JobInf *jobs;

TraceReader::TraceReader(char *s) {
  strncpy(tracePath, s, strlen(s) + 1);
  totalTlineLength=0;
}

TraceReader::~TraceReader() {
   delete [] allNodeOffsets;
}

void TraceReader::loadTraceSummary(){
  int numX, numY, numZ, numCth;
  BgLoadTraceSummary(tracePath, totalWorkerProcs, numX, numY, numZ, numCth,
      numWth, numEmPes);
  totalNodes = totalWorkerProcs/numWth;
}

void TraceReader::loadOffsets(){
  totalNodes= totalWorkerProcs/numWth;
  traceFileName = tracePath;
  allNodeOffsets = BgLoadOffsets(totalWorkerProcs,numEmPes);
}

void TraceReader::readTrace(int* tot, int* totn, int* emPes, int* nwth, PE* pe,
    int penum, int jobnum, double* startTime)
{
  *nwth = numWth;
  *tot = totalWorkerProcs;
  *totn= totalNodes;
  *emPes = numEmPes;

  fileLoc=0;
  firstLog=0;
  totalTlineLength=0;

  int nodeNum = penum/numWth;
  int myEmulPe = nodeNum%numEmPes;

  traceFileName = tracePath;

  pe->msgDestLogs = new map<int, int>[numEmPes];
  pe->numWth = numWth;
  pe->numEmPes = numEmPes;

  if(jobs[jobnum].skipMsgId == -1) {
    BgTimeLineRec tlinerec2;
    BgReadProc( 0, numWth , numEmPes, totalWorkerProcs, allNodeOffsets, tlinerec2);
    for(int j = 0; j < tlinerec2.length(); j++) {
      BgTimeLog *bglog = tlinerec2[j];
      if(bglog->isStartEvent()) {
        jobs[jobnum].skipMsgId = bglog->msgs[0]->msgID;
        break;
      }
    }
    if(jobs[jobnum].skipMsgId == -1) {
      jobs[jobnum].skipMsgId = -2;
    }
  }

  BgTimeLineRec tlinerec; // Time line (list of logs)
  int status = BgReadProc( penum, numWth , numEmPes, totalWorkerProcs,
      allNodeOffsets, tlinerec);
  assert(status!=-1);
  pe->myNum = penum;
  pe->jobNum = jobnum;
  pe->myEmPE = (penum/numWth)%numEmPes;
  pe->myTasks= new Task[tlinerec.length()];
  pe->tasksCount = tlinerec.length();
  pe->totalTasksCount = tlinerec.length();
  pe->firstTask = -1;

  if(jobs[jobnum].skipMsgId == -2) {
     pe->firstTask = 0;
  }

  //int status = BgReadProcWindow( penum, numWth , numEmPes, totalWorkerProcs,
  //            allNodeOffsets, tlinerec, fileLoc, totalTlineLength, firstLog,
  //            totalTlineLength);

  *startTime = 0;

  for(int logInd=0; logInd<tlinerec.length(); logInd++)
  {
    BgTimeLog *bglog=tlinerec[logInd];

    if(pe->firstTask == -1) {
      if(bglog->msgId.pe() == 0 && bglog->msgId.msgID() == jobs[jobnum].skipMsgId) {
        pe->firstTask = logInd;
      } else {
        pe->myTasks[logInd].done = true;
        continue;
      }
    }

    if(logInd < pe->firstTask) {
      return;
    }

    // first job's index is zero
    setTaskFromLog(&(pe->myTasks[logInd]), bglog, penum, pe->myEmPE, 0, pe);

    int sPe = bglog->msgId.pe();
    int smsgID = bglog->msgId.msgID();
    if(sPe >= 0) {
        map<int, int>::iterator it;
        it = pe->msgDestLogs[(sPe/numWth)%numEmPes].find(smsgID); 
        // some task set it before so it is a broadcast
        if (it == pe->msgDestLogs[(sPe/numWth)%numEmPes].end()){
            pe->msgDestLogs[(sPe/numWth)%numEmPes].insert(pair<int,int>(smsgID, logInd + firstLog));
        } else{
            // it may be a broadcast
            printf(" %d I should never come here, please fix me %d\n", penum, it->second);
            assert(0);
            it->second = -100;
        }
    }
    if(logInd == pe->firstTask) {
      pe->myTasks[logInd].myMsgId.pe = -1;
    }
  }
  firstLog += tlinerec.length();
}

void TraceReader::setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, int myEmPE, int jobPEindex, PE* pe)
{
  t->execTime = (double)TIME_MULT * bglog->execTime;
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

    // sendTime is absolute
    t->myEntries[i].sendOffset = (double)TIME_MULT * (bglog->msgs[i]->sendTime - bglog->startTime);
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
