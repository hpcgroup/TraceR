#ifndef __CWRAPPER_H
#define __CWRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

//MsgEntry
typedef struct MsgEntry MsgEntry;
typedef struct MsgID MsgID;
MsgID* newMsgID(int size, int pe, int id);

MsgEntry* newMsgEntry();
int MsgEntry_getSize(MsgEntry* m);
int MsgEntry_getID(MsgEntry* m);
int MsgEntry_getPE(MsgEntry* m);
//void MsgEntry_sendMsg(MsgEntry*, unsigned long long startTime);

//Event
typedef struct Event event;
//PE
typedef struct PE PE;
PE* newPE();
void PE_set_busy(PE* p, bool b);
bool PE_noUnsatDep(PE* p, int tInd);
unsigned long long PE_getTaskExecTime(PE* p, int tInd);
int PE_getTaskMsgEntryCount(PE* p, int tInd);
MsgEntry* PE_getTaskMsgEntries(PE* p, int tInd);
void PE_set_taskDone(PE* p, int tInd, bool b);
int* PE_getTaskFwdDep(PE* p, int tInd);
int PE_getTaskFwdDepSize(PE* p, int tInd);
void PE_set_currentTask(PE* p, int tInd);

//TraceReader
typedef struct TraceReader TraceReader;
TraceReader* newTraceReader();
void TraceReader_readTrace(TraceReader* t, int* tot, int* numnodes, int* empes, int* nwth, PE* pe, int penum, unsigned long long* startTime, int** dests);
int TraceReader_totalWorkerProcs(TraceReader* t);
#ifdef __cplusplus
}
#endif

#endif
