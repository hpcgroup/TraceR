#include <string.h>
#include <assert.h>
#include <ross.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <algorithm>

extern "C" {
#include "codes/model-net.h"
#include "codes/model-net-sched.h"
#include "codes/lp-io.h"
#include "codes/codes.h"
#include "codes/codes_mapping.h"
#include "codes/configuration.h"
#include "codes/lp-type-lookup.h"
}

#include "tracer-driver.h"
#include "qos-manager.h"

extern QoSManager qosManager;

void handle_recv_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    int task_id;
#if TRACER_BIGSIM_TRACES
    task_id = PE_findTaskFromMsg(ns->my_pe, &m->msgId);
#else
#if DEBUG_PRINT
    if(ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
    printf("%d RECD MSG: %d %d %d %lld\n", 
        ns->my_pe_num, m->msgId.pe, m->msgId.id, m->msgId.comm,
        m->msgId.seq);
    }
#endif
    MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
    KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
    assert((it == ns->my_pe->pendingMsgs.end()) || (!it->second.empty()));
    if(it == ns->my_pe->pendingMsgs.end() || it->second.front() == -1) {
      task_id = -1;
      ns->my_pe->pendingMsgs[key].push_back(task_id);
      b->c2 = 1;
      return;
    } else {
      b->c3 = 1;
      task_id = it->second.front();
      it->second.pop_front();
      if(it->second.empty()) {
        ns->my_pe->pendingMsgs.erase(it);
      }
#if DEBUG_PRINT
      printf("[%d:%d] RECD MSG FOUND TASK: %d %d %d %d - %d\n", ns->my_job,
          ns->my_pe_num, m->msgId.pe, m->msgId.id, m->msgId.comm,
          ns->my_pe->recvSeq[m->msgId.pe], task_id);
#endif
    }
#endif
    int iter = m->iteration;
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: handle_recv_event - received from %d id: %d for task: "
        "%d. TIME now:%f.\n", ns->my_pe_num, m->msgId.pe, m->msgId.id,
        task_id, now);
#endif
    bool isBusy = PE_is_busy(ns->my_pe);

#if TRACER_BIGSIM_TRACES
    if(task_id >= 0 && PE_noMsgDep(ns->my_pe, iter, task_id)) {
      m->executed.taskid = -2;
      return;
    }
#endif
#if TRACER_OTF_TRACES
    if(task_id == -1) {
      b->c4 = 1;
      return;
    }
#endif
#if DEBUG_PRINT
    if(1 || ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
    printf("%d Recv  busy %d %d\n", ns->my_pe_num, isBusy, task_id);
    }
#endif
    m->incremented_flag = isBusy;
    m->executed.taskid = -1;
    if(task_id>=0){
        //The matching task should not be already done
        if(PE_get_taskDone(ns->my_pe,iter,task_id)){ //TODO: check this
          printf("[%d:%d] WARNING: MSG RECV TASK IS DONE: %d\n", ns->my_job,
              ns->my_pe_num, task_id);
            assert(0);
        }
        PE_invertMsgPe(ns->my_pe, iter, task_id);
        if(m->msgId.size <= eager_limit && ns->my_pe->currIter == 0) {
          b->c1 = 1;
          if(m->msgId.pe != ns->my_pe_num) {
            PE_addTaskExecTime(ns->my_pe, task_id, nic_delay);
          }
          PE_addTaskExecTime(ns->my_pe, task_id, m->msgId.size * copy_per_byte);
        }
        //Add task to the task buffer
        TaskPair pair;
        pair.iter = iter; pair.taskid = task_id;
        PE_addToBuffer(ns->my_pe, &pair);

        //Check if pe is busy, if not we can execute the next available task in the buffer
        //else do nothing
#if TRACER_OTF_TRACES
        assert(!isBusy);
#endif
        if(!isBusy){
            TaskPair buffd_task = PE_getNextBuffedMsg(ns->my_pe);
            //Store the executed_task id for reverse handler msg
            m->executed = buffd_task;
            m->fwd_dep_count = 0;
            if(buffd_task.taskid != -1){
                exec_task(ns, buffd_task, lp, m, b);
            }
        }
        return;
    }
    printf("PE%d: Going beyond hash look up on receiving a message %d:%d\n",
      lpid_to_pe(lp->gid), m->msgId.pe, m->msgId.id);
    assert(0);
}

