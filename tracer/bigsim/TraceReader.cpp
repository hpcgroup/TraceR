//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// Written by:
//     Nikhil Jain <nikhil.jain@acm.org>
//     Bilge Acun <acun2@illinois.edu>
//     Abhinav Bhatele <bhatele@llnl.gov>
//
// LLNL-CODE-681378. All rights reserved.
//
// This file is part of TraceR. For details, see:
// https://github.com/LLNL/tracer
// Please also read the LICENSE file for our notice and the LGPL.
//////////////////////////////////////////////////////////////////////////////

#include "TraceReader.h"
#include <cstdio>
//#include <iostream>
//#include <fstream>

#include "blue.h"
#include "blue_impl.h"
#include "datatypes.h"
#include <cmath>

extern double soft_delay_mpi;
extern int size_replace_by;
extern int size_replace_limit;
extern double time_replace_by;
extern double time_replace_limit;

// global variables of bigsim
extern char* traceFileName;
extern JobInf *jobs;
#include <map>
#include <string>

std::map<std::string, double> eventSubs;

void addEventSub(char *key, double val) {
  std::string skey(key);
  eventSubs[key] = (double)TIME_MULT * val;
}

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

  pe->msgDestLogs = new std::map<int, int>[numEmPes];
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
  pe->taskStatus= new bool*[jobs[jobnum].numIters];
  pe->msgStatus= new bool*[jobs[jobnum].numIters];
  pe->allMarked= new bool[jobs[jobnum].numIters];
  for(int i = 0; i < jobs[jobnum].numIters; i++) {
    pe->taskStatus[i] = new bool[tlinerec.length()];
    pe->msgStatus[i] = new bool[tlinerec.length()];
    pe->allMarked[i] = false;
  }
  pe->tasksCount = tlinerec.length();
  pe->totalTasksCount = tlinerec.length();
  pe->firstTask = -1;

  if(jobs[jobnum].skipMsgId == -2) {
     pe->firstTask = 0;
  }

  *startTime = 0;

  for(int logInd=0; logInd<tlinerec.length(); logInd++)
  {
    BgTimeLog *bglog=tlinerec[logInd];

    if(pe->firstTask == -1) {
      if(bglog->msgId.pe() == 0 && bglog->msgId.msgID() == jobs[jobnum].skipMsgId) {
        pe->firstTask = logInd;
      } else {
        for(int i = 0; i < jobs[jobnum].numIters; i++) {
          pe->taskStatus[i][logInd] = true;
        }
        continue;
      }
    }

    if(logInd < pe->firstTask) {
      assert(0);
    }

    // first job's index is zero
    setTaskFromLog(&(pe->myTasks[logInd]), bglog, penum, pe->myEmPE, 0, pe, logInd);

    int sPe = bglog->msgId.pe();
    int smsgID = bglog->msgId.msgID();
    if(sPe >= 0) {
        std::map<int, int>::iterator it;
        it = pe->msgDestLogs[(sPe/numWth)%numEmPes].find(smsgID); 
        // some task set it before so it is a broadcast
        if (it == pe->msgDestLogs[(sPe/numWth)%numEmPes].end()){
            pe->msgDestLogs[(sPe/numWth)%numEmPes].insert(std::pair<int,int>(smsgID, logInd + firstLog));
        } else{
            // it may be a broadcast
            printf(" %d I should never come here, please fix me %d\n", penum, it->second);
            assert(0);
            it->second = -100;
        }
    }
    if(logInd == pe->firstTask) {
      for(int i = 0; i < jobs[jobnum].numIters; i++) {
        pe->msgStatus[i][logInd] = true;
      }
    }
  }
  firstLog += tlinerec.length();
}

void TraceReader::setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, int myEmPE, int jobPEindex, PE* pe, int logInd)
{
  if(time_replace_limit != -1 && bglog->execTime >= time_replace_limit) {
    t->execTime = (double)TIME_MULT * time_replace_by;
  } else {
    t->execTime = (double)TIME_MULT * bglog->execTime;
  }

  if( strcmp(bglog->name, "AMPI_generic") == 0 ||
    strcmp(bglog->name, "AMPI_SEND_END") == 0 ||
    strcmp(bglog->name, "msgep") == 0 ||
    strcmp(bglog->name, "GroupReduce") == 0 ||
    strcmp(bglog->name, "RECV_RESUME") == 0 ||
    strcmp(bglog->name, "start-broadcast") == 0 ||
    strcmp(bglog->name, "split-broadcast") == 0 ||
    strcmp(bglog->name, "end-broadcast") == 0 ||
    strcmp(bglog->name, "AMPI_WAITALL") == 0) {
    t->execTime = 0.0;
  }

  if(strcmp(bglog->name, "AMPI_Irecv") == 0 ||
    strcmp(bglog->name, "AMPI_SEND") == 0 ||
    strcmp(bglog->name, "AMPI_Allreduce") == 0 ||
    strcmp(bglog->name, "AMPI_Recv") == 0 ||
    strcmp(bglog->name, "AMPI_Sendrecv") == 0 ||
    strcmp(bglog->name, "AMPI_Waitall") == 0 ||
    strcmp(bglog->name, "AMPI_Barrier") == 0 ||
    strcmp(bglog->name, "AMPI_Wait") == 0) {
    t->execTime = soft_delay_mpi;
  }

  if(t->execTime < 0) {
     t->execTime = 0;
  }
  
  if(strcmp(bglog->name, "AMPI_BgSetEndEvent") == 0) {
    t->endEvent = true;
    t->execTime = 0;
  } else {
    t->endEvent = false;
  }

  if(strcmp(bglog->name, "AMPI_BgLoopToStart") == 0) {
    t->loopEvent = true;
    t->execTime = 0;
  } else {
    t->loopEvent = false;
  }

  //depends on message or not
  if(bglog->msgId.pe() < 0) {
    for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
      pe->msgStatus[i][logInd] = true;
      pe->taskStatus[i][logInd] = false;
    }
  } else {
    for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
      pe->msgStatus[i][logInd] = false;
      pe->taskStatus[i][logInd] = false;
    }
  }

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
    //t->myEntries[i].sendOffset = (double)TIME_MULT * (bglog->msgs[i]->sendTime - bglog->startTime);
    t->myEntries[i].sendOffset = 0;
    if(size_replace_limit != -1 && bglog->msgs[i]->msgsize >= size_replace_limit) {
      t->myEntries[i].msgId.size = size_replace_by;
    } else {
      t->myEntries[i].msgId.size = bglog->msgs[i]->msgsize;
    }
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
    } else {
      std::map<std::string, double>::iterator loc =
        eventSubs.find(std::string((char *)bglog->evts[i]->data));
      if(loc != eventSubs.end()) {
        t->execTime = loc->second;
      }
    }
  }
}
