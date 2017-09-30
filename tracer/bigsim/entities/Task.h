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

#ifndef TASK_H_
#define TASK_H_
#include "MsgEntry.h"
#include <cstdlib>
#include <cstdio>
#if TRACER_BIGSIM_TRACES
#include <mpi.h>
#include <ross.h>
#endif

#include <cstring>
#include <vector>
#define TIME_MULT 1000000000

#if TRACER_BIGSIM_TRACES
class BgPrint{
  public:
    void print(tw_lp * lp, double startTime, int PEno, int jobNo)
    {
      char str[1000];
      strcpy(str, "[%d %d : %s] ");
      strcat(str, msg);
      tw_output(lp, str, jobNo, PEno, taskName, startTime/((double)TIME_MULT));
    }
    char* msg;
    double time;
    char taskName[50];
};
#endif

// represents each DEP ~ SEB
class Task {
  public:
    Task();
    ~Task();
#if TRACER_BIGSIM_TRACES
    void printEvt(tw_lp * lp, double startTime, int PEno, int jobNo);
    std::vector<MsgEntry> myEntries; // outgoing messages of task
    int msgEntCount; // number of msg entries
    std::vector<int> forwardDep; //backward dependent tasks
    int forwDepSize;	// size of forwardDep array

    std::vector<int> backwardDep;	//forward dependent tasks
    int backwDepSize;	// size of backwDep array
    std::vector<BgPrint> myBgPrints;
    int bgPrintCount;
#elif TRACER_OTF_TRACES
    int64_t event_id;
    int64_t req_id;
    bool isNonBlocking;
    MsgEntry myEntry;
    bool beginEvent;
#else
#error Either TRACER_BIGSIM_TRACES or TRACER_OTF_TRACES should be 1
#endif
    bool endEvent;
    bool loopEvent, loopStartEvent;
    double execTime;	//execution time of the task
};

#endif /* TASK_H_ */
