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
#include <list>
#include <map>
#include "datatypes.h"

class Task;

class MsgKey {
  public:
  int rank, comm, tag;
  uint64_t seq;
  MsgKey(int _rank, int _tag, int _comm, uint64_t _seq) {
    rank = _rank; tag = _tag; comm = _comm; seq = _seq;
  }
  bool operator< (const MsgKey &rhs) const {
    if(rank != rhs.rank) return rank < rhs.rank;
    else if(tag != rhs.tag) return tag < rhs.tag;
    else if(comm != rhs.comm) return comm < rhs.comm;
    else return seq < rhs.seq;
  }
  ~MsgKey() { }
};
typedef std::map< MsgKey, std::list<int> > KeyType;

class PE {
  public:
    PE();
    ~PE();
    std::list<TaskPair> msgBuffer;
    Task* myTasks;	// all tasks of this PE
    bool **taskStatus;
    bool **msgStatus;
    bool *allMarked;
    double currTime;
    bool busy;
    int beforeTask, totalTasksCount;
    int myNum, myEmPE, jobNum;
    int tasksCount;	//total number of tasks
    int currentTask; // index of first not-executed task (helps searching messages)
    int firstTask;
    int currIter;

    bool noUnsatDep(int iter, int tInd);	// there is no unsatisfied dependency for task
    void mark_all_done(int iter, int tInd);
    double taskExecTime(int tInd);
    void printStat();
    void check();
    void printState();

    //functions added by Bilge for codes-tracing
    void invertMsgPe(int iter, int tInd);
    double getTaskExecTime(int tInd);
    void addTaskExecTime(int tInd, double time);
    std::map<int, int>* msgDestLogs;
    int findTaskFromMsg(MsgID* msg);
    int numWth, numEmPes;
    KeyType pendingMsgs;
    int64_t *sendSeq, *recvSeq;
};

#endif /* PE_H_ */
