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

#ifndef __CWRAPPER_H
#define __CWRAPPER_H

#include <ross.h>
#include "datatypes.h"

//MsgID
typedef struct MsgID MsgID;
MsgID* newMsgID(int size, int pe, int id);
int MsgID_getSize(MsgID* m);
int MsgID_getID(MsgID* m);
int MsgID_getPE(MsgID* m);

//MsgEntry
typedef struct MsgEntry MsgEntry;
MsgEntry* newMsgEntry();
int MsgEntry_getSize(MsgEntry* m);
int MsgEntry_getID(MsgEntry* m);
int MsgEntry_getPE(MsgEntry* m);
int MsgEntry_getNode(MsgEntry* m);
int MsgEntry_getThread(MsgEntry* m);

//PE
typedef struct PE PE;
PE* newPE();
void PE_set_busy(PE* p, bool b);
bool PE_is_busy(PE* p);
bool PE_noUnsatDep(PE* p, int, int tInd);
bool PE_noMsgDep(PE* p, int, int tInd);
int PE_get_iter(PE* p);
void PE_inc_iter(PE* p);
void PE_dec_iter(PE* p);
double PE_getTaskExecTime(PE* p, int tInd);
void PE_addTaskExecTime(PE* p, int tInd, double time);
#if TRACER_BIGSIM_TRACES
int PE_getTaskMsgEntryCount(PE* p, int tInd);
MsgEntry** PE_getTaskMsgEntries(PE* p, int tInd);
MsgEntry* PE_getTaskMsgEntry(PE* p, int tInd, int mInd);
void PE_execPrintEvt(tw_lp * lp, PE* p, int tInd, double stime);
#endif
void PE_set_taskDone(PE* p, int, int tInd, bool b);
void PE_mark_all_done(PE *p, int iter, int task_id);
bool PE_get_taskDone(PE* p, int, int tInd);
#if TRACER_BIGSIM_TRACES
int* PE_getTaskFwdDep(PE* p, int tInd);
int PE_getTaskFwdDepSize(PE* p, int tInd);
void PE_undone_fwd_deps(PE* p, int iter, int tInd);
#endif
void PE_set_currentTask(PE* p, int tInd);
int PE_get_currentTask(PE* p);
int PE_get_myEmPE(PE* p);
int PE_get_myNum(PE* p);
int PE_getFirstTask(PE* p);
bool PE_isEndEvent(PE *p, int task_id);
bool PE_isLoopEvent(PE *p, int task_id);

int PE_getBufferSize(PE* p);
void PE_clearMsgBuffer(PE* p);
void PE_addToBuffer(PE* p, TaskPair *task_id);
void PE_addToFrontBuffer(PE* p, TaskPair *task_id);
void PE_removeFromBuffer(PE* p, TaskPair *task_id);
void PE_resizeBuffer(PE* p, int num_elems_to_remove);
TaskPair PE_getNextBuffedMsg(PE* p);

int PE_findTaskFromMsg(PE* p, MsgID* msgId);
void PE_invertMsgPe(PE* p, int, int tInd);
int PE_get_tasksCount(PE* p);
int PE_get_totalTasksCount(PE* p);
void PE_printStat(PE* p);
int PE_get_numWorkThreads(PE* p);

#if TRACER_BIGSIM_TRACES
//TraceReader
typedef struct TraceReader TraceReader;
TraceReader* newTraceReader(char*);
void TraceReader_loadTraceSummary(TraceReader* t);
void TraceReader_loadOffsets(TraceReader* t);
int* TraceReader_getOffsets(TraceReader* t);
void TraceReader_setOffsets(TraceReader* t, int** offsets);
void TraceReader_readTrace(TraceReader* t, int* tot, int* numnodes, int* empes,
    int* nwth, PE* pe, int penum, int jobnum, double* startTime);
int TraceReader_totalWorkerProcs(TraceReader* t);
#endif
void addEventSub(int job, char *key, double val, int numjobs);
void addMsgSizeSub(int job, int64_t key, int64_t val, int numjobs);

bool isPEonThisRank(int jobID, int i);
void TraceReader_readOTF2Trace(PE* pe, int my_pe_num, int my_job, double *startTime);
#endif
