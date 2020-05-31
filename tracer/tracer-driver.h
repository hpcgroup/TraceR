/** \file tracer-driver.h
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * Written by:
 *     Nikhil Jain <nikhil.jain@acm.org>
 *     Bilge Acun <acun2@illinois.edu>
 *     Abhinav Bhatele <bhatele@llnl.gov>
 *
 * LLNL-CODE-740483. All rights reserved.
 *
 * This file is part of TraceR. For details, see:
 * https://github.com/LLNL/TraceR
 * Please also read the LICENSE file for the MIT License notice.
 */

#ifndef _TRACER_DRIVER_H_
#define _TRACER_DRIVER_H_

#include "reader/datatypes.h"
#include "reader/CWrapper.h"
#include "elements/MsgEntry.h"
#include "elements/PE.h"

#if TRACER_OTF_TRACES
#include "reader/otf2_reader.h"
#endif

#define BCAST_DEGREE  2
#define REDUCE_DEGREE  2

#define TRACER_A2A_ALG_CUTOFF 512
#define TRACER_ALLGATHER_ALG_CUTOFF 163840
#define TRACER_BLOCK_SIZE 32
#define MPI_INTERNAL_DELAY 10
#define TRACER_SCATTER_ALG_CUTOFF 0

/* stores mapping of core to job ID and process ID */
typedef struct CoreInf {
    int mapsTo, jobID;
} CoreInf;

/* stores mapping of core to Communication Time and Computation Time */
typedef struct TimeInfo{
    int jobID;
    int rank;
    tw_stime comm_time;
    tw_stime comp_time;
} TimeInfo;

/* ROSS level state information for each core */
struct proc_state
{
    tw_stime start_ts;  /* time when first event is processed */
    tw_stime end_ts;    /* time when last event is processed */
    tw_stime computation_t; /* store time spend in computation*/
    int region_start;/* flag to mark the start of a region*/
    int region_end;/* flag to mark end of a region*/
    int region_start_sim_time;/* store current simulation time when the region starts*/
    int region_end_sim_time;/* store current simulation time when region ends*/
    PE* my_pe;          /* stores all core information */
#if TRACER_BIGSIM_TRACES
    TraceReader* trace_reader; /* for reading the bigsim traces */
#endif
    clock_t sim_start;  /* clock time when simulation starts */
    int my_pe_num, my_job;
};

extern JobInf *jobs;
extern tw_stime soft_delay_mpi;
extern tw_stime nic_delay;
extern tw_stime rdma_delay;

extern int net_id;
extern unsigned int print_frequency;
extern double copy_per_byte;
extern double eager_limit;


/* types of events that will constitute ROSS event requests */
enum proc_event
{
    KICKOFF=1,          /* initial event */
    LOCAL,              /* local event */
    RECV_MSG,           /* receive a message */
    BCAST,              /* broadcast --> to be deprecated */
    EXEC_COMPLETE,      /* marks completion of task */
    SEND_COMP,          /* send completed */
    RECV_POST,          /* Message from receiver that the recv is posted */
    COLL_BCAST,         /* Collective impl for bcast */
    COLL_REDUCTION,     /* Collective impl for reduction */
    COLL_A2A,           /* Collective impl for a2a */
    COLL_A2A_SEND_DONE,
    COLL_ALLGATHER,     /* Collective impl for allgather */
    COLL_ALLGATHER_SEND_DONE, 
    COLL_BRUCK,         /* event used by Bruck implementation */
    COLL_BRUCK_SEND_DONE,
    COLL_A2A_BLOCKED,   /* event used by blocked A2A implementation */
    COLL_A2A_BLOCKED_SEND_DONE,
    COLL_SCATTER_SMALL, /* scatter event for small messages */
    COLL_SCATTER,       /* scatter event */
    COLL_SCATTER_SEND_DONE,
    RECV_COLL_POST,     /* Message from receiver that a recv for collective is posted */
    COLL_COMPLETE       /* collective completion event */
};

