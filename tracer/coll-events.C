#include <string.h>
#include <assert.h>
#include <ross.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <algorithm>

extern "C" {
#include "codes/model-net.h"
#include "codes/lp-io.h"
#include "codes/codes.h"
#include "codes/codes_mapping.h"
#include "codes/configuration.h"
#include "codes/lp-type-lookup.h"
}

#include "tracer-driver.h"

// the indexing should match between the #define and the lookUpTable
#define TRACER_A2A 0
#define TRACER_ALLGATHER 1
#define TRACER_BRUCK 2
#define TRACER_BLOCKED 3
#define TRACER_SCATTER 4

Coll_lookup lookUpTable[] = { { COLL_A2A, COLL_A2A_SEND_DONE },
                              { COLL_ALLGATHER, COLL_ALLGATHER_SEND_DONE },
                              { COLL_BRUCK, COLL_BRUCK_SEND_DONE },
                              { COLL_A2A_BLOCKED, COLL_A2A_BLOCKED_SEND_DONE },
                              { COLL_SCATTER, COLL_SCATTER_SEND_DONE }
                            };


void handle_bcast_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {

  tw_stime soft_latency = codes_local_latency(lp);
  m->model_net_calls = 0;
  int num_sends = bcast_msg(ns, m->msgId.size, m->iteration, &m->msgId,
      0, soft_latency, lp, m);

  if(!num_sends) num_sends++;

  tw_event*  e = tw_event_new(lp->gid, num_sends * soft_latency + codes_local_latency(lp), lp);
  proc_msg * msg = (proc_msg*)tw_event_data(e);
  memcpy(&msg->msgId, &m->msgId, sizeof(m->msgId));
  msg->iteration = m->iteration;
  msg->proc_event_type = RECV_MSG;
  tw_event_send(e);
}

void handle_bcast_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  codes_local_latency_reverse(lp);
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));
}


#if TRACER_BIGSIM_TRACES

