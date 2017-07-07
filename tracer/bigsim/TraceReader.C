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
  allNodeOffsets = NULL;
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

void TraceReader::readTrace(PE* pe){
  pe->numWth = numWth;
  pe->numEmPes = numEmPes;

  traceFileName = tracePath;
  if(jobs[pe->jobNum].skipMsgId == -1) {
    BgTimeLineRec tlinerec2;
    BgReadProc( 0, numWth , numEmPes, totalWorkerProcs, allNodeOffsets, tlinerec2);
    for(int j = 0; j < tlinerec2.length(); j++) {
      BgTimeLog *bglog = tlinerec2[j];
      if(bglog->isStartEvent()) {
        jobs[pe->jobNum].skipMsgId = bglog->msgs[0]->msgID;
        break;
      }
    }
    if(jobs[pe->jobNum].skipMsgId == -1) {
      jobs[pe->jobNum].skipMsgId = -2;
    }
  }

  BgTimeLineRec tlinerec; // Time line (list of logs)
  int status = BgReadProc( pe->myNum, numWth , numEmPes, totalWorkerProcs,
      allNodeOffsets, tlinerec);
  assert(status!=-1);
  pe->myTasks.resize(tlinerec.length());
  pe->initialize(jobs);
  pe->firstTask = -1;
  pe->myEmPE = (pe->myNum/numWth)%numEmPes;

  if(jobs[pe->jobNum].skipMsgId == -2) {
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

  for(int logInd=0; logInd<pe->tasksCount; logInd++)
  {
    BgTimeLog *bglog=tlinerec[logInd];

    if(pe->firstTask == -1) {
      if(bglog->msgId.pe() == 0 && bglog->msgId.msgID() == jobs[pe->jobNum].skipMsgId) {
        pe->firstTask = logInd;
      } else {
        for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
          pe->taskStatus[i][logInd] = true;
          pe->taskExecuted[i][logInd] = true;
        }
        continue;
      }
    }

    assert(logInd >= pe->firstTask);

    // first job's index is zero
    setTaskFromLog(&(pe->myTasks[logInd]), bglog, pe->myNum, pe->myEmPE, 0, pe->jobNum,
      logInd, isScaling, scaling_factor);

    //depends on message or not
    for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
      pe->msgStatus[i][logInd] = (bglog->msgId.pe() < 0);
    }

    int sPe = bglog->msgId.pe();
    int smsgID = bglog->msgId.msgID();
    if(sPe >= 0) {
        std::map<int, int>::iterator it;
        it = pe->msgDestLogs[(sPe/numWth)%numEmPes].find(smsgID); 
        // some task set it before so it is a broadcast
        if (it == pe->msgDestLogs[(sPe/numWth)%numEmPes].end()){
            pe->msgDestLogs[(sPe/numWth)%numEmPes].insert(std::pair<int,int>(smsgID, logInd));
        } else{
            // it may be a broadcast
            printf(" %d I should never come here, please fix me %d\n", pe->myNum, it->second);
            assert(0);
            it->second = -100;
        }
    }
    if(logInd == pe->firstTask) {
      for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
        pe->msgStatus[i][logInd] = true;
      }
    }
  }
}

void TraceReader::setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, 
  int myEmPE, int jobPEindex, int taskJob, int logInd, bool isScaling,
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
      eventSubs[taskJob].find(bglog->name);
    if(loc != eventSubs[taskJob].end()) {
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

  t->msgEntCount = bglog->msgs.length();
  t->myEntries.resize(t->msgEntCount);

  for(int i=0; i<bglog->msgs.length(); i++)
  {
    t->myEntries[i].msgId.id = bglog->msgs[i]->msgID;
    t->myEntries[i].msgId.pe = taskPE;
    
    t->myEntries[i].node = bglog->msgs[i]->dstNode;
    t->myEntries[i].thread = bglog->msgs[i]->tID;

    if(size_replace_limit[taskJob] != -1 && bglog->msgs[i]->msgsize >= size_replace_limit[taskJob]) {
      t->myEntries[i].msgId.size = size_replace_by[taskJob];
    } else {
      t->myEntries[i].msgId.size = bglog->msgs[i]->msgsize;
    }
  }

  t->backwDepSize = bglog->backwardDeps.length();
  t->backwardDep.resize(t->backwDepSize);
  for(int i=0; i< t->backwDepSize; i++)
  {
    t->backwardDep[i]= abs(bglog->backwardDeps[i]->seqno);
  }

  t->forwDepSize=bglog->forwardDeps.length();
  t->forwardDep.resize(t->forwDepSize);
  for(int i=0; i<t->forwDepSize; i++){
    t->forwardDep[i]= abs(bglog->forwardDeps[i]->seqno);
  }

  t->bgPrintCount=0;
  for(int i=0; i< bglog->evts.length();i++){
    if (bglog->evts[i]->eType == BG_EVENT_PRINT)  t->bgPrintCount++;
  }
  if (t->bgPrintCount)  t->myBgPrints.resize(t->bgPrintCount);
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
        eventSubs[taskJob].find(std::string((char *)bglog->evts[i]->data));
      if(loc != eventSubs[taskJob].end()) {
        t->execTime = loc->second;
      }
    }
  }
}

#elif TRACER_OTF_TRACES
#include "otf2_reader.h"
void TraceReader_readOTF2Trace(PE* pe) {
  LocationData ld;
  readLocationTasks(pe->jobNum, jobs[pe->jobNum].reader, jobs[pe->jobNum].allData,
      pe->myNum, &ld);

  pe->myTasks.swap(ld.tasks);
  pe->initialize(jobs);
  pe->firstTask = 0;

  int num_communicators = jobs[pe->jobNum].allData->communicators.size();
  pe->collectiveSeq.resize(num_communicators, 0);
 
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
    Task *t = &(pe->myTasks[logInd]);
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
}
#endif

