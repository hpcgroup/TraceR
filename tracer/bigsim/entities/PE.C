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

#include "assert.h"
#include "PE.h"
#include <math.h>
#define MAX_LOGS 5000000
extern JobInf *jobs;

PE::PE() {
  busy = false;
  currentTask = 0;
  beforeTask = 0;
  currIter = 0;
}

PE::~PE() {
    msgBuffer.clear();
#if TRACER_BIGSIM_TRACES
    delete [] myTasks;
    delete [] msgDestLogs;
#endif
}

void PE::mark_all_done(int iter, int tInd) {
  if(allMarked[iter]) return;
  for(int i = tInd + 1; i < tasksCount; i++) {
    taskStatus[iter][i] = true;
  }
  if(allMarked[iter]) true;
}

bool PE::noUnsatDep(int iter, int tInd)
{
#if TRACER_BIGSIM_TRACES
  for(int i=0; i<myTasks[tInd].backwDepSize; i++)
  {
    int bwInd = myTasks[tInd].backwardDep[i];
    if(!taskStatus[iter][bwInd])
      return false;
  }
  return true;
#else
  return taskStatus[iter][tInd - 1];
#endif
}

double PE::taskExecTime(int tInd)
{
  return myTasks[tInd].execTime;
}

void PE::printStat()
{
  int countTask=0;
  for(int j = 0; j < jobs[jobNum].numIters; j++) {
    for(int i=0; i<tasksCount; i++)
    {
      if(!taskStatus[j][i])
      {
        printf("PE: %d not done:%d,%d\n", myNum, j, i);
        countTask++;
      }
    }
  }
  if(countTask != 0) {
    printf("PE%d: not done count:%d \n ",myNum, countTask);
  }
}

void PE::check()
{
  printStat();
}

void PE::printState()
{
  printStat();
}

void PE::invertMsgPe(int iter, int tInd)
{
  msgStatus[iter][tInd] = !msgStatus[iter][tInd];
}

double PE::getTaskExecTime(int tInd)
{
  return myTasks[tInd].execTime;
}

void PE::addTaskExecTime(int tInd, double time)
{
  myTasks[tInd].execTime += time;
}

int PE::findTaskFromMsg(MsgID* msgId)
{
  std::map<int, int>::iterator it;
  int sPe = msgId->pe;
  int sEmPe = (sPe/numWth)%numEmPes;
  int smsgID = msgId->id;
  it = msgDestLogs[sEmPe].find(smsgID);
  if(it!=msgDestLogs[sEmPe].end())
    return it->second;
  else return -1;
}
