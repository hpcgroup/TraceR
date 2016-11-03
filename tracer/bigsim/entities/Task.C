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
#include "Task.h"

Task::Task() {
  execTime = -1;
  endEvent = false;
  loopEvent = false;
  loopStartEvent = false;
#if TRACER_BIGSIM_TRACES
  backwDepSize = 0;
  forwDepSize = 0;
  backwardDep = 0;
  forwardDep = 0;
  myEntries = 0;
  msgEntCount = 0;
  bgPrintCount = 0;
#endif
}

#if TRACER_BIGSIM_TRACES
void Task::printEvt(tw_lp * lp, double startTime, int PEno, int jobNo)
{
  for(int i = 0; i < bgPrintCount; i++) {
    myBgPrints[i].print(lp, startTime, PEno, jobNo);
  }
}
#endif

Task::~Task()
{
#if TRACER_BIGSIM_TRACES
  delete[] forwardDep;
  delete[] backwardDep;
  delete[] myBgPrints;
  delete[] myEntries;
#endif
}

