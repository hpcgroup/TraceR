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

#include "datatypes.h"
#include <cmath>

extern double soft_delay_mpi;
extern int* size_replace_by;
extern int* size_replace_limit;
extern double time_replace_by;
extern double time_replace_limit;

// global variables of bigsim
extern char* traceFileName;
extern JobInf *jobs;
#include <map>
#include <string>

std::map<std::string, double> *eventSubs = NULL;
std::map<int64_t, int64_t> *msgSizeSub = NULL;

void addEventSub(int jobid, char *key, double val, int numjobs) {
  if(eventSubs == NULL) {
    eventSubs = new std::map<std::string, double>[numjobs];
  }
  std::string skey(key);
  eventSubs[jobid][key] = (double)TIME_MULT * val;
}

void addMsgSizeSub(int jobid, int64_t key, int64_t val, int numjobs) {
  if(msgSizeSub == NULL) {
    msgSizeSub = new std::map<int64_t, int64_t>[numjobs];
  }
  msgSizeSub[jobid][key] = val;
}

#if TRACER_BIGSIM_TRACES
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
  pe->taskExecuted= new bool*[jobs[jobnum].numIters];
  pe->msgStatus= new bool*[jobs[jobnum].numIters];
  pe->allMarked= new bool[jobs[jobnum].numIters];
  for(int i = 0; i < jobs[jobnum].numIters; i++) {
    pe->taskStatus[i] = new bool[tlinerec.length()];
    pe->taskExecuted[i] = new bool[tlinerec.length()];
    pe->msgStatus[i] = new bool[tlinerec.length()];
    pe->allMarked[i] = false;
  }
  pe->tasksCount = tlinerec.length();
  pe->totalTasksCount = tlinerec.length();
  pe->firstTask = -1;

  if(jobs[jobnum].skipMsgId == -2) {
     pe->firstTask = 0;
  }
  
  double scaling_factor;
  bool isScaling = false;

  if(eventSubs != NULL) {
    std::map<std::string, double>::iterator loc1 =
      eventSubs[pe->jobNum].find("scale_all");
    if(loc1 != eventSubs[pe->jobNum].end()) {
      scaling_factor = loc1->second/TIME_MULT;
      isScaling = true;
    }
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
          pe->taskExecuted[i][logInd] = true;
        }
        continue;
      }
    }

    if(logInd < pe->firstTask) {
      assert(0);
    }

    // first job's index is zero
    setTaskFromLog(&(pe->myTasks[logInd]), bglog, penum, pe->myEmPE, 0, pe, 
      logInd, isScaling, scaling_factor);

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

void TraceReader::setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, 
  int myEmPE, int jobPEindex, PE* pe, int logInd, bool isScaling, 
  double scaling_factor)
{
  if(isScaling) {
    t->execTime = (double)TIME_MULT * bglog->execTime / scaling_factor;
  } else if(time_replace_limit != -1 && bglog->execTime >= time_replace_limit) {
    t->execTime = (double)TIME_MULT * time_replace_by;
  } else {
    t->execTime = (double)TIME_MULT * bglog->execTime;
  }

  if(strncmp(bglog->name, "AMPI_", 5) == 0) {
    t->execTime = soft_delay_mpi;
  }

  if(strcmp(bglog->name, "AMPI_START") == 0 ||
    strcmp(bglog->name, "AMPI_generic") == 0 ||
    strcmp(bglog->name, "AMPI_Recv") == 0 ||
    strcmp(bglog->name, "AMPI_Irecv") == 0 ||
    strcmp(bglog->name, "AMPI_SEND") == 0 ||
    strcmp(bglog->name, "AMPI_SEND_END") == 0 ||
    strcmp(bglog->name, "ATAReq_wait") == 0 ||
    strcmp(bglog->name, "RECV_RESUME") == 0 ||
    strcmp(bglog->name, "SPLIT_RESUME") == 0 ||
    strcmp(bglog->name, "PROBE_RESUME") == 0 ||
    strcmp(bglog->name, "IPROBE_RESUME") == 0 ||
    strcmp(bglog->name, "msgep") == 0 ||
    strcmp(bglog->name, "GroupReduce") == 0 ||
    strcmp(bglog->name, "start-broadcast") == 0 ||
    strcmp(bglog->name, "split-broadcast") == 0 ||
    strcmp(bglog->name, "end-broadcast") == 0 ||
    strcmp(bglog->name, "AMPI_WAIT") == 0 ||
    strcmp(bglog->name, "AMPI_WAITALL") == 0) {
    t->execTime = 0.0;
  }
 
  if(eventSubs != NULL) {
    std::map<std::string, double>::iterator loc =
      eventSubs[pe->jobNum].find(bglog->name);
    if(loc != eventSubs[pe->jobNum].end()) {
      t->execTime = loc->second;
    }
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
      pe->taskExecuted[i][logInd] = false;
    }
  } else {
    for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
      pe->msgStatus[i][logInd] = false;
      pe->taskStatus[i][logInd] = false;
      pe->taskExecuted[i][logInd] = false;
    }
  }

  t->msgEntCount = bglog->msgs.length();
  t->myEntries = new MsgEntry[t->msgEntCount];

  for(int i=0; i<bglog->msgs.length(); i++)
  {
    t->myEntries[i].msgId.id = bglog->msgs[i]->msgID;
    t->myEntries[i].msgId.pe = taskPE;
    
    t->myEntries[i].node = bglog->msgs[i]->dstNode;
    t->myEntries[i].thread = bglog->msgs[i]->tID;

    if(size_replace_limit[pe->jobNum] != -1 && bglog->msgs[i]->msgsize >= size_replace_limit[pe->jobNum]) {
      t->myEntries[i].msgId.size = size_replace_by[pe->jobNum];
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
    } else if(eventSubs != NULL && !isScaling) {
      std::map<std::string, double>::iterator loc =
        eventSubs[pe->jobNum].find(std::string((char *)bglog->evts[i]->data));
      if(loc != eventSubs[pe->jobNum].end()) {
        t->execTime = loc->second;
      }
    }
  }
}