void handle_recv_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
#if TRACER_OTF_TRACES
    if(b->c2 || b->c4) {
      MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
      KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
      if(b->c2) {
        assert(it != ns->my_pe->pendingMsgs.end());
        it->second.pop_back();
        if(it->second.size() == 0) {
          ns->my_pe->pendingMsgs.erase(it);
        }
      } else if(b->c4) {
        ns->my_pe->pendingMsgs[key].push_front(-1);
      }
      return;
    }
#endif
#if TRACER_BIGSIM_TRACES
    if(m->executed.taskid == -2) {
        return;
    }
#endif
    bool wasBusy = m->incremented_flag;
    int iter = m->iteration;
    PE_set_busy(ns->my_pe, wasBusy);
#if DEBUG_PRINT
    if(1 || ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
    printf("%d Recv rev busy %d %d\n", ns->my_pe_num, wasBusy, m->executed.taskid);
    }
#endif

#if TRACER_BIGSIM_TRACES
    int task_id = PE_findTaskFromMsg(ns->my_pe, &m->msgId);
#else
    int task_id =  m->executed.taskid;
    if(b->c3) {
      MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
      ns->my_pe->pendingMsgs[key].push_front(task_id);
    }
#endif
    PE_invertMsgPe(ns->my_pe, iter, task_id);

#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: In reverse handler of recv message with id: %d  task_id: %d."
    " wasBusy: %d. TIME now:%f\n", ns->my_pe_num, m->msgId.id, task_id,
    wasBusy, now);
#endif
    if(b->c1) {
      if(m->msgId.pe != ns->my_pe_num) {
        PE_addTaskExecTime(ns->my_pe, task_id, -1 * nic_delay);
      }
      PE_addTaskExecTime(ns->my_pe, task_id, -1 * (m->msgId.size * copy_per_byte));
    }

    if(!wasBusy){
        //if the task that I executed was not me
        if(m->executed.taskid != task_id && m->executed.taskid != -1){
#if TRACER_OTF_TRACES
        assert(0);
#endif
            PE_addToFrontBuffer(ns->my_pe, &m->executed);    
            TaskPair pair;
            pair.iter = iter; pair.taskid = task_id;
            PE_removeFromBuffer(ns->my_pe, &pair);
        }
        if(m->executed.taskid != -1) {
          exec_task_rev(ns, m->executed, lp, m, b);
        }
    } else {
#if TRACER_OTF_TRACES
        assert(0);
#endif
        assert(m->executed.taskid == -1);
        TaskPair pair;
        pair.iter = iter; pair.taskid = task_id;
        PE_removeFromBuffer(ns->my_pe, &pair);
    }
}


#if TRACER_BIGSIM_TRACES

void handle_send_comp_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_send_comp_rev_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_recv_post_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_recv_post_rev_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}

#elif TRACER_OTF_TRACES

void handle_send_comp_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    std::map<int, int>::iterator it = ns->my_pe->pendingReqs.find(m->msgId.id);
    if(it == ns->my_pe->pendingReqs.end()) {
      b->c1 = 1;
    } else if(it->second == -1) {
      b->c2 = 1;
      ns->my_pe->pendingReqs.erase(it);
    } else {
      b->c3 = 1;
      m->executed.taskid = it->second;
      exec_comp(ns, ns->my_pe->currIter, it->second, 0, 0, 0, lp);
      ns->my_pe->pendingReqs.erase(it);
    }
}

void handle_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    if(b->c1) return;
    if(b->c2) ns->my_pe->pendingReqs[m->msgId.id] = -1;
    if(b->c3) ns->my_pe->pendingReqs[m->msgId.id] = m->executed.taskid;
}

