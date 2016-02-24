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

#ifndef PE_H_
#define PE_H_

#include "MsgEntry.h"
#include <cstring>
#include "Task.h"
#include <vector>
#include <algorithm>
#include <list>
#include <map>

using namespace std;
class Task;

class PE {
  public:
    PE();
    ~PE();
    list<int> msgBuffer;
    vector<bool> busyStateBuffer;
    map<int, vector<int> > taskMsgBuffer; //For optimistic mode: store copy of the messages received per task
    Task* myTasks;	// all tasks of this PE
    double currTime;
    bool busy;
    int windowOffset;
    int beforeTask, totalTasksCount;
    int myNum, myEmPE, jobNum;
    int tasksCount;	//total number of tasks
    int currentTask; // index of first not-executed task (helps searching messages)
    int firstTask;

    bool noUnsatDep(int tInd);	// there is no unsatisfied dependency for task
    double taskExecTime(int tInd);
    void printStat();
    void check();
    void printState();

    //functions added by Bilge for codes-tracing
    void invertMsgPe(int tInd);
    double getTaskExecTime(int tInd);
    map<int, int>* msgDestLogs;
    int findTaskFromMsg(MsgID* msg);
    int numWth, numEmPes;

};

#endif /* PE_H_ */