#elif TRACER_OTF_TRACES
#include "otf2_reader.h"
void TraceReader_readOTF2Trace(PE* pe, int my_pe_num, int my_job, double *startTime) {
  pe->myNum = my_pe_num;
  pe->jobNum = my_job;
  LocationData *ld = new LocationData;
  
  readLocationTasks(my_job, jobs[my_job].reader, jobs[my_job].allData,
      my_pe_num, ld);

  pe->myTasks = new Task[ld->tasks.size()];
  memcpy(pe->myTasks, &ld->tasks[0], ld->tasks.size() * sizeof(Task));
  pe->tasksCount = ld->tasks.size();
  pe->totalTasksCount = pe->tasksCount;
  pe->taskStatus= new bool*[jobs[pe->jobNum].numIters];
  pe->taskExecuted= new bool*[jobs[pe->jobNum].numIters];
  pe->msgStatus= new bool*[jobs[pe->jobNum].numIters];
  pe->allMarked= new bool[jobs[pe->jobNum].numIters];
  for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
    pe->taskStatus[i] = new bool[pe->tasksCount];
    pe->taskExecuted[i] = new bool[pe->tasksCount];
    pe->msgStatus[i] = new bool[pe->tasksCount];
    pe->allMarked[i] = false;
  }
  pe->firstTask = 0;
  *startTime = 0;

  int num_communicators = jobs[my_job].allData->communicators.size();
  pe->collectiveSeq.resize(num_communicators, 0);
  pe->currentCollComm = pe->currentCollSeq = pe->currentCollTask = -1;
  pe->currentCollRank = pe->currentCollPartner = pe->currentCollSize = -1;
  pe->currentCollMsgSize = pe->currentCollSendCount = pe->currentCollRecvCount = -1;
 
  double user_timing, scaling_factor;
  bool isScaling = false, isUserTiming = false;

  if(eventSubs != NULL) {
    std::map<std::string, double>::iterator loc1 =
      eventSubs[pe->jobNum].find("scale_all");
    if(loc1 != eventSubs[pe->jobNum].end()) {
      scaling_factor = loc1->second/TIME_MULT;
      isScaling = true;
    }
    std::map<std::string, double>::iterator loc2 =
      eventSubs[pe->jobNum].find("user_code");
    if(loc2 != eventSubs[pe->jobNum].end()) {
      if(!isScaling) { 
        isUserTiming = true;
        user_timing = loc2->second;
      }
    }
  }

  for(int logInd = 0; logInd  < pe->tasksCount; logInd++)
  {
    Task *t = &(ld->tasks[logInd]);
    if(time_replace_limit != -1 && t->execTime >= time_replace_limit) {
      t->execTime = (double)TIME_MULT * time_replace_by;
    } 
    
    if(eventSubs != NULL && t->event_id == TRACER_USER_EVT) {
      if(isScaling) {
        t->execTime = t->execTime/scaling_factor;
      } else if(isUserTiming) {
        t->execTime = user_timing;
      }
    }

    for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
      pe->taskStatus[i][logInd] = false;
      pe->taskExecuted[i][logInd] = false;
      pe->msgStatus[i][logInd] = false;
    }
  
    if(t->event_id == TRACER_SEND_EVT || t->event_id == TRACER_RECV_POST_EVT
       || t->event_id == TRACER_RECV_EVT || t->event_id == TRACER_RECV_COMP_EVT
       || t->event_id == TRACER_COLL_EVT)
    { 
      if(size_replace_limit[pe->jobNum] != -1 && 
          t->myEntry.msgId.size >= size_replace_limit[pe->jobNum]) {
        t->myEntry.msgId.size = size_replace_by[pe->jobNum];
      }
      if(msgSizeSub != NULL) {
        std::map<int64_t, int64_t>::iterator loc =
          msgSizeSub[pe->jobNum].find(t->myEntry.msgId.size);
        if(loc != msgSizeSub[pe->jobNum].end()) {
          t->myEntry.msgId.size = loc->second;
        }
      }
    }
  }

  delete ld;
}
#endif