void handle_coll_recv_post_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {} 
void handle_coll_recv_post_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {} 
void perform_collective( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b) {}
void perform_collective_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b) {} 
void perform_bcast( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_bcast_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_reduction( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_reduction_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void perform_a2a( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void perform_a2a_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void handle_a2a_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_a2a_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void perform_allreduce( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_allreduce_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void perform_allgather( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void perform_allgather_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void handle_allgather_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_allgather_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void perform_bruck( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void perform_bruck_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void handle_bruck_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_bruck_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void perform_a2a_blocked( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void perform_a2a_blocked_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
void handle_a2a_blocked_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_a2a_blocked_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void perform_scatter_small( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_scatter_small_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_scatter( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void perform_scatter_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
void handle_scatter_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_scatter_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_coll_complete_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
void handle_coll_complete_rev_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
int send_coll_comp(proc_state *, tw_stime, int, tw_lp *, int, proc_msg*) {}
int send_coll_comp_rev(proc_state *, tw_stime, int, tw_lp *, int, proc_msg *) {}

#elif TRACER_OTF_TRACES

void enqueue_coll_msg(
        int index,
        proc_state * ns,
        int size,
        int iter,
        MsgID *msgId,
        int64_t seq,
        int dest,
        tw_stime sendOffset,
        tw_stime copyTime,
        tw_lp * lp,
        proc_msg *m,
        tw_bf * b,
        bool useEager = false) {
    
    CollMsgKey key(dest, msgId->comm, seq);
    CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.find(key);  
    bool isEager = (size <= eager_limit) || useEager;
    //if(it != ns->my_pe->pendingRCollMsgs.end() && it->second.size() == 0) {
    //  CollKeyType::iterator it2 = ns->my_pe->pendingRCollMsgs.begin();
    //  printf("%d enqueue %d %d %d -- %d %d -- %d %d %d %d \n", ns->my_pe_num, dest, 
    //  msgId->comm, seq, ns->my_pe->pendingRCollMsgs.size(), it->second.size(),
    //  it2->first.rank, it2->first.comm, it2->first.seq, it->second.size());
    //  fflush(stdout);
    //  assert(0);
    //}
    if(!isEager && (it == ns->my_pe->pendingRCollMsgs.end() || 
        it->second.front() != -1)) {
      b->c16 = 1;
      ns->my_pe->pendingRCollMsgs[key].push_back(index);
      //printf("%d Added %d %d %d\n",  ns->my_pe_num, dest, msgId->comm, seq);
    } else {
      proc_msg m_remote, m_local;
      m_remote.proc_event_type = lookUpTable[index].remote_event;
      m_remote.src = lp->gid;
      m_remote.msgId.size = size;
      m_remote.msgId.pe = msgId->pe;
      m_remote.msgId.id = msgId->id;
#if TRACER_OTF_TRACES
      m_remote.msgId.comm = msgId->comm;
      m_remote.msgId.seq = seq;
#endif
      m_remote.iteration = iter;

      m_local.proc_event_type = lookUpTable[index].local_event;
      m_local.executed.taskid = ns->my_pe->currentCollTask;

      model_net_event(net_id, "coll", pe_to_lpid(dest, ns->my_job), size, 
          sendOffset + copyTime*(isEager?1:0), sizeof(proc_msg), 
          (const void*)&m_remote, sizeof(proc_msg), &m_local, lp);
      m->model_net_calls++;
      if(!isEager) {
        b->c17 = 1;
        it->second.pop_front();
        if(it->second.size() == 0) {
          ns->my_pe->pendingRCollMsgs.erase(it);
        }
      }
    }
}


void enqueue_coll_msg_rev(
        int index,
        proc_state * ns,
        MsgID *msgId,
        int64_t seq,
        int dest,
        tw_lp * lp,
        proc_msg *m,
        tw_bf * b) {
  CollMsgKey key(dest, msgId->comm, seq);
  if(b->c16) {
    //printf("%d Removing %d %d %d\n", ns->my_pe_num, dest, msgId->comm, seq);
    //fflush(stdout);
    assert(ns->my_pe->pendingRCollMsgs.find(key) != ns->my_pe->pendingRCollMsgs.end());
    ns->my_pe->pendingRCollMsgs[key].pop_back();
    if(ns->my_pe->pendingRCollMsgs[key].size() == 0) {
      ns->my_pe->pendingRCollMsgs.erase(key);
    }
  }
  if(b->c17) {
    ns->my_pe->pendingRCollMsgs[key].push_front(-1);
  }
}

void handle_coll_recv_post_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  //printf("%d recv post %d %d %d\n", ns->my_pe_num, m->msgId.pe, m->msgId.comm, m->msgId.seq);
  //fflush(stdout);
  CollMsgKey key(m->msgId.pe, m->msgId.comm, m->msgId.seq);
  CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.find(key);
  assert(it == ns->my_pe->pendingRCollMsgs.end() || it->second.size() != 0);
  if(it == ns->my_pe->pendingRCollMsgs.end() || it->second.front() == -1) {
    b->c1 = 1;
    ns->my_pe->pendingRCollMsgs[key].push_back(-1);
    //printf("%d Added recv post %d %d %d\n",  ns->my_pe_num, m->msgId.pe, m->msgId.comm, m->msgId.seq);
  } else {
    b->c2 = 1;
    assert(ns->my_pe->currentCollTask >= 0);
    Task *t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
    m->model_net_calls = 1;
    assert(ns->my_pe->currentCollSeq == m->msgId.seq);
    assert(ns->my_pe->currentCollComm == m->msgId.comm);
    int index = it->second.front();
    m->coll_info = index;
    //printf("%d Sending coll %d %d\n", ns->my_pe_num, index, m->msgId.pe);
    proc_msg m_remote, m_local;
    m_remote.proc_event_type = lookUpTable[index].remote_event;
    m_remote.src = lp->gid;
    m_remote.msgId.size = t->myEntry.msgId.size;
    m_remote.msgId.pe = t->myEntry.msgId.pe;
    m_remote.msgId.id = t->myEntry.msgId.id;
    m_remote.msgId.comm = ns->my_pe->currentCollComm;
    m_remote.msgId.seq = ns->my_pe->currentCollSeq;
    m_remote.iteration = ns->my_pe->currIter;

    m_local.proc_event_type = lookUpTable[index].local_event;
    m_local.executed.taskid = ns->my_pe->currentCollTask;
    int64_t size = t->myEntry.msgId.size;
    if((t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL && 
        t->myEntry.msgId.size <= TRACER_A2A_ALG_CUTOFF) ||
       (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER &&
        t->myEntry.msgId.size * ns->my_pe->currentCollSize <= TRACER_ALLGATHER_ALG_CUTOFF) ||
       (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_SCATTER &&
        t->myEntry.msgId.size * ns->my_pe->currentCollSize > TRACER_SCATTER_ALG_CUTOFF)) {
      size = m->msgId.size;
      m_remote.msgId.size = size;
    }
    model_net_event(net_id, "coll", pe_to_lpid(m->msgId.pe, ns->my_job), 
        size, nic_delay, sizeof(proc_msg), 
        (const void*)&m_remote, sizeof(proc_msg), &m_local, lp);
    it->second.pop_front();
    if(it->second.size() == 0) {
      ns->my_pe->pendingRCollMsgs.erase(it);
    }
  }
}

void handle_coll_recv_post_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  CollMsgKey key(m->msgId.pe, m->msgId.comm, m->msgId.seq);
  if(b->c1) {
    //printf("%d Removing recv post %d %d %d\n", ns->my_pe_num, m->msgId.pe, m->msgId.comm, m->msgId.seq);
    //fflush(stdout);
    assert(ns->my_pe->pendingRCollMsgs.find(key) != ns->my_pe->pendingRCollMsgs.end());
    ns->my_pe->pendingRCollMsgs[key].pop_back();
    if(ns->my_pe->pendingRCollMsgs[key].size() == 0) {
      ns->my_pe->pendingRCollMsgs.erase(key);
    }
  }
  if(b->c2) {
    ns->my_pe->pendingRCollMsgs[key].push_front(m->coll_info);
    model_net_event_rc2(lp, &(m->model_net_calls));
  }
}

void perform_collective(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b) {
  Task *t = &ns->my_pe->myTasks[taskid];
  assert(t->event_id == TRACER_COLL_EVT);
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[t->myEntry.msgId.comm]];
  if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST) {
    perform_bcast(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_REDUCE) {
    perform_reduction(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLREDUCE) {
    perform_allreduce(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL && 
            t->myEntry.msgId.size > TRACER_A2A_ALG_CUTOFF) {
    perform_a2a(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL && 
            t->myEntry.msgId.size <= TRACER_A2A_ALG_CUTOFF) {
    perform_bruck(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALLV) {
    perform_a2a_blocked(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER &&
            t->myEntry.msgId.size * g.members.size() > TRACER_ALLGATHER_ALG_CUTOFF) {
    perform_allgather(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER &&
            t->myEntry.msgId.size * g.members.size() <= TRACER_ALLGATHER_ALG_CUTOFF) {
    perform_bruck(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_SCATTER &&
            t->myEntry.msgId.size * g.members.size() <= TRACER_SCATTER_ALG_CUTOFF) {
    perform_scatter_small(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_SCATTER &&
            t->myEntry.msgId.size * g.members.size() > TRACER_SCATTER_ALG_CUTOFF) {
    perform_scatter(ns, taskid, lp, m, b, 0);
  } else {
    assert(0);
  }
}

void perform_collective_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b) {
  Task *t = &ns->my_pe->myTasks[taskid];
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[t->myEntry.msgId.comm]];
  if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST) {
    perform_bcast_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_REDUCE) {
    perform_reduction_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLREDUCE) {
    perform_allreduce_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL && 
            t->myEntry.msgId.size > TRACER_A2A_ALG_CUTOFF) {
    perform_a2a_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL && 
            t->myEntry.msgId.size <= TRACER_A2A_ALG_CUTOFF) {
    perform_bruck_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALLV) {
    perform_a2a_blocked_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER &&
            t->myEntry.msgId.size * g.members.size() > TRACER_ALLGATHER_ALG_CUTOFF) {
    perform_allgather_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER &&
            t->myEntry.msgId.size * g.members.size() <= TRACER_ALLGATHER_ALG_CUTOFF) {
    perform_bruck_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_SCATTER &&
            t->myEntry.msgId.size * g.members.size() <= TRACER_SCATTER_ALG_CUTOFF) {
    perform_scatter_small_rev(ns, taskid, lp, m, b, 0);
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_SCATTER &&
            t->myEntry.msgId.size * g.members.size() > TRACER_SCATTER_ALG_CUTOFF) {
    perform_scatter_rev(ns, taskid, lp, m, b, 0);
  } else {
    assert(0);
  }
}

void perform_bcast(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  Task *t;
  int recvCount;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.find(t->myEntry.msgId.comm);
    if(it == ns->my_pe->pendingCollMsgs.end()) {
      recvCount = 0;
    } else {
      std::map<int64_t, std::map<int, int> >::iterator cIt =
        it->second.find(collSeq);
      if(cIt == it->second.end()) {
        recvCount = 0;
      } else {
        assert(cIt->second.size() > 0);
        recvCount = cIt->second[0];
      }
    }
  } else {
    int comm = m->msgId.comm;
    int64_t collSeq = m->msgId.seq;
    if(comm != ns->my_pe->currentCollComm ||
       collSeq != ns->my_pe->currentCollSeq || ns->my_pe->currentCollTask == -1) {
      if(ns->my_pe->pendingCollMsgs[comm][collSeq].size()) {
        ns->my_pe->pendingCollMsgs[comm][collSeq][0]++;
      } else {
        ns->my_pe->pendingCollMsgs[comm][collSeq][0] = 1;
      }
      b->c12 = 1;
      return;
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
    recvCount = 1;
  }

  bool amIroot = (ns->my_pe->myNum == t->myEntry.msgId.pe);

  if(recvCount == 0 && !amIroot) {
    b->c13 = 1;
    return;
  }
 
  if(!isEvent && !amIroot) {
    b->c14 = 1;
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][0]--;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][0] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
  }

  int numValidChildren = 0;
  int myChildren[BCAST_DEGREE];
  int thisTreePe, index, maxSize;

  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
  std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
  if(it == g.rmembers.end()) {
    assert(0);
  } else {
    index = it->second;
  }
  maxSize = g.members.size();

  thisTreePe = (index - t->myEntry.node + maxSize) % maxSize;

  for(int i = 0; i < BCAST_DEGREE; i++) {
    int next_child = BCAST_DEGREE * thisTreePe + i + 1;
    if(next_child >= maxSize) {
      break;
    }
    myChildren[i] = (t->myEntry.node + next_child) % maxSize;
    numValidChildren++;
  }

  tw_stime delay = codes_local_latency(lp);
  tw_stime copyTime = copy_per_byte * t->myEntry.msgId.size;
  m->model_net_calls = 0;
  for(int i = 0; i < numValidChildren; i++) {
    int dest = g.members[myChildren[i]];
    send_msg(ns, t->myEntry.msgId.size, ns->my_pe->currIter,
      &t->myEntry.msgId,  ns->my_pe->currentCollSeq, pe_to_lpid(dest, ns->my_job),
      delay, COLL_BCAST, lp);
    delay += copyTime + MPI_INTERNAL_DELAY;
    m->model_net_calls++;
  }
  send_coll_comp(ns, delay, TRACER_COLLECTIVE_BCAST, lp, isEvent, m);
}

void perform_bcast_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][0]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][0] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm].erase(m->msgId.seq);
      }
      return;
    }
  }

  if(b->c13) return;
 
  if(b->c14) {
    if(ns->my_pe->pendingCollMsgs[comm][collSeq].size()) {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0]++;
    } else {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0] = 1;
    }
  }
  
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));
  send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_BCAST, lp, isEvent, m);
}

