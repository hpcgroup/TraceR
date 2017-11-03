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

#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#if TRACER_OTF_TRACES
#include "otf2_reader.h"
#endif

#include <map>
#include <list>

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
#if TRACER_OTF_TRACES
    AllData *allData;
    OTF2_Reader *reader;
    bool localDefs;
#endif
} JobInf;

#endif

