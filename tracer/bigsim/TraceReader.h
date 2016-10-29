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
class PE;
class Node;
class Task;

class TraceReader {
public:
    TraceReader(char *);
    ~TraceReader();
#if TRACER_BIGSIM_TRACES
    void loadOffsets();
    void loadTraceSummary();
    void readTrace(int* tot, int* numnodes, int* empes, int* nwth, PE* pe,
        int penum, int jobnum, double* startTime);
    void setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, int emPE, int jobPEindex, PE* pe, int);
#endif

    int numEmPes;	// number of emulation PEs, there is a trace file for each of them
    int totalWorkerProcs;
    int totalNodes;
    int numWth;	//Working PEs per node
    int* allNodeOffsets;
    char tracePath[256];

    int fileLoc; // each worker needs separate file offset
    int firstLog; // first log of window to read for each worker
    int totalTlineLength; // apparently totalTlineLength should be kept for each PE!
};

#endif /* TRACEFILEREADER_H_ */