void perform_reduction(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  Task *t;
  int recvCount;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.find(t->myEntry.msgId.comm);
    if(it == ns->my_pe->pendingCollMsgs.end()) {
      recvCount = 0;
    } else {
      std::map<int64_t, std::map<int, int> >::iterator cIt =
        it->second.find(collSeq);
      if(cIt == it->second.end()) {
        recvCount = 0;
      } else {
        assert(cIt->second.size() > 0);
        recvCount = cIt->second[0];
      }
    }
  } else {
    int comm = m->msgId.comm;
    int64_t collSeq = m->msgId.seq;
    if(ns->my_pe->pendingCollMsgs[comm][collSeq].size()) {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0]++;
    } else {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0] = 1;
    }
    if(comm != ns->my_pe->currentCollComm ||
       collSeq != ns->my_pe->currentCollSeq || ns->my_pe->currentCollTask == -1) {
      b->c12 = 1;
      return;
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
    recvCount = ns->my_pe->pendingCollMsgs[comm][collSeq][0];
  }

  int numValidChildren = 0;
  int thisTreePe, index, maxSize;

  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
  std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
  if(it == g.rmembers.end()) {
    assert(0);
  } else {
    index = it->second;
  }
  maxSize = g.members.size();

  thisTreePe = (index - t->myEntry.node + maxSize) % maxSize;

  for(int i = 0; i < REDUCE_DEGREE; i++) {
    int next_child = REDUCE_DEGREE * thisTreePe + i + 1;
    if(next_child >= maxSize) {
      break;
    }
    numValidChildren++;
  }
  
  if(recvCount != numValidChildren) {
    b->c13 = 1;
    return;
  }
  
  bool amIroot = (ns->my_pe->myNum == t->myEntry.msgId.pe);
  int myParent = (thisTreePe - 1)/REDUCE_DEGREE;
  myParent = (t->myEntry.node + myParent) % maxSize;
 
  if(numValidChildren != 0) {
    b->c14 = 1;
    m->coll_info = ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][0];
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
  }

  tw_stime delay = codes_local_latency(lp);
  tw_stime copyTime = copy_per_byte * t->myEntry.msgId.size;
  m->model_net_calls = 0;
  if(!amIroot) {
    int dest = g.members[myParent];
    send_msg(ns, t->myEntry.msgId.size, ns->my_pe->currIter,
        &t->myEntry.msgId,  ns->my_pe->currentCollSeq, pe_to_lpid(dest, ns->my_job),
        delay+ nic_delay*((t->myEntry.msgId.size>16)?1:0) + MPI_INTERNAL_DELAY, COLL_REDUCTION, lp);
    m->model_net_calls++;
  }
  delay += copyTime;
  send_coll_comp(ns, delay, TRACER_COLLECTIVE_REDUCE, lp, isEvent, m);
}

void perform_reduction_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12 || b->c13) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][0]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][0] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm].erase(m->msgId.seq);
      }
      return;
    }
  }

  if(b->c13) return;
 
  if(b->c14) {
    int toInsert = m->coll_info;
    if(isEvent) toInsert--;
    if(toInsert != 0) {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0] = toInsert;
    }
  }
  
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));
  send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_REDUCE, lp, isEvent, m);
}

void perform_a2a(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  Task *t;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    int index, maxSize;
    Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
    if(it == g.rmembers.end()) {
      assert(0);
    } else {
      index = it->second;
    }
    maxSize = g.members.size();

    ns->my_pe->currentCollRank = index;
    ns->my_pe->currentCollPartner = 0;
    ns->my_pe->currentCollSize = maxSize;
    t->myEntry.msgId.pe = index;
  } else {
    if((m->msgId.pe != ns->my_pe->currentCollRank) || 
       (m->msgId.comm != ns->my_pe->currentCollComm) ||
       (m->msgId.seq != ns->my_pe->currentCollSeq)) {
      int comm = m->msgId.comm;
      int64_t collSeq = m->msgId.seq;
      ns->my_pe->pendingCollMsgs[comm][collSeq][m->msgId.pe]++;
      if(comm != ns->my_pe->currentCollComm 
          || collSeq != ns->my_pe->currentCollSeq 
          || ns->my_pe->currentCollTask == -1
          || ns->my_pe->currentCollPartner == 0) {
        b->c12 = 1;
        return;
      }
      int currSrc;
      if((ns->my_pe->currentCollSize & (ns->my_pe->currentCollSize - 1)) == 0) {
        currSrc = ns->my_pe->currentCollRank ^ ns->my_pe->currentCollPartner;
      } else {
        currSrc = ((ns->my_pe->currentCollRank - ns->my_pe->currentCollPartner 
                    + ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
      }
      if(m->msgId.pe != currSrc) {
        b->c12 = 1;
        return;
      }
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  }

  m->model_net_calls = 0;
  tw_stime delay = codes_local_latency(lp);
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];

  if(isEvent && m->msgId.pe != ns->my_pe->currentCollRank) {
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe]--;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(m->msgId.pe);
    }
  }

  if(ns->my_pe->currentCollPartner < ns->my_pe->currentCollSize - 1) {
    b->c13 = 1;
    int dest, src;
    if((ns->my_pe->currentCollSize & (ns->my_pe->currentCollSize - 1)) == 0) {
      dest = ns->my_pe->currentCollRank ^ (ns->my_pe->currentCollPartner + 1);
      src = dest;
    } else {
      dest = (ns->my_pe->currentCollRank + ns->my_pe->currentCollPartner + 1) 
        %  ns->my_pe->currentCollSize;
      src = (ns->my_pe->currentCollRank - ns->my_pe->currentCollPartner - 1
        + ns->my_pe->currentCollSize) %  ns->my_pe->currentCollSize;
    }
    assert(dest >= 0);
    assert(dest < ns->my_pe->currentCollSize);
    assert(src >= 0);
    assert(src < ns->my_pe->currentCollSize);
    dest = g.members[dest];
    m->coll_info = dest;
    tw_stime copyTime = copy_per_byte * t->myEntry.msgId.size;
    enqueue_coll_msg(TRACER_A2A, ns, t->myEntry.msgId.size, 
        ns->my_pe->currIter, &t->myEntry.msgId,  ns->my_pe->currentCollSeq, 
        dest, delay + nic_delay, copyTime, lp, m, b);
    if(t->myEntry.msgId.size > eager_limit) {
      m->model_net_calls++;
      t->myEntry.msgId.pe = ns->my_pe_num;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, 
        ns->my_pe->currentCollSeq, pe_to_lpid(g.members[src], ns->my_job), 
        delay, RECV_COLL_POST, lp);
      t->myEntry.msgId.pe = ns->my_pe->currentCollRank;
    }
    delay += copyTime + MPI_INTERNAL_DELAY;
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALLTOALL_LARGE, lp, isEvent, m);
  }
}

void perform_a2a_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  int64_t seq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = ns->my_pe->currentCollRank = 
    ns->my_pe->currentCollSize = ns->my_pe->currentCollPartner = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq].erase(m->msgId.pe);
      }
      return;
    }
  }
  
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));

  if(b->c13) {
    if(isEvent) {
       assert(ns->my_pe->currentCollTask >= 0);
       t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];  
    }
    enqueue_coll_msg_rev(TRACER_A2A, ns, &t->myEntry.msgId, seq, m->coll_info, 
      lp, m, b);
  }
  
  if(b->c15) {
    send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_ALLTOALL_LARGE, lp, isEvent, m);
  }
}

void handle_a2a_send_comp_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  int recvCount;
  if(ns->my_pe->currentCollTask == -1 ||
     (ns->my_pe->currentCollTask != m->executed.taskid) || 
     (ns->my_pe->currentCollPartner == (ns->my_pe->currentCollSize - 1))) {
    b->c13 = 1;
    return;
  }
  ns->my_pe->currentCollPartner++;
  int partner;
  if((ns->my_pe->currentCollSize & (ns->my_pe->currentCollSize - 1)) == 0) {
    partner = ns->my_pe->currentCollRank ^ ns->my_pe->currentCollPartner;
  } else {
    partner = ((ns->my_pe->currentCollRank - ns->my_pe->currentCollPartner 
          + ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
  }
  std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
    ns->my_pe->pendingCollMsgs.find(ns->my_pe->currentCollComm);
  if(it == ns->my_pe->pendingCollMsgs.end()) {
    recvCount = 0;
  } else {
    std::map<int64_t, std::map<int, int> >::iterator cIt =
      it->second.find(ns->my_pe->currentCollSeq);
    if(cIt == it->second.end()) {
      recvCount = 0;
    } else {
      std::map<int, int>::iterator c2It = cIt->second.find(partner);
      if(c2It == cIt->second.end()) {
        recvCount = 0;
      } else {
        recvCount = c2It->second;
      }
    }
  }
  assert(recvCount >= 0);
  if(recvCount != 0) {
    b->c14 = 1;
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner]--;
    m->coll_info = partner;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(partner);
    }
    //send to self
    tw_event *e = tw_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = ns->my_pe->currentCollRank;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_A2A;
    tw_event_send(e);
  }
}

