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

#ifndef MSGENTRY_H_
#define MSGENTRY_H_

#ifdef __cplusplus
#include <climits>
#endif

struct MsgID {
    int pe;	// PE that sent it
    int id; // emulating PE increments with each msg it sends
    int size;
#ifdef __cplusplus
    MsgID() : pe(INT_MIN), id(0), size(0) {}
    MsgID(int size_) : pe(INT_MIN), id(0), size(size_) {}
    MsgID(int size_, int pe_, int id_) : pe(pe_), id(id_), size(size_) {};
#endif
};

struct MsgEntry {
#ifdef __cplusplus
    MsgEntry();
    void sendMsg(double startTime);
#endif
    int node;	// node number in global order
    int thread;	// destination thread in node
    double sendOffset;
    MsgID msgId;
};

#endif /* MSGENTRY_H_ */
