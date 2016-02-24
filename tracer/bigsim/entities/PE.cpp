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

#include "PE.h"
#include <math.h>
#define MAX_LOGS 5000000

PE::PE() {
  busy = false;
  currentTask = 0;
  windowOffset = 0;
  beforeTask = 0;
  busyStateBuffer.push_back(false);
}

PE::~PE() {
    msgBuffer.clear();
    taskMsgBuffer.clear();
    delete [] myTasks;
    delete [] msgDestLogs;
}

bool PE::noUnsatDep(int tInd)
{
  for(int i=0; i<myTasks[tInd].backwDepSize; i++)
  {
    int bwInd = myTasks[tInd].backwardDep[i] - windowOffset;
    if(/*bwInd >= currentTask &&*/ !myTasks[bwInd].done)
      return false;
  }
  return true;
}

double PE::taskExecTime(int tInd)
{
    return myTasks[tInd].execTime;
}
// no stats yet?
void PE::printStat()
{
  int countTask=0;
  for(int i=0; i<tasksCount; i++)
  {
    // assert(myTasks[i].done);
    if(!myTasks[i].done)
    {
      printf("PE: %d not done:%d\n", myNum, i);
      countTask++;
    }
  }
}

void PE::check()
{
  int countTask=0;
  for(int i=firstTask; i<tasksCount; i++)
  {
    if(!myTasks[i].done)
    {
      countTask++;
    }
  }

  if(countTask != 0) {
    printf("PE%d: not done count:%d ",myNum, countTask);
    int i = 0, count=0;
    while (i < tasksCount) {
      if (!myTasks[i].done) {
        printf(" %d", i);
        i++;
        count = 0;
        while ((i < tasksCount) && (!myTasks[i].done)) {
          i++;
          count++;
        }
        if (count) {
          printf("-%d", i-1);
        }
      }
      i++;
    }
    printf("\n");
  }
  //else printf("PE:%d ALL TASKS ARE DONE\n", myNum);
}

void PE::printState()
{
  printf("PE:%d, busy:%d, currentTask:%d totalTasks:%d\n", myNum, busy, currentTask, totalTasksCount);
  printf("msgBuffer: ");
/*
  for(int i=0; i<msgBuffer.size(); i++)
  {
    printf("%d, ",msgBuffer[i]);
  }
*/
  printf("\n tasks from curr: ");
  for(int i=currentTask; i<tasksCount; i++)
  {
    printf("%d, ", myTasks[i].done);
  }
  printf("\n");

}

//BILGE
void PE::invertMsgPe(int tInd)
{
  myTasks[tInd].myMsgId.pe = -myTasks[tInd].myMsgId.pe;
}

double PE::getTaskExecTime(int tInd)
{
  return myTasks[tInd].execTime;
}

int PE::findTaskFromMsg(MsgID* msgId)
{
  map<int, int>::iterator it;
  int sPe = msgId->pe;
  int sEmPe = (sPe/numWth)%numEmPes;
  int smsgID = msgId->id;
  it = msgDestLogs[sEmPe].find(smsgID);
  if(it!=msgDestLogs[sEmPe].end())
    return it->second;
  else return -1;
}