void handle_a2a_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if(b->c13) return;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  ns->my_pe->currentCollPartner--;
  if(b->c14) {
    codes_local_latency_reverse(lp);
    ns->my_pe->pendingCollMsgs[comm][collSeq][m->coll_info]++;
  }
}

void perform_allreduce(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  perform_reduction(ns, taskid, lp, m, b, isEvent);
}

void perform_allreduce_rev(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  perform_reduction_rev(ns, taskid, lp, m, b, isEvent);
}

void perform_allgather(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  Task *t;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    int index, maxSize;
    Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
    if(it == g.rmembers.end()) {
      assert(0);
    } else {
      index = it->second;
    }
    maxSize = g.members.size();

    ns->my_pe->currentCollRank = index;
    ns->my_pe->currentCollPartner = 0;
    ns->my_pe->currentCollSize = maxSize;
    t->myEntry.msgId.pe = 0;
    //printf("%d New coll %d %d %d\n", ns->my_pe_num, index, ns->my_pe->currentCollComm,
     //ns->my_pe->currentCollSeq);
  } else {
    if((m->msgId.pe != 0) || 
       (m->msgId.comm != ns->my_pe->currentCollComm) ||
       (m->msgId.seq != ns->my_pe->currentCollSeq)) {
      int comm = m->msgId.comm;
      int64_t collSeq = m->msgId.seq;
      ns->my_pe->pendingCollMsgs[comm][collSeq][m->msgId.pe]++;
      if(comm != ns->my_pe->currentCollComm ||
          collSeq != ns->my_pe->currentCollSeq || ns->my_pe->currentCollTask == -1) {
        b->c12 = 1;
        return;
      }
      int currSrc = ns->my_pe->currentCollPartner;
      if(m->msgId.pe != currSrc) {
        b->c12 = 1;
        return;
      }
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  }

  m->model_net_calls = 0;
  tw_stime delay = codes_local_latency(lp);
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];

  if(isEvent && m->msgId.pe != 0) {
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe]--;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(m->msgId.pe);
    }
  }

  if(ns->my_pe->currentCollPartner < ns->my_pe->currentCollSize - 1) {
    b->c13 = 1;
    int dest, src;
    dest = (ns->my_pe->currentCollRank + 1) %  ns->my_pe->currentCollSize;
    src = (ns->my_pe->currentCollRank - 1 + ns->my_pe->currentCollSize) %  
            ns->my_pe->currentCollSize;
    dest = g.members[dest];
    m->coll_info = dest;
    tw_stime copyTime = copy_per_byte * t->myEntry.msgId.size;
    t->myEntry.msgId.pe++;
    //if(ns->my_pe_num == 23) {
    //  CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.begin();
    //  printf("%d enqueue -- %d %d -- %d %d %d \n", ns->my_pe_num,
    //  ns->my_pe->pendingRCollMsgs.size(), it->second.size(),
    //  it->first.rank, it->first.comm, it->first.seq);
    //  fflush(stdout);
    //}
    enqueue_coll_msg(TRACER_ALLGATHER, ns, t->myEntry.msgId.size, 
        ns->my_pe->currIter, &t->myEntry.msgId,  ns->my_pe->currentCollSeq, 
        dest, delay + nic_delay, copyTime, lp, m, b);
    if(t->myEntry.msgId.size > eager_limit) {
      m->model_net_calls++;
      int saved_pe = t->myEntry.msgId.pe;
      t->myEntry.msgId.pe = ns->my_pe_num;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, 
        ns->my_pe->currentCollSeq, pe_to_lpid(g.members[src], ns->my_job), 
        delay, RECV_COLL_POST, lp);
      t->myEntry.msgId.pe = saved_pe;
      //printf("%d Send MSG to %d %d %lld\n", ns->my_pe_num, src, g.members[src], ns->my_pe->currentCollSeq);
    }
    //if(ns->my_pe_num == 23) {
    //  CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.begin();
    //  printf("%d after enqueue -- %d %d -- %d %d %d \n", ns->my_pe_num,
    //  ns->my_pe->pendingRCollMsgs.size(), it->second.size(),
    //  it->first.rank, it->first.comm, it->first.seq);
    //  fflush(stdout);
    //}
    delay += copyTime + MPI_INTERNAL_DELAY;
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALLGATHER_LARGE, lp, isEvent, m);
  }
}

void perform_allgather_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  int64_t seq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = ns->my_pe->currentCollRank = 
    ns->my_pe->currentCollSize = ns->my_pe->currentCollPartner = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq].erase(m->msgId.pe);
      }
      return;
    }
  }
  
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));

  if(b->c13) {
    if(isEvent) {
       t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];  
    }
    t->myEntry.msgId.pe--;
    enqueue_coll_msg_rev(TRACER_ALLGATHER, ns, &t->myEntry.msgId, seq, 
      m->coll_info, lp, m, b);
  }
  
  if(b->c15) {
    send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_ALLGATHER_LARGE, lp, isEvent, m);
  }
}

void handle_allgather_send_comp_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if(ns->my_pe->currentCollTask == -1 ||
     (ns->my_pe->currentCollTask != m->executed.taskid) || 
     (ns->my_pe->currentCollPartner == (ns->my_pe->currentCollSize - 1))) {
    b->c13 = 1;
    return;
  }
  int recvCount;
  ns->my_pe->currentCollPartner++;
  int partner = ns->my_pe->currentCollPartner;
  std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
    ns->my_pe->pendingCollMsgs.find(ns->my_pe->currentCollComm);
  //printf("%d Coll send complete %d %d %d\n", ns->my_pe_num, ns->my_pe->currentCollComm, 
  //  ns->my_pe->currentCollSeq, partner);
  if(it == ns->my_pe->pendingCollMsgs.end()) {
    recvCount = 0;
  } else {
    std::map<int64_t, std::map<int, int> >::iterator cIt =
      it->second.find(ns->my_pe->currentCollSeq);
    if(cIt == it->second.end()) {
      recvCount = 0;
    } else {
      std::map<int, int>::iterator c2It = cIt->second.find(partner);
      if(c2It == cIt->second.end()) {
        recvCount = 0;
      } else {
        recvCount = c2It->second;
      }
    }
  }
  assert(recvCount >= 0);
  if(recvCount != 0) {
    b->c14 = 1;
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner]--;
    m->coll_info = partner;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(partner);
    }
    //send to self
    tw_event *e = tw_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = 0;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_ALLGATHER;
    tw_event_send(e);
  }
}

void handle_allgather_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if(b->c13) return;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  ns->my_pe->currentCollPartner--;
  if(b->c14) {
    codes_local_latency_reverse(lp);
    ns->my_pe->pendingCollMsgs[comm][collSeq][m->coll_info]++;
  }
}