void handle_recv_post_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
  KeyType::iterator it = ns->my_pe->pendingRMsgs.find(key);
  if(it == ns->my_pe->pendingRMsgs.end() || it->second.front() == -1) {
    b->c1 = 1;
    ns->my_pe->pendingRMsgs[key].push_back(-1);
  } else {
    b->c2 = 1;
    assert(it->second.size() != 0);
    Task *t = &ns->my_pe->myTasks[it->second.front()];
    m->model_net_calls = 1;
    delegate_send_msg(ns, lp, m, b, t, it->second.front(), 0);
    m->executed.taskid = it->second.front();
    it->second.pop_front();
    if(it->second.size() == 0) {
      ns->my_pe->pendingRMsgs.erase(it);
    }
  }
#if DEBUG_PRINT
  printf("%d: Recv post recevied %d %d %d %d, found %d %d\n", ns->my_pe_num, 
      m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq, b->c2, m->executed.taskid);
#endif
}

void handle_recv_post_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
  KeyType::iterator it = ns->my_pe->pendingRMsgs.find(key);
  if(b->c1) {
    it->second.pop_back();
    if(it->second.size() == 0) {
      ns->my_pe->pendingRMsgs.erase(it);
    }
  }
  if(b->c2) {
     ns->my_pe->pendingRMsgs[key].push_front(m->executed.taskid);
     for(int i = 0; i < m->model_net_calls; i++) {
       model_net_event_rc(net_id, lp, 0);
     }
  }
}

void delegate_send_msg(proc_state *ns,
  tw_lp * lp,
  proc_msg * m,
  tw_bf * b,
  Task * t,
  int taskid,
  tw_stime delay) {
  proc_msg m_local;
  if(t->isNonBlocking) {
    m_local.proc_event_type = SEND_COMP;
    m_local.msgId.id = t->req_id;
  } else {
    m_local.proc_event_type = EXEC_COMPLETE;
    m_local.iteration = ns->my_pe->currIter;
    m_local.msgId.id = taskid;
  }
  MsgEntry *taskEntry = &t->myEntry;
  enqueue_msg(ns, MsgEntry_getSize(taskEntry),
      ns->my_pe->currIter, &taskEntry->msgId, taskEntry->msgId.seq,
      pe_to_lpid(taskEntry->node, ns->my_job), nic_delay+rdma_delay+delay, 
      RECV_MSG, &m_local, lp);
}

#endif 