/* Tracer's part of the ROSS message */
struct proc_msg
{
    enum proc_event proc_event_type;
    tw_lpid src;            /* source of this event */
    int iteration;          /* iteration number when repeating traces */
    TaskPair executed;      /* task related to this event */
    int fwd_dep_count;      /* number of tasks dependent on the source task */
    int saved_task;         /* which task was acted on (for REV_HDL) */
    MsgID msgId;            /* message ID */
    bool incremented_flag;  /* core status (for REV_HDL) */
    int model_net_calls;    /* number of model_net calls (for REV_HDL) */
    unsigned int coll_info, coll_info_2; /* collective info */
};

/* Collective routine type */
enum tracer_coll_type
{
  TRACER_COLLECTIVE_BCAST=1,
  TRACER_COLLECTIVE_REDUCE,
  TRACER_COLLECTIVE_BARRIER,
  TRACER_COLLECTIVE_ALLTOALL_LARGE,
  TRACER_COLLECTIVE_ALLTOALL_BLOCKED,
  TRACER_COLLECTIVE_ALL_BRUCK,
  TRACER_COLLECTIVE_ALLGATHER_LARGE,
  TRACER_COLLECTIVE_SCATTER_SMALL,
  TRACER_COLLECTIVE_SCATTER
};


/* pairs up a local and remote event for collective */
struct Coll_lookup {
  proc_event remote_event, local_event;
};

/* core info to/from ROSS LP */
int pe_to_lpid(int pe, int job);
int pe_to_job(int pe);
int lpid_to_pe(int lp_gid);
int lpid_to_job(int lp_gid);

/* change of units for time */
tw_stime ns_to_s(tw_stime ns);
tw_stime s_to_ns(tw_stime ns);

void proc_init(
    proc_state * ns,
    tw_lp * lp);
void proc_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void proc_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void proc_commit_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void proc_finalize(
    proc_state * ns,
    tw_lp * lp);

//event handler declarations
void handle_kickoff_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_local_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_recv_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_bcast_event( /* to be deprecated */
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_exec_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_a2a_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_allgather_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_bruck_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_a2a_blocked_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_scatter_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_recv_post_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);

//reverse event handler declarations
void handle_kickoff_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_local_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
void handle_recv_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_bcast_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_exec_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_a2a_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_allgather_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_bruck_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_a2a_blocked_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_scatter_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
void handle_recv_post_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

tw_stime exec_task(
    proc_state * ns,
    TaskPair task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf *b);

void exec_task_rev(
    proc_state * ns,
    TaskPair task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf *b);

int send_msg(
    proc_state * ns,
    int size,
    int iter,
    MsgID *msgId,
    int64_t seq,
    int dest_id,
    tw_stime timeOffset,
    enum proc_event evt_type,
    tw_lp * lp,
    bool fillSz = false,
    int64_t size2 = 0);

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
    tw_lp * lp);

void delegate_send_msg(
    proc_state *ns,
    tw_lp * lp,
    proc_msg * m,
    tw_bf * b,
    Task * t,
    int taskid,
    tw_stime delay);

int bcast_msg(
    proc_state * ns,
    int size,
    int iter,
    MsgID *msgId,
    tw_stime timeOffset,
    tw_stime copyTime,
    tw_lp * lp,
    proc_msg *m);

int exec_comp(
    proc_state * ns,
    int iter,
    int task_id,
    int comm_id,
    tw_stime sendOffset,
    int recv,
    tw_lp * lp);

void perform_collective(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b);

void perform_bcast(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_reduction(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_a2a(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_allreduce(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_allgather(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_bruck(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_a2a_blocked(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_scatter_small(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_scatter(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void handle_coll_recv_post_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

void handle_coll_complete_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

int send_coll_comp(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg * m);

void perform_collective_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b);

void perform_bcast_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_reduction_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_a2a_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_allreduce_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_allgather_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_bruck_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_a2a_blocked_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_scatter_small_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void perform_scatter_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

void handle_coll_recv_post_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

void handle_coll_complete_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

int send_coll_comp_rev(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg * m);


#endif