void perform_bruck(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  Task *t;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    int index, maxSize;
    Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
    if(it == g.rmembers.end()) {
      assert(0);
    } else {
      index = it->second;
    }
    maxSize = g.members.size();

    ns->my_pe->currentCollRank = index;
    ns->my_pe->currentCollPartner = 0;
    ns->my_pe->currentCollSize = maxSize;
    if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL) {
      ns->my_pe->currentCollMsgSize = t->myEntry.msgId.size * maxSize/2;
    } else if (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER) {
      ns->my_pe->currentCollMsgSize = t->myEntry.msgId.size;
    } else {
      assert(0);
    }
    t->myEntry.msgId.pe = index;
  } else {
    if((m->msgId.pe != ns->my_pe->currentCollRank) || 
       (m->msgId.comm != ns->my_pe->currentCollComm) ||
       (m->msgId.seq != ns->my_pe->currentCollSeq)) {
      int comm = m->msgId.comm;
      int64_t collSeq = m->msgId.seq;
      ns->my_pe->pendingCollMsgs[comm][collSeq][m->msgId.pe]++;
      if(comm != ns->my_pe->currentCollComm 
          || collSeq != ns->my_pe->currentCollSeq 
          || ns->my_pe->currentCollTask == -1
          || ns->my_pe->currentCollPartner == 0) {
        b->c12 = 1;
        return;
      }
      int currSrc;
      t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
      if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL) {
        currSrc = ((ns->my_pe->currentCollRank - ns->my_pe->currentCollPartner 
            + ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
      } else if (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER) {
        currSrc = ((ns->my_pe->currentCollRank + ns->my_pe->currentCollPartner) 
            % ns->my_pe->currentCollSize);
      } else {
        assert(0);
      }
      if(m->msgId.pe != currSrc) {
        b->c12 = 1;
        return;
      }
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  }

  m->model_net_calls = 0;
  tw_stime delay = codes_local_latency(lp);
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];

  if(isEvent && m->msgId.pe != ns->my_pe->currentCollRank) {
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe]--;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(m->msgId.pe);
    }
  }

  if(ns->my_pe->currentCollPartner < (ns->my_pe->currentCollSize/2)) {
    b->c13 = 1;
    int dest, src;
    int partner = 2*ns->my_pe->currentCollPartner;
    if(partner == 0) partner = 1;
    if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL) {
      src = ((ns->my_pe->currentCollRank - partner 
            + ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
      dest = ((ns->my_pe->currentCollRank + partner) 
          % ns->my_pe->currentCollSize);
    } else if (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER) {
      src = ((ns->my_pe->currentCollRank + partner) 
          % ns->my_pe->currentCollSize);
      dest = ((ns->my_pe->currentCollRank - partner 
            + ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
    } else {
      assert(0);
    }
    assert(dest >= 0);
    assert(dest < ns->my_pe->currentCollSize);
    assert(src >= 0);
    assert(src < ns->my_pe->currentCollSize);
    dest = g.members[dest];
    m->coll_info = dest;
    tw_stime copyTime = copy_per_byte * ns->my_pe->currentCollMsgSize;
    enqueue_coll_msg(TRACER_BRUCK, ns, ns->my_pe->currentCollMsgSize,
        ns->my_pe->currIter, &t->myEntry.msgId,  ns->my_pe->currentCollSeq, 
        dest, delay + nic_delay, copyTime, lp, m, b);
    if(ns->my_pe->currentCollMsgSize > eager_limit) {
      m->model_net_calls++;
      t->myEntry.msgId.pe = ns->my_pe_num;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, 
        ns->my_pe->currentCollSeq, pe_to_lpid(g.members[src], ns->my_job), 
        delay, RECV_COLL_POST, lp, true,  ns->my_pe->currentCollMsgSize);
      t->myEntry.msgId.pe = ns->my_pe->currentCollRank;
    }
    delay += copyTime + MPI_INTERNAL_DELAY;
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALL_BRUCK, lp, isEvent, m);
  }
}

void perform_bruck_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  int64_t seq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = ns->my_pe->currentCollRank = 
    ns->my_pe->currentCollSize = ns->my_pe->currentCollPartner = 
    ns->my_pe->currentCollMsgSize = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq].erase(m->msgId.pe);
      }
      return;
    }
  }
  
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));

  if(b->c13) {
    if(isEvent) {
       assert(ns->my_pe->currentCollTask >= 0);
       t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];  
    }
    enqueue_coll_msg_rev(TRACER_BRUCK, ns, &t->myEntry.msgId, seq, m->coll_info, 
      lp, m, b);
  }
  
  if(b->c15) {
    send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_ALL_BRUCK, lp, isEvent, m);
  }
}

void handle_bruck_send_comp_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  int recvCount;
  if(ns->my_pe->currentCollTask == -1 ||
     (ns->my_pe->currentCollTask != m->executed.taskid) || 
     (ns->my_pe->currentCollPartner >= (ns->my_pe->currentCollSize/2))) {
    b->c13 = 1;
    return;
  }
  if(ns->my_pe->currentCollPartner == 0) {
     ns->my_pe->currentCollPartner = 1;
  } else {
    ns->my_pe->currentCollPartner *= 2;
  }
  int partner;
  Task *t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL) {
    partner = ((ns->my_pe->currentCollRank - ns->my_pe->currentCollPartner 
          + ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
  } else if (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER) {
    partner = ((ns->my_pe->currentCollRank + ns->my_pe->currentCollPartner) 
        % ns->my_pe->currentCollSize);
    ns->my_pe->currentCollMsgSize *= 2;
  } else {
    assert(0);
  }
  std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
    ns->my_pe->pendingCollMsgs.find(ns->my_pe->currentCollComm);
  if(it == ns->my_pe->pendingCollMsgs.end()) {
    recvCount = 0;
  } else {
    std::map<int64_t, std::map<int, int> >::iterator cIt =
      it->second.find(ns->my_pe->currentCollSeq);
    if(cIt == it->second.end()) {
      recvCount = 0;
    } else {
      std::map<int, int>::iterator c2It = cIt->second.find(partner);
      if(c2It == cIt->second.end()) {
        recvCount = 0;
      } else {
        recvCount = c2It->second;
      }
    }
  }
  assert(recvCount >= 0);
  if(recvCount != 0) {
    b->c14 = 1;
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner]--;
    m->coll_info = partner;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(partner);
    }
    //send to self
    tw_event *e = tw_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = ns->my_pe->currentCollRank;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_BRUCK;
    tw_event_send(e);
  }
}

void handle_bruck_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if(b->c13) return;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  ns->my_pe->currentCollPartner /= 2;
  Task *t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  if (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER) {
    ns->my_pe->currentCollMsgSize /= 2;
  }
  if(b->c14) {
    codes_local_latency_reverse(lp);
    ns->my_pe->pendingCollMsgs[comm][collSeq][m->coll_info]++;
  }
}

void perform_a2a_blocked(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  Task *t;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    int index, maxSize;
    Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
    if(it == g.rmembers.end()) {
      assert(0);
    } else {
      index = it->second;
    }
    maxSize = g.members.size();

    ns->my_pe->currentCollRank = index;
    ns->my_pe->currentCollPartner = 0;
    ns->my_pe->currentCollSize = maxSize;
    ns->my_pe->currentCollRecvCount = 0;
    ns->my_pe->currentCollSendCount = 0;
    t->myEntry.msgId.pe = index;
  } else {
    if((m->msgId.pe != ns->my_pe->currentCollRank) || 
       (m->msgId.comm != ns->my_pe->currentCollComm) ||
       (m->msgId.seq != ns->my_pe->currentCollSeq)) {
      int comm = m->msgId.comm;
      int64_t collSeq = m->msgId.seq;
      ns->my_pe->pendingCollMsgs[comm][collSeq][m->msgId.pe]++;
      if(comm != ns->my_pe->currentCollComm 
          || collSeq != ns->my_pe->currentCollSeq 
          || ns->my_pe->currentCollTask == -1
          || ns->my_pe->currentCollPartner == 0) {
        b->c12 = 1;
        return;
      }
      //TODO make efficient
      bool done = false;
      int start = ns->my_pe->currentCollPartner - TRACER_BLOCK_SIZE + 1;
      if((ns->my_pe->currentCollPartner == ns->my_pe->currentCollSize - 1) && (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE != 0)) {
        start = ns->my_pe->currentCollPartner - 
                (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE) + 1;
      }
      assert(start >= 1);
      for(int i = start; i <= ns->my_pe->currentCollPartner; i++) {
        int currSrc = ((ns->my_pe->currentCollRank - i + 
                      ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
        if(currSrc == ns->my_pe->currentCollRank) break;
        if(m->msgId.pe == currSrc) {
          b->c18 = 1;
          ns->my_pe->currentCollRecvCount++;
          ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe]--;
          if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe] == 0) {
            ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(m->msgId.pe);
          }
          if((ns->my_pe->currentCollRecvCount % TRACER_BLOCK_SIZE == 0) ||
             (ns->my_pe->currentCollRecvCount == ns->my_pe->currentCollSize - 1)) {
            done = true;
          }
          break;
        }
      }
      if(!done) {
        b->c12 = 1;
        return;
      }
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  }

  m->model_net_calls = 0;
  tw_stime delay = codes_local_latency(lp);
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];

  if(ns->my_pe->currentCollPartner < ns->my_pe->currentCollSize - 1) {
    b->c13 = 1;
    for(int i = 1; i <= TRACER_BLOCK_SIZE; i++) {
      int dest;
      dest = (ns->my_pe->currentCollRank + ns->my_pe->currentCollPartner + i) 
        %  ns->my_pe->currentCollSize;
      assert(dest >= 0);
      assert(dest < g.members.size());
      if(dest == ns->my_pe->currentCollRank) break;
      dest = g.members[dest];
      tw_stime copyTime = copy_per_byte * t->myEntry.msgId.size;
      enqueue_coll_msg(TRACER_BLOCKED, ns, t->myEntry.msgId.size, 
          ns->my_pe->currIter, &t->myEntry.msgId,  ns->my_pe->currentCollSeq, 
          dest, delay + nic_delay, copyTime, lp, m, b, true);
      delay += copyTime + MPI_INTERNAL_DELAY;
    }
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALLTOALL_BLOCKED, lp, isEvent, m);
  }
}