//executes the task with the specified id
tw_stime exec_task(
            proc_state * ns,
            TaskPair task_id,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b)
{
    m->model_net_calls = 0;
    if(ns->my_pe->taskExecuted[task_id.iter][task_id.taskid]) {
      b->c10 = 1;
      return 0;
    }
    //Check if the backward dependencies are satisfied
    //If not, do nothing yet
    //If yes, execute the task
    if(!PE_noUnsatDep(ns->my_pe, task_id.iter, task_id.taskid)){
        printf("[%d:%d] WARNING: TASK HAS TASK DEP: %d\n", ns->my_job,
          ns->my_pe_num, task_id.taskid);
        assert(0);
    }
    //Check if the task is already done -- safety check?
    if(PE_get_taskDone(ns->my_pe, task_id.iter, task_id.taskid)){
        printf("[%d:%d] WARNING: TASK IS ALREADY DONE: %d\n", ns->my_job,
          ns->my_pe_num, task_id.taskid);
        assert(0);
    }
#if TRACER_BIGSIM_TRACES
    //Check the task does not have any message dependency
    if(!PE_noMsgDep(ns->my_pe, task_id.iter, task_id.taskid)){
        printf("[%d:%d] WARNING: TASK HAS MESSAGE DEP: %d\n", ns->my_job,
          ns->my_pe_num, task_id.taskid);
        assert(0);
    }
#endif

    tw_stime recvFinishTime = 0;
#if TRACER_OTF_TRACES
    Task *t = &ns->my_pe->myTasks[task_id.taskid];

    //delegate to routine that handles collectives
    if(t->event_id == TRACER_COLL_EVT) {
      b->c11 = 1;
      perform_collective(ns, task_id.taskid, lp, m, b);
      ns->my_pe->taskExecuted[task_id.iter][task_id.taskid] = true;
      m->saved_task = ns->my_pe->currentTask;
      ns->my_pe->currentTask = task_id.taskid;
      return 0;
    }

    //else continue
    bool needPost = false, returnAtEnd = false;
    int64_t seq;
    if(t->event_id == TRACER_RECV_POST_EVT) {
      seq = ns->my_pe->recvSeq[t->myEntry.node];
      ns->my_pe->pendingRReqs[t->req_id] = seq;
      ns->my_pe->recvSeq[t->myEntry.node]++;
#if DEBUG_PRINT
      if(ns->my_pe_num ==  1222 || ns->my_pe_num == 1217) {
        printf("%d Post Irecv: %d - %d %d %d %lld \n", ns->my_pe_num, 
            t->req_id, t->myEntry.node, t->myEntry.msgId.id,
            t->myEntry.msgId.comm, ns->my_pe->recvSeq[t->myEntry.node]-1);
      }
#endif
    }
    if((t->event_id == TRACER_RECV_EVT || t->event_id == TRACER_RECV_COMP_EVT) 
       && !PE_noMsgDep(ns->my_pe, task_id.iter, task_id.taskid)) {
      b->c7 = 1;
      seq = ns->my_pe->recvSeq[t->myEntry.node];
      if(t->event_id == TRACER_RECV_COMP_EVT) {
        std::map<int, int64_t>::iterator it = ns->my_pe->pendingRReqs.find(t->req_id);
        assert(it != ns->my_pe->pendingRReqs.end());
        seq = it->second;
        t->myEntry.msgId.seq = seq;
        ns->my_pe->pendingRReqs.erase(it);
      }
      MsgKey key(t->myEntry.node, t->myEntry.msgId.id, t->myEntry.msgId.comm, seq);
      if(t->event_id == TRACER_RECV_EVT) {
        needPost = true;
        ns->my_pe->recvSeq[t->myEntry.node]++;
      }
      KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
      if(it == ns->my_pe->pendingMsgs.end()) {
        assert(PE_is_busy(ns->my_pe) == false);
        ns->my_pe->pendingMsgs[key].push_back(task_id.taskid);
#if DEBUG_PRINT
        if(1 || ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
        printf("%d PUSH recv: %d - %d %d %d %lld %lld %d\n", ns->my_pe_num, 
            task_id.taskid, t->myEntry.node, t->myEntry.msgId.id,
            t->myEntry.msgId.comm, seq, ns->my_pe->recvSeq[t->myEntry.node]-1, t->event_id == TRACER_RECV_EVT);
        }
#endif
        b->c21 = 1;
        if(!needPost) {
          return 0;
        } else {
          returnAtEnd = true;
        }
      } else {
        b->c22 = 1;
#if DEBUG_PRINT
        if(ns->my_pe_num ==  1222 || ns->my_pe_num == 1217) {
        printf("%d Recv matched: %d - %d %d %d %lld, %lld %d\n", ns->my_pe_num, 
            task_id.taskid, t->myEntry.node, t->myEntry.msgId.id,
            t->myEntry.msgId.comm, seq, ns->my_pe->recvSeq[t->myEntry.node]-1, t->event_id == TRACER_RECV_EVT);
        }
#endif
        assert(it->second.front() == -1);
        ns->my_pe->pendingMsgs[key].pop_front();
        if(it->second.size() == 0) {
          ns->my_pe->pendingMsgs.erase(it);
        }
      }
    }
    if(t->myEntry.node != ns->my_pe_num && 
       t->myEntry.msgId.size > eager_limit &&
       (t->event_id == TRACER_RECV_POST_EVT || needPost)) {
      m->model_net_calls++;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, seq,  
        pe_to_lpid(t->myEntry.node, ns->my_job), nic_delay, RECV_POST, lp);
#if DEBUG_PRINT
      printf("%d: Recv post %d %d %d %d\n", ns->my_pe_num, 
          t->myEntry.node, t->myEntry.msgId.id, t->myEntry.msgId.comm, 
          seq);
#endif
      recvFinishTime += nic_delay;
    }
    if(returnAtEnd) return 0;
#endif

    //Executing the task, set the pe as busy
    PE_set_busy(ns->my_pe, true);
