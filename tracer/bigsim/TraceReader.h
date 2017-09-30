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

#ifndef TRACEFILEREADER_H_
#define TRACEFILEREADER_H_
#include "assert.h"
#if TRACER_BIGSIM_TRACES
#include "blue.h"
#include "blue_impl.h"
#endif
#include "entities/PE.h"
#include "entities/Task.h"

class TraceReader {
public:
    TraceReader(char *);
    ~TraceReader();
#if TRACER_BIGSIM_TRACES
    void loadOffsets();
    void loadTraceSummary();
    void readTrace(PE* pe);
    void setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, int emPE, int jobPEindex, int taskJob, int, bool, double);
#endif

    int numEmPes;	// number of emulation PEs, there is a trace file for each of them
    int totalWorkerProcs;
    int totalNodes;
    int numWth;	//Working PEs per node
    int* allNodeOffsets;
    char tracePath[256];
};

#endif /* TRACEFILEREADER_H_ */