void perform_a2a_blocked_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  int64_t seq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = ns->my_pe->currentCollRank = 
    ns->my_pe->currentCollSize = ns->my_pe->currentCollPartner = 
    ns->my_pe->currentCollSendCount = ns->my_pe->currentCollRecvCount = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c18) {
      ns->my_pe->currentCollRecvCount--;
    }
    if(b->c12) {
      if(!b->c18) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe]--;
        if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe] == 0) {
          ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq].erase(m->msgId.pe);
        }
      } 
      return;
    }
  }
  
  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));

  if(b->c15) {
    send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_ALLTOALL_BLOCKED, lp, isEvent, m);
  }
}

void handle_a2a_blocked_send_comp_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  int recvCount;
  if(ns->my_pe->currentCollTask == -1 ||
     (ns->my_pe->currentCollTask != m->executed.taskid) || 
     (ns->my_pe->currentCollPartner == (ns->my_pe->currentCollSize - 1))) {
    b->c13 = 1;
    return;
  }

  int nextPartner = ns->my_pe->currentCollPartner + TRACER_BLOCK_SIZE;
  if(nextPartner > ns->my_pe->currentCollSize - 1) {
    nextPartner = ns->my_pe->currentCollSize - 1;
  }

  ns->my_pe->currentCollSendCount++;
  if(ns->my_pe->currentCollSendCount < nextPartner) {
    b->c15 = 1;
    return;
  }

  ns->my_pe->currentCollPartner = nextPartner;

  m->coll_info = 0;
  unsigned int bitSet = 1;
  int start = ns->my_pe->currentCollPartner - TRACER_BLOCK_SIZE + 1;
  if((ns->my_pe->currentCollPartner == ns->my_pe->currentCollSize - 1) && (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE != 0)) {
    start = ns->my_pe->currentCollPartner - 
      (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE) + 1;
  }

  for(int i = start; i <= ns->my_pe->currentCollPartner; i++) {
    int partner = ((ns->my_pe->currentCollRank - i + 
          ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
    if(partner == ns->my_pe->currentCollRank) break;

    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.find(ns->my_pe->currentCollComm);
    if(it == ns->my_pe->pendingCollMsgs.end()) {
      recvCount = 0;
    } else {
      std::map<int64_t, std::map<int, int> >::iterator cIt =
        it->second.find(ns->my_pe->currentCollSeq);
      if(cIt == it->second.end()) {
        recvCount = 0;
      } else {
        std::map<int, int>::iterator c2It = cIt->second.find(partner);
        if(c2It == cIt->second.end()) {
          recvCount = 0;
        } else {
          recvCount = c2It->second;
        }
      }
    }
    if(recvCount != 0) {
      m->coll_info = m->coll_info | bitSet;
      ns->my_pe->currentCollRecvCount++;
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner]--;
      if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][partner] == 0) {
        ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(partner);
      }
    }
    bitSet = bitSet << 1;
  }
  if(ns->my_pe->currentCollRecvCount == ns->my_pe->currentCollPartner) {
    //send to self
    b->c14 = 1;
    tw_event *e = tw_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = ns->my_pe->currentCollRank;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_A2A_BLOCKED;
    tw_event_send(e);
  }
}

void handle_a2a_blocked_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if(b->c13) return;
  ns->my_pe->currentCollSendCount--;
  if(b->c15) return;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  if(m->coll_info) {
    unsigned int bitSet = 1;
    int start = ns->my_pe->currentCollPartner - TRACER_BLOCK_SIZE + 1;
    if((ns->my_pe->currentCollPartner == ns->my_pe->currentCollSize - 1) && (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE != 0)) {
      start = ns->my_pe->currentCollPartner - 
        (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE) + 1;
    }
    for(int i = start; i <= ns->my_pe->currentCollPartner; i++) {
      if(m->coll_info & bitSet) {
        int partner = ((ns->my_pe->currentCollRank - i + 
              ns->my_pe->currentCollSize) % ns->my_pe->currentCollSize);
        if(partner == ns->my_pe->currentCollRank) break;
        ns->my_pe->pendingCollMsgs[comm][collSeq][partner]++;
        ns->my_pe->currentCollRecvCount--;
      }
      bitSet = bitSet << 1;
    }
  }
  if((ns->my_pe->currentCollPartner == ns->my_pe->currentCollSize - 1) && (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE != 0)) {
    ns->my_pe->currentCollPartner -= (ns->my_pe->currentCollPartner % TRACER_BLOCK_SIZE);
  } else {
    ns->my_pe->currentCollPartner -= TRACER_BLOCK_SIZE;
  }
  if(b->c14) {
    codes_local_latency_reverse(lp);
  }
}

void perform_scatter_small(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent)
{
  Task *t;
  int recvCount;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;
    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.find(t->myEntry.msgId.comm);
    if(it == ns->my_pe->pendingCollMsgs.end()) {
      recvCount = 0;
    } else {
      std::map<int64_t, std::map<int, int> >::iterator cIt =
        it->second.find(collSeq);
      if(cIt == it->second.end()) {
        recvCount = 0;
      } else {
        assert(cIt->second.size() > 0);
        recvCount = cIt->second[0];
      }
    }
  } else {
      int comm = m->msgId.comm;
      int64_t collSeq = m->msgId.seq;
      if((comm != ns->my_pe->currentCollComm) ||
         (collSeq != ns->my_pe->currentCollSeq) ||
         (ns->my_pe->currentCollTask == -1)) {
        if(ns->my_pe->pendingCollMsgs[comm][collSeq].size()) {
          ns->my_pe->pendingCollMsgs[comm][collSeq][0]++;
        } else {
          ns->my_pe->pendingCollMsgs[comm][collSeq][0] = 1;
        }
        b->c12 = 1;
        return;
      }
      t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
      recvCount = 1;
  }

  bool amIroot = (ns->my_pe->myNum == t->myEntry.msgId.pe);

  if(recvCount == 0 && !amIroot) {
    b->c13 = 1;
    return;
  }

  if(!isEvent && !amIroot) {
    b->c14 = 1;
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][0]--;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][0] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
  }

  int thisTreePe, index, maxSize;

  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
  std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe_num);
  if(it == g.rmembers.end()) {
    assert(0);
  } else {
    index = it->second;
  }
  maxSize = g.members.size();

  thisTreePe = (index - t->myEntry.node + maxSize) % maxSize;

  tw_stime delay = codes_local_latency(lp);
  m->model_net_calls = 0;

  int mask;
  for(mask = 0x1; mask < maxSize; mask <<= 1) {
    if (thisTreePe & mask) {
      break;
    }
  }
  mask >>= 1;
  int remainingSize = amIroot ? (t->myEntry.msgId.size * maxSize) : m->msgId.size;
  while(mask > 0) {
    if(thisTreePe + mask < maxSize) {
      int dest = g.members[(t->myEntry.node + thisTreePe + mask) % maxSize];
      int sendSize = remainingSize - (t->myEntry.msgId.size * mask);
      send_msg(ns, sendSize, ns->my_pe->currIter,
        &t->myEntry.msgId,  ns->my_pe->currentCollSeq, pe_to_lpid(dest, ns->my_job),
        delay, COLL_SCATTER_SMALL, lp);
      delay += copy_per_byte * sendSize;
      m->model_net_calls++;
      remainingSize -= sendSize;
    }
    mask >>= 1;
  }
  send_coll_comp(ns, delay, TRACER_COLLECTIVE_SCATTER_SMALL, lp, isEvent, m);
}

