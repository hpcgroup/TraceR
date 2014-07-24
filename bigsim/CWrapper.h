#ifndef __CWRAPPER_H
#define __CWRAPPER_H

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
//void MsgEntry_sendMsg(MsgEntry*, unsigned long long startTime);

//Event
typedef struct Event event;

//PE
typedef struct PE PE;
PE* newPE();
void PE_set_busy(PE* p, bool b);
bool PE_is_busy(PE* p);
bool PE_noUnsatDep(PE* p, int tInd);
unsigned long long PE_getTaskExecTime(PE* p, int tInd);
int PE_getTaskMsgEntryCount(PE* p, int tInd);
MsgEntry** PE_getTaskMsgEntries(PE* p, int tInd);
void PE_set_taskDone(PE* p, int tInd, bool b);
int* PE_getTaskFwdDep(PE* p, int tInd);
int PE_getTaskFwdDepSize(PE* p, int tInd);
void PE_set_currentTask(PE* p, int tInd);
int PE_get_myEmPE(PE* p);
void PE_addToBuffer(PE* p, int task_id);
int PE_getNextBuffedMsg(PE* p);
int PE_findTaskFromMsg(PE* p, MsgID* msgId);

//TraceReader
typedef struct TraceReader TraceReader;
TraceReader* newTraceReader();
void TraceReader_readTrace(TraceReader* t, int* tot, int* numnodes, int* empes, int* nwth, PE* pe, int penum, unsigned long long* startTime);
int TraceReader_totalWorkerProcs(TraceReader* t);

#ifdef __cplusplus
}
#endif

#endif
