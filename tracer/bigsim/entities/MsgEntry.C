/////////////////////////////////////////////////////////////////////////////
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

#include "MsgEntry.h"
#include <cstdio>

MsgEntry::MsgEntry() {
  node = INT_MIN;
  thread = INT_MIN;
  msgId.size = INT_MIN;
}

// From TCsim.C file:
// Given the following inputs for destNode and destTID, we need to
// send out messages either directly to the local node, or through
// the network.  To do this we have to specify correct information
// about the destination BGproc and the source and destination
// switch in the network

// destNode   destTID   Behavior
// ========== ========= ==============================================
// -1         -1        Broadcast to ALL worker threads of ALL nodes
// -2         -1        Multicast to all nodes given by the pe list in the task msg
// -1         K         SHOULD NOT HAPPEN???
// N          -1        Send to ALL worker threads of node N
// N          K         Send to worker thread K of node N
// -100-N     -1        Broadcast to all worker threads of all nodes
//                      except for N (no worker threads of N receive)
// -100-N     K         Broadcast to all worker threads of all nodes
//                      except worker K of node N
