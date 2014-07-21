#include "entities/MsgEntry.h"
#include "entities/PE.h"
#include "CWrapper.h"
#include "TraceReader.h"

extern "C" {

    //MsgEntry
    MsgEntry* newMsgEntry(){return new MsgEntry();}
    /*void MsgEntry_sendMsg(MsgEntry* m, unsigned long long startTime) {
        m->sendMsg(startTime);
    }*/
    int MsgEntry_getSize(MsgEntry* m){return m->msgId.size;}
    int MsgEntry_getPE(MsgEntry* m){return m->msgId.pe;}
    int MsgEntry_getID(MsgEntry* m){return m->msgId.id;}

    MsgID* newMsgID(int size, int pe, int id){return new MsgID(size, pe, id);}

    //PE
    PE* newPE(){return new PE();}
    void PE_set_busy(PE* p, bool b){p->busy = b;}
    bool PE_noUnsatDep(PE* p, int tInd){return p->noUnsatDep(tInd);}
    unsigned long long PE_getTaskExecTime(PE* p, int tInd){return p->noUnsatDep(tInd);}
    int PE_getTaskMsgEntryCount(PE* p, int tInd){return p->myTasks[tInd].msgEntCount;}
    MsgEntry* PE_getTaskMsgEntries(PE* p, int tInd){return p->myTasks[tInd].myEntries;}
    void PE_set_taskDone(PE* p, int tInd, bool b){p->myTasks[tInd].done=b;}
    int* PE_getTaskFwdDep(PE* p, int tInd){return p->myTasks[tInd].forwardDep;}
    int PE_getTaskFwdDepSize(PE* p, int tInd){return p->myTasks[tInd].forwDepSize;}
    void PE_set_currentTask(PE* p, int tInd){p->currentTask=tInd;}


    //TraceReader
    TraceReader* newTraceReader(){return new TraceReader();}
    void TraceReader_readTrace(TraceReader* t, int* tot, int* numnodes, int* empes, int* nwth, PE* pe, int penum, unsigned long long* startTime, int** dests){
         t->readTrace(tot, numnodes, empes, nwth, pe, penum, startTime, dests);
    }
    int TraceReader_totalWorkerProcs(TraceReader* t){return t->totalWorkerProcs;}

}
