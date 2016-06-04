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

#ifndef _DATATYPES_H_
#define _DATATYPES_H_

struct TaskPair {
  int iter;
  int taskid;

#ifdef __cplusplus
  TaskPair(int a, int b) {
    iter = a;
    taskid = b;
  }

  TaskPair() {
    iter = -1;
    taskid = -1;
  }

  TaskPair(const TaskPair &t) {
    iter = t.iter;
    taskid = t.taskid;
  }

  inline bool operator==(const TaskPair &t) {
    return iter == t.iter && taskid == t.taskid;
  }
#endif
};

typedef struct TaskPair TaskPair;

typedef struct JobInf {
    int numRanks;
    char traceDir[256];
    char map_file[256];
    int *rankMap;
    int *offsets;
    int skipMsgId;
    int numIters;
} JobInf;

#endif