#if DEBUG_PRINT
    if(1 || ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
      printf("%d Set busy true %d\n", ns->my_pe_num, task_id.taskid);
    }
#endif
    //Mark the execution time of the task
    tw_stime time = PE_getTaskExecTime(ns->my_pe, task_id.taskid);
    ns->my_pe->taskExecuted[task_id.iter][task_id.taskid] = true;
    m->saved_task = ns->my_pe->currentTask;
    ns->my_pe->currentTask = task_id.taskid;

#if TRACER_BIGSIM_TRACES
    //For each entry of the task, create a recv event and send them out to
    //whereever it belongs       
    int msgEntCount= PE_getTaskMsgEntryCount(ns->my_pe, task_id.taskid);
    int myPE = ns->my_pe_num;
    int nWth = PE_get_numWorkThreads(ns->my_pe);  
    int myNode = myPE/nWth;
    tw_stime soft_latency = codes_local_latency(lp);
    tw_stime delay = MPI_INTERNAL_DELAY + soft_latency; //intra node latency
    double sendFinishTime = 0;

    for(int i=0; i<msgEntCount; i++){
        MsgEntry* taskEntry = PE_getTaskMsgEntry(ns->my_pe, task_id.taskid, i);
        tw_stime copyTime = copy_per_byte * MsgEntry_getSize(taskEntry);
        if(MsgEntry_getSize(taskEntry) > eager_limit) {
          copyTime = soft_latency;
        }
        int node = MsgEntry_getNode(taskEntry);
        int thread = MsgEntry_getThread(taskEntry);
        tw_stime sendOffset = soft_delay_mpi;
        
        //If there are intraNode messages
        if (node == myNode || node == -1 || (node <= -100 && (node != -100-myNode || thread != -1)))
        {
          if(node == -100-myNode && thread != -1)
          {
            int destPE = myNode*nWth - 1;
            for(int i=0; i<nWth; i++)
            {
              destPE++;
              if(i == thread) continue;
              delay += copyTime;
              if(destPE == ns->my_pe_num) {
                exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
              }else{
                m->model_net_calls++;
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    task_id.iter, &taskEntry->msgId, 0 /*not used */,
                    pe_to_lpid(destPE, ns->my_job), sendOffset+delay, RECV_MSG,
                    lp);
              }
            }
          } else if(node != -100-myNode && node <= -100) {
            int destPE = myNode*nWth - 1;
            for(int i=0; i<nWth; i++)
            {
              destPE++;
              delay += copyTime;
              if(destPE == ns->my_pe_num){
                exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
              }else{
                m->model_net_calls++;
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    task_id.iter, &taskEntry->msgId,  0 /*not used */,
                    pe_to_lpid(destPE, ns->my_job), sendOffset+delay, RECV_MSG,
                    lp);
              }
            }
          } else if(thread >= 0) {
            int destPE = myNode*nWth + thread;
            delay += copyTime;
            if(destPE == ns->my_pe_num){
              exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
            }else{
              m->model_net_calls++;
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  task_id.iter, &taskEntry->msgId,  0 /*not used */,
                  pe_to_lpid(destPE, ns->my_job), sendOffset+delay, RECV_MSG,
                  lp);
            }
          } else if(thread==-1) { // broadcast to all work cores
            int destPE = myNode*nWth - 1;
            for(int i=0; i<nWth; i++)
            {
              destPE++;
              delay += copyTime;
              if(destPE == ns->my_pe_num){
                exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
              }else{
                m->model_net_calls++;
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    task_id.iter, &taskEntry->msgId,  0 /*not used */,
                    pe_to_lpid(destPE, ns->my_job), sendOffset+delay, RECV_MSG,
                    lp);
              }
            }
          }
        }
          
        if(node != myNode)
        {
          delay += copyTime;
          if(node >= 0){
            m->model_net_calls++;
            send_msg(ns, MsgEntry_getSize(taskEntry),
                task_id.iter, &taskEntry->msgId,  0 /*not used */,
                pe_to_lpid(node, ns->my_job), sendOffset+delay, RECV_MSG, lp);
          }
          else if(node == -1){
            bcast_msg(ns, MsgEntry_getSize(taskEntry),
                task_id.iter, &taskEntry->msgId,
                sendOffset+delay, copyTime, lp, m);
          }
          else if(node <= -100 && thread == -1){
            for(int j=0; j<jobs[ns->my_job].numRanks; j++){
              if(j == -node-100 || j == myNode) continue;
              delay += copyTime;
              m->model_net_calls++;
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  task_id.iter, &taskEntry->msgId,  0 /*not used */,
                  pe_to_lpid(j, ns->my_job), sendOffset+delay, RECV_MSG, lp);
            }

          }
          else if(node <= -100){
            for(int j=0; j<jobs[ns->my_job].numRanks; j++){
              if(j == myNode) continue;
              delay += copyTime;
              m->model_net_calls++;
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  task_id.iter, &taskEntry->msgId,  0 /*not used */,
                  pe_to_lpid(j, ns->my_job), sendOffset+delay, RECV_MSG, lp);
            }
          }
          else{
            printf("message not supported yet! node:%d thread:%d\n",node,thread);
          }
        }
        sendFinishTime = delay;
    }

    PE_execPrintEvt(lp, ns->my_pe, task_id.taskid, tw_now(lp));
