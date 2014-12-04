#ifndef __CWRAPPER_H
#define __CWRAPPER_H

/*
 *
 * This file is a wrapper for the c++ functions in this folder
 * to be called from the modelnet-test-bigsim.c file.
 * The functions are used for trace reading and storing task dependencies.
 * BigSim trace reading code and the entities are taken from fastSim code.
 *
 * Author: Bilge Acun
 * 2014
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

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
unsigned long long MsgEntry_getSendOffset(MsgEntry* m);

//PE
typedef struct PE PE;
PE* newPE();
void PE_set_busy(PE* p, bool b);
bool PE_is_busy(PE* p);
bool PE_noUnsatDep(PE* p, int tInd);
bool PE_noMsgDep(PE* p, int tInd);
unsigned long long PE_getTaskExecTime(PE* p, int tInd);
int PE_getTaskMsgEntryCount(PE* p, int tInd);
MsgEntry** PE_getTaskMsgEntries(PE* p, int tInd);
MsgEntry* PE_getTaskMsgEntry(PE* p, int tInd, int mInd);
void PE_set_taskDone(PE* p, int tInd, bool b);
bool PE_get_taskDone(PE* p, int tInd);
int* PE_getTaskFwdDep(PE* p, int tInd);
int PE_getTaskFwdDepSize(PE* p, int tInd);
void PE_set_currentTask(PE* p, int tInd);
int PE_get_currentTask(PE* p);
void PE_increment_currentTask(PE* p, int tInd);
int PE_get_myEmPE(PE* p);
int PE_get_myNum(PE* p);

void PE_addToBuffer(PE* p, int task_id);
void PE_removeFromBuffer(PE* p, int task_id);
int PE_getNextBuffedMsg(PE* p);

//---
//Optimistic mode related stuff
void PE_addToCopyBuffer(PE* p, int entry_task_id, int msg_task_id);
void PE_removeFromCopyBuffer(PE* p, int entry_task_id, int msg_task_id);
int PE_getCopyBufferSize(PE* p, int entry_task_id);
int PE_getNextCopyBuffedMsg(PE* p, int entry_task_id);
void PE_moveFromCopyToMessageBuffer(PE* p, int entry_task_id);
void PE_addToBusyStateBuffer(PE* p, bool state);
void PE_popBusyStateBuffer(PE* p);
bool PE_isLastStateBusy(PE* p);
//---

int PE_findTaskFromMsg(PE* p, MsgID* msgId);
void PE_invertMsgPe(PE* p, int tInd);
MsgID PE_getTaskMsgID(PE* p, int tInd);
int PE_get_tasksCount(PE* p);
int PE_get_totalTasksCount(PE* p);
void PE_printStat(PE* p);
int PE_get_numWorkThreads(PE* p);

//TraceReader
typedef struct TraceReader TraceReader;
TraceReader* newTraceReader();
void TraceReader_readTrace(TraceReader* t, int* tot, int* numnodes, int* empes, int* nwth, PE* pe, int penum, unsigned long long* startTime);
int TraceReader_totalWorkerProcs(TraceReader* t);

#ifdef __cplusplus
}
#endif

#endif