void perform_scatter_small_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent)
{
  Task *t;
  int comm = ns->my_pe->currentCollComm;
  int64_t collSeq = ns->my_pe->currentCollSeq;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = -1;
    ns->my_pe->currentCollTask = -1;
    ns->my_pe->currentCollSeq = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][0]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][0] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm].erase(m->msgId.seq);
      }
      return;
    }
  }

  if(b->c13) {
    return;
  }

  if(b->c14) {
    if(ns->my_pe->pendingCollMsgs[comm][collSeq].size()) {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0]++;
    } else {
      ns->my_pe->pendingCollMsgs[comm][collSeq][0] = 1;
    }
  }

  codes_local_latency_reverse(lp);
  model_net_event_rc2(lp, &(m->model_net_calls));
  send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_SCATTER_SMALL, lp, isEvent, m);
}

/* Implementation of scatter using binomial tree:
 * Recurvsive trees rooted at logical rank 0 which sends to 2^n, 2^n-1, 2^n-2...
 * Each of those receipients than send to nodes in their subtrees
 *
*/
void perform_scatter(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent)
{
  Task *t;
  int recvCount;
  m->model_net_calls = 0;
  if(!isEvent) {
    PE_set_busy(ns->my_pe, true);
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = t->myEntry.msgId.comm;
    ns->my_pe->currentCollTask = taskid;
    int64_t collSeq = ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]++;
    ns->my_pe->currentCollSeq = collSeq;

    int index, maxSize;
    Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it1 = g.rmembers.find(ns->my_pe_num);
    if(it1 == g.rmembers.end()) {
      assert(0);
    } else {
      index = it1->second;
    }
    maxSize = g.members.size();

    //my logical rank in the tree
    ns->my_pe->currentCollRank = (index - t->myEntry.node + maxSize) % maxSize;
    //this gets sent with the msg to identify the source
    t->myEntry.msgId.pe = ns->my_pe->currentCollRank;
    ns->my_pe->currentCollSize = maxSize;

    int partner;
    //distance on left of partner from which I receive the first message: 0 is the
    //root and sends to 2^n, 2^n-1, and so on
    for(partner = 0x1; partner < maxSize; partner <<= 1) {
      if (ns->my_pe->currentCollRank & partner) {
        break;
      }
    }
    //distance of the partner on the right to which I send next
    ns->my_pe->currentCollPartner = partner >> 1;
    //logical rank in the tree from which i expect to receive
    partner = (ns->my_pe->currentCollRank - partner + maxSize) % maxSize;

    int size_mult = 2 * ns->my_pe->currentCollPartner;
    if(size_mult == 0) {
      size_mult = 1;
    }
    if(ns->my_pe->currentCollRank + size_mult > maxSize) {
      size_mult = maxSize - ns->my_pe->currentCollRank;
    }
    ns->my_pe->currentCollMsgSize = t->myEntry.msgId.size * size_mult;
    
    tw_stime delay = codes_local_latency(lp);
    if(ns->my_pe->currentCollRank != 0 && ns->my_pe->currentCollMsgSize > eager_limit) {
      m->model_net_calls++;
      //POST messages are matched on global rank
      t->myEntry.msgId.pe = ns->my_pe_num;
      int src = (t->myEntry.node + partner) % ns->my_pe->currentCollSize;
      src = g.members[src];
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId,
        ns->my_pe->currentCollSeq, pe_to_lpid(src, ns->my_job),
        delay, RECV_COLL_POST, lp, true, ns->my_pe->currentCollMsgSize);
      //Reset the task PE to the rank in current comm
      t->myEntry.msgId.pe = ns->my_pe->currentCollRank;
    }

    //check if message received already
    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.find(ns->my_pe->currentCollComm);
    if(it == ns->my_pe->pendingCollMsgs.end()) {
      recvCount = 0;
    } else {
      std::map<int64_t, std::map<int, int> >::iterator cIt =
        it->second.find(ns->my_pe->currentCollSeq);
      if(cIt == it->second.end()) {
        recvCount = 0;
      } else {
        std::map<int, int>::iterator c2It = cIt->second.find(partner);
        if(c2It == cIt->second.end()) {
          recvCount = 0;
        } else {
          recvCount = c2It->second;
        }
      }
    }
    assert(recvCount >= 0);
  } else {
    int comm = m->msgId.comm;
    int64_t collSeq = m->msgId.seq;
    //if not a self message
    if((m->msgId.pe != ns->my_pe->currentCollRank) ||
       (comm != ns->my_pe->currentCollComm) ||
       (collSeq != ns->my_pe->currentCollSeq)) {
      ns->my_pe->pendingCollMsgs[comm][collSeq][m->msgId.pe]++;
      //if not reached the MPI task yet, return
      if((comm != ns->my_pe->currentCollComm) ||
         (collSeq != ns->my_pe->currentCollSeq) ||
         (ns->my_pe->currentCollTask == -1)) {
        b->c12 = 1;
        return;
      }
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
    if(m->msgId.pe != ns->my_pe->currentCollRank) {
      b->c13 = 1;
      ns->my_pe->currentCollMsgSize = m->msgId.size;
    }
    recvCount = 1;
  }

  //if not root, and haven't received a message, return
  if((recvCount == 0) && (ns->my_pe->currentCollRank != 0)) {
    b->c14 = 1;
    return;
  }

  //if non-self message, then pendingCollMsgs need to be updated
  if(isEvent && (m->msgId.pe != ns->my_pe->currentCollRank)) {
    ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe]--;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq][m->msgId.pe] == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].erase(m->msgId.pe);
    }
  }

  tw_stime delay = codes_local_latency(lp);
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];

  //scan till find a destination that exist
  m->coll_info_2 = ns->my_pe->currentCollPartner;
  while((ns->my_pe->currentCollPartner > 0) &&
     (ns->my_pe->currentCollRank + ns->my_pe->currentCollPartner >= ns-> my_pe->currentCollSize)) {
    ns->my_pe->currentCollPartner >>= 1;
  }

  if(ns->my_pe->currentCollPartner > 0) {
    b->c15 = 1;
    //destination is shifted by the root's rank to look up the comm map
    int dest = (t->myEntry.node + ns->my_pe->currentCollRank + ns->my_pe->currentCollPartner) % ns->my_pe->currentCollSize;
    dest = g.members[dest];
    m->coll_info = dest;
    //temporary storage
    m->msgId.size = ns->my_pe->currentCollMsgSize;
    //this is the amount of data we need to send next
    ns->my_pe->currentCollMsgSize -= (t->myEntry.msgId.size * ns->my_pe->currentCollPartner);
    tw_stime copyTime = copy_per_byte * ns->my_pe->currentCollMsgSize;
    enqueue_coll_msg(TRACER_SCATTER, ns, ns->my_pe->currentCollMsgSize,
        ns->my_pe->currIter, &t->myEntry.msgId,  ns->my_pe->currentCollSeq,
        dest, delay + nic_delay + soft_delay_mpi, copyTime, lp, m, b);
    delay += copyTime;
    //left over data after the previous send
    ns->my_pe->currentCollMsgSize = m->msgId.size - ns->my_pe->currentCollMsgSize;
  } else {
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_SCATTER, lp, isEvent, m);
  }
}

void perform_scatter_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent)
{
  Task *t;
  int64_t seq = ns->my_pe->currentCollSeq;
  model_net_event_rc2(lp, &(m->model_net_calls));

  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = -1;
    ns->my_pe->currentCollTask = -1;
    ns->my_pe->currentCollSeq = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;

    ns->my_pe->currentCollRank = -1;
    ns->my_pe->currentCollSize = -1;
    ns->my_pe->currentCollMsgSize = -1;
    ns->my_pe->currentCollPartner = -1;
    codes_local_latency_reverse(lp);
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe]--;
      if(ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq][m->msgId.pe] == 0) {
        ns->my_pe->pendingCollMsgs[m->msgId.comm][m->msgId.seq].erase(m->msgId.pe);
      }
      return;
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
    if(b->c13) {
      ns->my_pe->currentCollMsgSize = -1;
    }
  }

  if(b->c14) {
    return;
  }

  codes_local_latency_reverse(lp);
  ns->my_pe->currentCollPartner = m->coll_info_2;
  if(b->c15) {
    ns->my_pe->currentCollMsgSize = m->msgId.size;
    enqueue_coll_msg_rev(TRACER_SCATTER, ns, &t->myEntry.msgId, seq, m->coll_info, lp, m, b);
  } else {
    send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_SCATTER, lp, isEvent, m);
  }
}