#else 
    tw_stime sendOffset, soft_latency = codes_local_latency(lp);
    tw_stime delay = soft_latency; //intra node latency
    double sendFinishTime = 0;

    if(t->event_id == TRACER_SEND_EVT) {
      b->c23 = 1;
      MsgEntry *taskEntry = &t->myEntry;
      bool isCopying = true;
      tw_stime copyTime = copy_per_byte * MsgEntry_getSize(taskEntry);
      int node = MsgEntry_getNode(taskEntry);
      if(MsgEntry_getSize(taskEntry) > eager_limit && node != ns->my_pe_num) {
        copyTime = soft_latency;
        isCopying = false;
      }
      sendOffset = soft_delay_mpi;

      if(node == ns->my_pe_num) {
        exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 
          taskEntry->msgId.comm, sendOffset+copyTime+delay, 1, lp);
        sendFinishTime = sendOffset + copyTime;
      } else {
#if DEBUG_PRINT
        if(ns->my_pe_num ==  1222 || ns->my_pe_num == 1217) {
          printf("%d SEND to: %d  %d %d %lld\n", ns->my_pe_num, node, 
            taskEntry->msgId.id, taskEntry->msgId.comm, ns->my_pe->sendSeq[node]);
        }
#endif

        if(isCopying) {
          m->model_net_calls++;
          send_msg(ns, MsgEntry_getSize(taskEntry),
              task_id.iter, &taskEntry->msgId, ns->my_pe->sendSeq[node]++,
              pe_to_lpid(node, ns->my_job), sendOffset+copyTime+nic_delay+delay, 
              RECV_MSG, lp);
          sendFinishTime = sendOffset+copyTime;
        } else {
          b->c24 = 1;
          taskEntry->msgId.seq = ns->my_pe->sendSeq[node]++;
          if(t->isNonBlocking) {
            if(ns->my_pe->pendingReqs.find(t->req_id) == 
               ns->my_pe->pendingReqs.end()) {
              b->c25 = 1;
              ns->my_pe->pendingReqs[t->req_id] = -1;
            }
          }
          MsgKey key(taskEntry->node, taskEntry->msgId.id, taskEntry->msgId.comm, 
            taskEntry->msgId.seq);
          KeyType::iterator it = ns->my_pe->pendingRMsgs.find(key);
          if(it == ns->my_pe->pendingRMsgs.end() || (it->second.front() != -1)) {
            b->c26 = 1;
            ns->my_pe->pendingRMsgs[key].push_back(task_id.taskid);
          } else {
            b->c27 = 1;
            m->model_net_calls++;
            delegate_send_msg(ns, lp, m, b, t, task_id.taskid, sendOffset+delay);
            it->second.pop_front();
            if(it->second.size() == 0) {
              ns->my_pe->pendingRMsgs.erase(it);
            }
          }
#if DEBUG_PRINT
          printf("%d: Send %d %d %d %d, nonblock %d/%d, wait %d, do %d, task %d\n", ns->my_pe_num, 
           taskEntry->node, taskEntry->msgId.id, taskEntry->msgId.comm, 
           taskEntry->msgId.seq, t->isNonBlocking, t->req_id, b->c26, b->c27, task_id.taskid);
#endif
          if(!t->isNonBlocking) return 0;
          sendFinishTime += sendOffset+copyTime+nic_delay;
        }
      }
    }
   
    //print event
    if(t->event_id >= 0) {
      char str[1000];
      if(t->beginEvent) {
        strcpy(str, "[ %d %d : Begin %s %f ]\n");
      } else {
        strcpy(str, "[ %d %d : End %s %f ]\n");
      }
      tw_output(lp, str, ns->my_job, ns->my_pe_num, 
          jobs[ns->my_job].allData->strings[jobs[ns->my_job].allData->regions[t->event_id].name].c_str(),
          tw_now(lp)/((double)TIME_MULT));
    }

    if(t->loopStartEvent) {
      ns->my_pe->loop_start_task = task_id.taskid;
    }

    if(t->event_id == TRACER_SEND_COMP_EVT) {
      std::map<int, int>::iterator it = ns->my_pe->pendingReqs.find(t->req_id);
      if(it !=  ns->my_pe->pendingReqs.end()) {
        if(it->second == -1) {
          b->c28 = 1;
          ns->my_pe->pendingReqs[t->req_id] = task_id.taskid;
        }
        b->c29 = 1;
        return 0;
      } 
    }
