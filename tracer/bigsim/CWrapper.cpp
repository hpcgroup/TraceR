#include "entities/MsgEntry.h"
#include "entities/PE.h"
#include "CWrapper.h"
#include "TraceReader.h"

extern "C" {

    //MsgID
    MsgID* newMsgID(int size, int pe, int id){return new MsgID(size, pe, id);}
    int MsgID_getSize(MsgID* m){return m->size;}
    int MsgID_getID(MsgID* m){return m->pe;}
    int MsgID_getPE(MsgID* m){return m->id;}

    //MsgEntry
    MsgEntry* newMsgEntry(){return new MsgEntry();}
    int MsgEntry_getSize(MsgEntry* m){return m->msgId.size;}
    int MsgEntry_getPE(MsgEntry* m){return m->msgId.pe;}
    int MsgEntry_getID(MsgEntry* m){return m->msgId.id;}
    int MsgEntry_getNode(MsgEntry* m){return m->node;}
    int MsgEntry_getThread(MsgEntry* m){return m->thread;}
    double MsgEntry_getSendOffset(MsgEntry* m){return m->sendOffset;}

    //PE
    PE* newPE(){return new PE();}
    void PE_set_busy(PE* p, bool b){p->busy = b;}
    bool PE_is_busy(PE* p){return p->busy;}
    bool PE_noUnsatDep(PE* p, int tInd){return p->noUnsatDep(tInd);}
    bool PE_noMsgDep(PE* p, int tInd){
        if(p->myTasks[tInd].myMsgId.pe < 0)
            return true;
        return false; 
    }
    double PE_getTaskExecTime(PE* p, int tInd){return p->taskExecTime(tInd);}
    int PE_getTaskMsgEntryCount(PE* p, int tInd){return p->myTasks[tInd].msgEntCount;}
    MsgEntry** PE_getTaskMsgEntries(PE* p, int tInd){
        return &(p->myTasks[tInd].myEntries);
    }
    MsgEntry* PE_getTaskMsgEntry(PE* p, int tInd, int mInd){
        return &(p->myTasks[tInd].myEntries[mInd]);
    }

    void PE_execPrintEvt(tw_lp * lp, PE* p, int tInd, double stime) {
        p->myTasks[tInd].printEvt(lp, stime, p->myNum, p->jobNum);
    }

    int PE_getFirstTask(PE* p){ return p->firstTask;}
    void PE_set_taskDone(PE* p, int tInd, bool b){p->myTasks[tInd].done=b;}
    bool PE_get_taskDone(PE* p, int tInd){return p->myTasks[tInd].done;}
    int* PE_getTaskFwdDep(PE* p, int tInd){return p->myTasks[tInd].forwardDep;}
    int PE_getTaskFwdDepSize(PE* p, int tInd){return p->myTasks[tInd].forwDepSize;}
    void PE_undone_fwd_deps(PE* p, int tInd){
        int fwd_dep_size = PE_getTaskFwdDepSize(p, tInd);
        int* fwd_deps = PE_getTaskFwdDep(p, tInd);
        for(int i=0; i<fwd_dep_size; i++){
            //if the forward dependency of the task is done
            if(PE_get_taskDone(p, fwd_deps[i])){
                //printf("Undo task_id: %d\n", fwd_deps[i]);
                PE_set_taskDone(p, fwd_deps[i], false);
                //Recursively mark the forward depencies as not done
                PE_undone_fwd_deps(p, fwd_deps[i]);
            }
        }
    }
    void PE_set_currentTask(PE* p, int tInd){p->currentTask=tInd;}
    int PE_get_currentTask(PE* p){return p->currentTask;}
    void PE_increment_currentTask(PE* p, int tInd){
        if(PE_get_currentTask(p) == tInd){
            PE_set_currentTask(p, tInd+1);
            // Search the following tasks until you see a not done task,
            // and increment the current task
            int i;
            for(i=tInd+1; i<PE_get_tasksCount(p); i++){
                if(PE_get_taskDone(p, i)){
                    PE_set_currentTask(p, i+1);
                }
                else return;
            }
        }
    }
    int PE_get_myEmPE(PE* p){return p->myEmPE;}
    int PE_get_myNum(PE* p){return p->myNum;}
    void PE_clearMsgBuffer(PE* p){p->msgBuffer.clear();}
    void PE_addToBuffer(PE* p, int task_id){
        bool found = (std::find(p->msgBuffer.begin(), p->msgBuffer.end(), task_id) != p->msgBuffer.end());
        if(found) assert(0);
        p->msgBuffer.push_back(task_id);
    }
    void PE_addToFrontBuffer(PE* p, int task_id){
        bool found = (std::find(p->msgBuffer.begin(), p->msgBuffer.end(), task_id) != p->msgBuffer.end());
        if(found) assert(0);
        p->msgBuffer.push_front(task_id);
    }
    int PE_getBufferSize(PE* p){ return p->msgBuffer.size();}
    void PE_resizeBuffer(PE* p, int num_elems_to_remove){
        int cur_size = p->msgBuffer.size();
        p->msgBuffer.resize(cur_size - num_elems_to_remove);
    }
    void PE_removeFromBuffer(PE* p, int task_id){
        //if the task_id is in the buffer, remove it from the buffer
        if(!p->msgBuffer.size()) assert(0);
        if(p->msgBuffer.back() != task_id) assert(0);
         p->msgBuffer.pop_back();

//No need to search the whole buffer, removes will be always from the back
/*
        for(int i=0; i<p->msgBuffer.size(); i++){
            if(p->msgBuffer[i] == task_id){
                p->msgBuffer.erase(p->msgBuffer.begin()+i);
                break;
            }
        }
*/
    }
    int PE_getNextBuffedMsg(PE* p){
        if(p->msgBuffer.size()<=0) return -1;
        else{
            int id = p->msgBuffer.front();
            p->msgBuffer.pop_front();
            return id;
        }
    }
    void PE_addToCopyBuffer(PE* p, int entry_task_id, int msg_task_id){
        //printf("Adding to copy buffer entry_task_id: %d, msg_task_id: %d\n", entry_task_id, msg_task_id);
        map<int, vector<int> >::iterator it;
        it=p->taskMsgBuffer.find(entry_task_id);
        if(it != p->taskMsgBuffer.end()){
            it->second.push_back(msg_task_id);
        }else{
            vector<int> msgs;
            msgs.push_back(msg_task_id);
            p->taskMsgBuffer[entry_task_id] = msgs;
        }
    }
    void PE_removeFromCopyBuffer(PE* p, int entry_task_id, int msg_task_id){
        map<int, vector<int> >::iterator it;
        it=p->taskMsgBuffer.find(entry_task_id);
        if(it == p->taskMsgBuffer.end()) assert(0);

        if(!it->second.size()) assert(0);
        if(it->second.back() != msg_task_id) assert(0);
        it->second.pop_back();

//No need to search the whole buffer, removes will be always from the back
/*
        for(int i=0; i<it->second.size(); i++){
            if((it->second)[i] == msg_task_id){
                (it->second).erase((it->second).begin()+i);
                return;
            }
        }
        assert(0);
*/
}
    int PE_getCopyBufferSize(PE* p, int entry_task_id){
        map<int, vector<int> >::iterator it;
        it=p->taskMsgBuffer.find(entry_task_id);
        if(it != p->taskMsgBuffer.end())
            return (it->second).size();
        else return 0;
    }
    int PE_getNextCopyBuffedMsg(PE* p, int entry_task_id){
        map<int, vector<int> >::iterator it;
        it=p->taskMsgBuffer.find(entry_task_id);
        if((it->second).size()<=0) return -1;
        else{
            int id = (it->second).front();
            (it->second).erase((it->second).begin());
            return id;
        }
    }
    void PE_moveFromCopyToMessageBuffer(PE* p, int entry_task_id){
        map<int, vector<int> >::iterator it;
        it=p->taskMsgBuffer.find(entry_task_id);
        if(it != p->taskMsgBuffer.end()){
            //printf("PE_moveFromCopyToMessageBuffer of size: %d. entry_task_id:%d. msg_task_ids: ", it->second.size(), entry_task_id) ;
            for(int i=0; i<it->second.size(); i++){
                PE_set_taskDone(p, (it->second)[i], false);
                PE_undone_fwd_deps(p, (it->second)[i]);
                //printf("%d, ", (it->second)[i]);
            }
            //printf("\n");
            if(it->second.size() != 0){
                p->msgBuffer.insert(p->msgBuffer.end(), it->second.begin(), it->second.end());
                //it->second.clear(); //BUG, it shoudl not clear the copy buffer
            }
        }
    }
    void PE_addToBusyStateBuffer(PE* p, bool state){
        p->busyStateBuffer.push_back(state);
    }
    bool PE_popBusyStateBuffer(PE* p){
        bool last_state = p->busyStateBuffer.back();
        p->busyStateBuffer.pop_back();
        return last_state;
    }
    bool PE_isLastStateBusy(PE* p){
        return p->busyStateBuffer.back();
    }

    int PE_findTaskFromMsg(PE* p, MsgID* msgId){
        return p->findTaskFromMsg(msgId);
    }
    void PE_invertMsgPe(PE* p, int tInd){
        p->invertMsgPe(tInd);
    }
    MsgID PE_getTaskMsgID(PE* p, int tInd){
        return p->myTasks[tInd].myMsgId;
    }
    int PE_get_tasksCount(PE* p){return p->tasksCount;}
    int PE_get_totalTasksCount(PE* p){return p->totalTasksCount;}
    void PE_printStat(PE* p){p->check();}
    int PE_get_numWorkThreads(PE* p){return p->numWth;}

    //TraceReader
    TraceReader* newTraceReader(char* s){return new TraceReader(s);}
    void TraceReader_loadTraceSummary(TraceReader* t){t->loadTraceSummary();}
    void TraceReader_loadOffsets(TraceReader* t){t->loadOffsets();}
    int* TraceReader_getOffsets(TraceReader* t){return t->allNodeOffsets;}
    void TraceReader_setOffsets(TraceReader* t, int** offsets){t->allNodeOffsets = *offsets;}
    void TraceReader_readTrace(TraceReader* t, int* tot, int* numnodes, int*
    empes, int* nwth, PE* pe, int penum, int jobnum, double* startTime){
         t->readTrace(tot, numnodes, empes, nwth, pe, penum, jobnum, startTime);
    }
    int TraceReader_totalWorkerProcs(TraceReader* t){return t->totalWorkerProcs;}

}
