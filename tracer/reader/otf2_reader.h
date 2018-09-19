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

#ifndef _OTF2_READER_H_
#define _OTF2_READER_H_
#if TRACER_OTF_TRACES

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <mpi.h>
#include <otf2/otf2.h>
#include <vector>
#include <map>
#include <string>
#include "elements/Task.h"

#if MPI_VERSION < 3
#define OTF2_MPI_UINT64_T MPI_UNSIGNED_LONG
#define OTF2_MPI_INT64_T  MPI_LONG
#endif
#include <otf2/OTF2_MPI_Collectives.h>

enum Tracer_evt_type {
  TRACER_USER_EVT = -1,
  TRACER_PRINT_EVT = -2,
  TRACER_SEND_EVT = -3,
  TRACER_RECV_EVT = -4,
  TRACER_COLL_EVT = -5,
  TRACER_SEND_COMP_EVT = -6,
  TRACER_RECV_POST_EVT = -7,
  TRACER_RECV_COMP_EVT = -8,
  TRACER_LOOP_EVT = -9
};

struct ClockProperties {
  uint64_t ticks_per_second;
  double ticksToSecond;
  uint64_t time_offset;
};

struct Region {
  OTF2_StringRef name;
  OTF2_RegionRole role;
  OTF2_Paradigm paradigm;
  bool isTracerPrintEvt;
  bool isLoopEvt;
  bool isCommunication;
};

struct Group {
  OTF2_GroupType type;
  std::vector<uint64_t> members;
  std::map<int, int> rmembers;
};

struct LocationData {
  uint64_t lastLogTime;
  bool firstEnter;
  std::vector<Task> tasks;
};

struct AllData {
  ClockProperties clockProperties;
  std::vector<uint64_t> locations;
  std::map<uint64_t, std::string> strings;
  std::map<uint64_t,uint64_t> communicators;
  std::map<uint64_t,Group> groups;
  std::map<uint64_t,Region> regions;
  LocationData *ld;
  std::map<int, int> matchRecvIds;//temp space
};

OTF2_Reader * readGlobalDefinitions(int jobID, char* tracefileName, 
  AllData *allData);

void readLocationTasks(int jobID, OTF2_Reader *reader, AllData *allData, 
  uint32_t loc, LocationData* ld);

void closeReader(OTF2_Reader *reader);
#endif
#endif
