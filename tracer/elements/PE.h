//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// Written by:
//     Nikhil Jain <nikhil.jain@acm.org>
//     Bilge Acun <acun2@illinois.edu>
//     Abhinav Bhatele <bhatele@llnl.gov>
//
// LLNL-CODE-740483. All rights reserved.
//
// This file is part of TraceR. For details, see:
// https://github.com/LLNL/TraceR
// Please also read the LICENSE file for the MIT License notice.
//////////////////////////////////////////////////////////////////////////////

#ifndef PE_H_
#define PE_H_

#include "MsgEntry.h"
#include <cstring>
#include "Task.h"
#include <list>
#include <map>
#include <vector>
#include "reader/datatypes.h"

class Task;

class MsgKey {
  public:
  uint32_t rank, comm, tag;
  int64_t seq;
  MsgKey(uint32_t _rank, uint32_t _tag, uint32_t _comm, int64_t _seq) {
    rank = _rank; tag = _tag; comm = _comm; seq = _seq;
  }
  bool operator< (const MsgKey &rhs) const {
    if(rank != rhs.rank) return rank < rhs.rank;
    else if(tag != rhs.tag) return tag < rhs.tag;
    else return comm < rhs.comm;
    //else if(comm != rhs.comm) return comm < rhs.comm;
    //else return seq < rhs.seq;
  }
  ~MsgKey() { }
};
typedef std::map< MsgKey, std::list<int> > KeyType;

class CollMsgKey {
  public:
  uint32_t rank, comm;
  int64_t seq;
  CollMsgKey(uint32_t _rank, uint32_t _comm, int64_t _seq) {
    rank = _rank; comm = _comm; seq = _seq;
  }
  bool operator< (const CollMsgKey &rhs) const {
    if(rank != rhs.rank) return rank < rhs.rank;
    else if(comm != rhs.comm) return comm < rhs.comm;
    else return seq < rhs.seq;
  }
  ~CollMsgKey() { }
};
typedef std::map< CollMsgKey, std::list<int> > CollKeyType;

class PE {
  public:
    PE();
    ~PE();
    std::list<TaskPair> msgBuffer;
    Task* myTasks;	// all tasks of this PE
    bool **taskStatus, **taskExecuted;
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
    int loop_start_task;

    void goToNextIter(int iter);
    bool noUnsatDep(int iter, int tInd);	// there is no unsatisfied dependency for task
    void mark_all_done(int iter, int tInd);
    double taskExecTime(int tInd);
    void printStat(int iter);

    void invertMsgPe(int iter, int tInd);
    double getTaskExecTime(int tInd);
    void addTaskExecTime(int tInd, double time);
    std::map<int, int>* msgDestLogs;
    int findTaskFromMsg(MsgID* msg);
    int numWth, numEmPes;

    KeyType pendingMsgs;
    KeyType pendingRMsgs;
    int64_t *sendSeq, *recvSeq;
    std::map<int, int> pendingReqs;
    std::map<int, int64_t> pendingRReqs;

    //handling collectives
    std::vector<int64_t> collectiveSeq;
    std::map<int64_t, std::map<int64_t, std::map<int, int> > > pendingCollMsgs;
    CollKeyType pendingRCollMsgs;
    int64_t currentCollComm, currentCollSeq, currentCollTask, currentCollMsgSize;
    int currentCollRank, currentCollPartner, currentCollSize;
    int currentCollSendCount, currentCollRecvCount;
};

#endif /* PE_H_ */