void handle_scatter_send_comp_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if((ns->my_pe->currentCollTask == -1) ||
     (ns->my_pe->currentCollTask != m->executed.taskid) ||
     (ns->my_pe->currentCollPartner <= 0)) {
    b->c18 = 1;
    return;
  }
  //move to the next destination by reducing the distance
  ns->my_pe->currentCollPartner >>= 1;
  //send to self
  tw_event *e = tw_event_new_bounded(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
  proc_msg *m_new = (proc_msg*)tw_event_data(e);
  m_new->msgId.pe = ns->my_pe->currentCollRank;
  m_new->msgId.comm = ns->my_pe->currentCollComm;
  m_new->msgId.seq = ns->my_pe->currentCollSeq;
  m_new->msgId.size = m->msgId.size;
  m_new->proc_event_type = COLL_SCATTER;
  tw_event_send(e);
}

void handle_scatter_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  if(b->c18) {
    return;
  }
  if(ns->my_pe->currentCollPartner == 0) {
    ns->my_pe->currentCollPartner = 0x1;
  } else {
    ns->my_pe->currentCollPartner <<= 1;
  }
  codes_local_latency_reverse(lp);
}

void handle_coll_complete_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {
  if(ns->my_pe->currentCollSeq == -1) {
    b->c3 = 1;
    return;
  }
  Task *t = &ns->my_pe->myTasks[m->executed.taskid];
  //printf("%d coll complete %d %d\n", ns->my_pe_num, ns->my_pe->currentCollComm,
  //    ns->my_pe->currentCollSeq);
  m->msgId.seq = ns->my_pe->currentCollSeq;
  m->msgId.comm = ns->my_pe->currentCollComm;
  m->coll_info = ns->my_pe->currentCollRank;
  ns->my_pe->currentCollComm = ns->my_pe->currentCollSeq = 
  ns->my_pe->currentCollRank = ns->my_pe->currentCollSize = -1;
  ns->my_pe->currentCollRecvCount = ns->my_pe->currentCollSendCount = -1;
  m->msgId.size = ns->my_pe->currentCollMsgSize;
  m->fwd_dep_count = ns->my_pe->currentCollPartner;
  ns->my_pe->currentCollMsgSize = ns->my_pe->currentCollPartner = -1;
  if((t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST) ||
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_REDUCE) || 
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLREDUCE &&
      m->msgId.coll_type == TRACER_COLLECTIVE_BCAST) ||
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALL) ||
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLGATHER) ||
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALLV) ||
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_SCATTER)
    ) {
    b->c1 = 1;
    exec_comp(ns, ns->my_pe->currIter, m->executed.taskid, 0,
      soft_delay_mpi + codes_local_latency(lp), 0, lp);

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

    if(ns->my_pe_num == 0 && (ns->my_pe->currentTask % print_frequency == 0)) {
      char str[1000];
      strcpy(str, "[ %d %d : time at task %d %f ]\n");
      tw_output(lp, str, ns->my_job, ns->my_pe_num, 
          ns->my_pe->currentTask, tw_now(lp)/((double)TIME_MULT));
    }
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLREDUCE &&
      m->msgId.coll_type == TRACER_COLLECTIVE_REDUCE) {
    b->c2 = 1;
    perform_bcast(ns, m->executed.taskid, lp, m, b, 0);
  }
}

void handle_coll_complete_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {
  if(b->c3) {
    return;
  }
  if(b->c2) {
    perform_bcast_rev(ns, m->executed.taskid, lp, m, b, 0);
  }
  ns->my_pe->currentCollSeq = m->msgId.seq;
  ns->my_pe->currentCollComm = m->msgId.comm;
  ns->my_pe->currentCollRank = m->coll_info;
  Task *t = &ns->my_pe->myTasks[m->executed.taskid];
  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
  if(m->msgId.coll_type == TRACER_COLLECTIVE_ALLTOALL_LARGE || 
     m->msgId.coll_type == TRACER_COLLECTIVE_ALLGATHER_LARGE || 
     m->msgId.coll_type == TRACER_COLLECTIVE_ALL_BRUCK ||
     m->msgId.coll_type == TRACER_COLLECTIVE_ALLTOALL_BLOCKED ||
     m->msgId.coll_type == TRACER_COLLECTIVE_SCATTER) {
    ns->my_pe->currentCollSize = g.members.size();
    ns->my_pe->currentCollPartner = m->fwd_dep_count;
    if(m->msgId.coll_type == TRACER_COLLECTIVE_ALL_BRUCK ||
       m->msgId.coll_type == TRACER_COLLECTIVE_SCATTER) {
      ns->my_pe->currentCollMsgSize = m->msgId.size;
    }
    if(m->msgId.coll_type == TRACER_COLLECTIVE_ALLTOALL_BLOCKED) {
      ns->my_pe->currentCollSendCount = ns->my_pe->currentCollRecvCount = 
        g.members.size() - 1;
    }
  }
  if(b->c1) {
    codes_local_latency_reverse(lp);
  }
}

int send_coll_comp(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg *m)
{
    tw_event *e;
    proc_msg *msg;
    
    int taskid = ns->my_pe->currentCollTask;
    m->executed.taskid = ns->my_pe->currentCollTask;
    ns->my_pe->currentCollTask = -1;

    if(sendOffset < g_tw_lookahead) {
      sendOffset += g_tw_lookahead;
    }
    e = tw_event_new(lp->gid, sendOffset + soft_delay_mpi, lp);
    msg = (proc_msg*)tw_event_data(e);
    msg->msgId.coll_type = collType;
    msg->proc_event_type = COLL_COMPLETE;
    msg->executed.taskid = taskid;
    tw_event_send(e);
    return 0;
}

int send_coll_comp_rev(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg *m)
{
  if(isEvent) ns->my_pe->currentCollTask = m->executed.taskid;
  return 0;
}
#endif

//Creates and initiates bcast_msg
int bcast_msg(
        proc_state * ns,
        int size,
        int iter,
        MsgID *msgId,
        tw_stime sendOffset,
        tw_stime copyTime,
        tw_lp * lp,
        proc_msg *m) {

    int numValidChildren = 0;
    int myChildren[BCAST_DEGREE];
    int thisTreePe = (ns->my_pe_num - msgId->pe + jobs[ns->my_job].numRanks) %
                      jobs[ns->my_job].numRanks;

    for(int i = 0; i < BCAST_DEGREE; i++) {
      int next_child = BCAST_DEGREE * thisTreePe + i + 1;
      if(next_child >= jobs[ns->my_job].numRanks) {
        break;
      }
      myChildren[i] = (msgId->pe + next_child) % jobs[ns->my_job].numRanks;
      numValidChildren++;
    }
    
    tw_stime delay = copyTime;

    for(int i = 0; i < numValidChildren; i++) {
      send_msg(ns, size, iter, msgId,  0 /*not used */, 
        pe_to_lpid(myChildren[i], ns->my_job), sendOffset + delay, BCAST, lp);
      delay += copyTime + MPI_INTERNAL_DELAY;
      m->model_net_calls++;
    }
    return numValidChildren;
}

int exec_comp(
    proc_state * ns,
    int iter,
    int task_id,
    int comm_id,
    tw_stime sendOffset,
    int recv,
    tw_lp * lp)
{
    //If it's a self event use tw_event_new instead of model_net_event 
    tw_event *e;
    proc_msg *m;

    if(sendOffset < g_tw_lookahead) {
      sendOffset += g_tw_lookahead;
    }
    e = tw_event_new(lp->gid, sendOffset, lp);
    m = (proc_msg*)tw_event_data(e);
    m->msgId.size = 0;
    m->msgId.pe = ns->my_pe_num;
    m->msgId.id = task_id;
#if TRACER_OTF_TRACES
    m->msgId.comm = comm_id;
    if(recv) {
      m->msgId.seq = ns->my_pe->sendSeq[ns->my_pe_num]++;
    }
#endif
    m->iteration = iter;
    if(recv) {
        m->proc_event_type = RECV_MSG;
#if DEBUG_PRINT
        if(ns->my_pe_num ==  1222 || ns->my_pe_num == 1217) {
          printf("%d Sending to %d %d %d %lld\n", ns->my_pe_num, ns->my_pe_num,
            m->msgId.id, m->msgId.comm, m->msgId.seq);
        }
#endif
    }
    else 
        m->proc_event_type = EXEC_COMPLETE;
    tw_event_send(e);

    return 0;
}


