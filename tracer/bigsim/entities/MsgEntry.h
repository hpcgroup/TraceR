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

#ifndef MSGENTRY_H_
#define MSGENTRY_H_

#ifdef __cplusplus
#include <climits>
#endif
#include <stdint.h>

struct MsgID {
    int pe;	
    int id; 
    uint64_t size;
#ifdef __cplusplus
    MsgID() : pe(INT_MIN), id(0), size(0) {}
    MsgID(int size_) : pe(INT_MIN), id(0), size(size_) {}
    MsgID(int size_, int pe_, int id_) : pe(pe_), id(id_), size(size_) {};
#endif
#if TRACER_OTF_TRACES
    int comm, coll_type;
    int64_t seq;
#endif
};

struct MsgEntry {
#ifdef __cplusplus
    MsgEntry();
#endif
    int node;	// node number in global order
    int thread;
    MsgID msgId;
};

#endif /* MSGENTRY_H_ */