#endif
    
    if(ns->my_pe_num == 0 && (ns->my_pe->currentTask % print_frequency == 0)) {
      char str[1000];
      strcpy(str, "[ %d %d : time at task %d/%d %f ]\n");
      tw_output(lp, str, ns->my_job, ns->my_pe_num, 
          ns->my_pe->currentTask, PE_get_tasksCount(ns->my_pe), tw_now(lp)/((double)TIME_MULT));
    }

    //Complete the task
    tw_stime finish_time = codes_local_latency(lp) + sendFinishTime + recvFinishTime + time;
    exec_comp(ns, task_id.iter, task_id.taskid, 0, finish_time, 0, lp);
    if(PE_isEndEvent(ns->my_pe, task_id.taskid)) {
      ns->end_ts = tw_now(lp);
    }
    //Return the execution time of the task
    return time;
}

void exec_task_rev(
    proc_state * ns,
    TaskPair task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b) {

  if(b->c10) return;

#if TRACER_OTF_TRACES
  if(b->c11) {
    perform_collective_rev(ns, task_id.taskid, lp, m, b);
    ns->my_pe->taskExecuted[task_id.iter][task_id.taskid] = false;
    ns->my_pe->currentTask = m->saved_task;
    return;
  }
  
  Task *t = &ns->my_pe->myTasks[task_id.taskid];
  if(t->event_id == TRACER_RECV_POST_EVT) {
    ns->my_pe->pendingRReqs.erase(t->req_id);
    ns->my_pe->recvSeq[t->myEntry.node]--;
  }

  int64_t seq;
  if(b->c7) {
    if(t->event_id == TRACER_RECV_COMP_EVT) {
      ns->my_pe->pendingRReqs[t->req_id] = t->myEntry.msgId.seq;
      seq = t->myEntry.msgId.seq;
    }
    if(t->event_id == TRACER_RECV_EVT) {
      ns->my_pe->recvSeq[t->myEntry.node]--;
      seq = ns->my_pe->recvSeq[t->myEntry.node];
    }
  }
  
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }

  if(b->c21 || b->c22) {
    MsgKey key(t->myEntry.node, t->myEntry.msgId.id, t->myEntry.msgId.comm, seq);
    KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
    if(b->c21) {
      assert(it != ns->my_pe->pendingMsgs.end());
      it->second.pop_back();
      if(it->second.size() == 0) {
        ns->my_pe->pendingMsgs.erase(it);
      }
      return;
    } else if(b->c22) {
      ns->my_pe->pendingMsgs[key].push_front(-1);
    }
  }
#endif

  ns->my_pe->taskExecuted[task_id.iter][task_id.taskid] = false;
  ns->my_pe->currentTask = m->saved_task;
  codes_local_latency_reverse(lp);
#if TRACER_OTF_TRACES
  if(b->c23) {
    Task *t = &ns->my_pe->myTasks[task_id.taskid];
    MsgEntry *taskEntry = &t->myEntry;
    ns->my_pe->sendSeq[MsgEntry_getNode(taskEntry)]--;
    if(b->c24) {
      if(b->c25) {
        ns->my_pe->pendingReqs.erase(t->req_id);
      }
      MsgKey key(taskEntry->node, taskEntry->msgId.id, taskEntry->msgId.comm, 
          taskEntry->msgId.seq);
      if(b->c26) {
        ns->my_pe->pendingRMsgs[key].pop_back();
        if(ns->my_pe->pendingRMsgs[key].size() == 0) {
          ns->my_pe->pendingRMsgs.erase(key);
        }
      }
      if(b->c27) {
        ns->my_pe->pendingRMsgs[key].push_front(-1);
      }
      if(!t->isNonBlocking) return;
    }
  }
  if(b->c28) { 
    Task *t = &ns->my_pe->myTasks[task_id.taskid];
    ns->my_pe->pendingReqs[t->req_id] = -1;
  }
  if(b->c29) return;
#endif
  codes_local_latency_reverse(lp);
}
//Creates and sends the message
int send_msg(
        proc_state * ns,
        int size,
        int iter,
        MsgID *msgId,
        int64_t seq,
        int dest_id,
        tw_stime sendOffset,
        enum proc_event evt_type,
        tw_lp * lp,
        bool fillSz,
        int64_t size2) {
        proc_msg m_remote;

        m_remote.proc_event_type = evt_type;
        m_remote.src = lp->gid;
        if(fillSz) {
          m_remote.msgId.size = size2;
        } else {
          m_remote.msgId.size = size;
        }
        m_remote.msgId.pe = msgId->pe;
        m_remote.msgId.id = msgId->id;
#if TRACER_OTF_TRACES
        m_remote.msgId.comm = msgId->comm;
        m_remote.msgId.seq = seq;
#endif
        m_remote.iteration = iter;

        int prio = qosManager.getServiceLevel(ns->my_job, lpid_to_pe(lp->id), lpid_to_pe(dest_id));
        model_net_set_msg_param(MN_MSG_PARAM_SCHED, MN_SCHED_PARAM_PRIO, (void*)&prio);
        /*   model_net_event params:
             int net_id, char* category, tw_lpid final_dest_lp,
             uint64_t message_size, tw_stime offset, int remote_event_size,
             const void* remote_event, int self_event_size,
             const void* self_event, tw_lp *sender */

        model_net_event(net_id, "test", dest_id, size, sendOffset,
          sizeof(proc_msg), &m_remote, 0, NULL, lp);
    
    return 0;
}

void enqueue_msg(
        proc_state * ns,
        int size,
        int iter,
        MsgID *msgId,
        int64_t seq,
        int dest_id,
        tw_stime sendOffset,
        enum proc_event evt_type,
        proc_msg *m_local,
        tw_lp * lp) {
        proc_msg m_remote;

        m_remote.proc_event_type = evt_type;
        m_remote.src = lp->gid;
        m_remote.msgId.size = size;
        m_remote.msgId.pe = msgId->pe;
        m_remote.msgId.id = msgId->id;
#if TRACER_OTF_TRACES
        m_remote.msgId.comm = msgId->comm;
        m_remote.msgId.seq = seq;
#endif
        m_remote.iteration = iter;

        int prio = qosManager.getServiceLevel(ns->my_job, lpid_to_pe(lp->id), lpid_to_pe(dest_id));
        model_net_set_msg_param(MN_MSG_PARAM_SCHED, MN_SCHED_PARAM_PRIO, (void*)&prio);
        model_net_event(net_id, "p2p", dest_id, size, sendOffset,
          sizeof(proc_msg), (const void*)&m_remote, sizeof(proc_msg), m_local, 
          lp);
}


