#ifndef _TRACER_DRIVER_H_
#define _TRACER_DRIVER_H_

#include "bigsim/datatypes.h"
#include "bigsim/CWrapper.h"
#include "bigsim/entities/MsgEntry.h"
#include "bigsim/entities/PE.h"

#if TRACER_OTF_TRACES
#include "bigsim/otf2_reader.h"
#endif

#define BCAST_DEGREE  2
#define REDUCE_DEGREE  2

typedef struct CoreInf {
    int mapsTo, jobID;
} CoreInf;

struct proc_state
{
    int msg_sent_count;   /* requests sent */
    int msg_recvd_count;  /* requests recvd */
    int local_recvd_count; /* number of local messages received */
    tw_stime start_ts;    /* time that we started sending requests */
    tw_stime end_ts;      /* time that we ended sending requests */
    PE* my_pe;          /* bigsim trace timeline, stores the task depency graph*/
#if TRACER_BIGSIM_TRACES
    TraceReader* trace_reader; /* for reading the bigsim traces */
#endif
    clock_t sim_start;
    int my_pe_num, my_job;
};

/* types of events that will constitute triton requests */
enum proc_event
{
    KICKOFF=1,    /* initial event */
    LOCAL,      /* local event */
    RECV_MSG,   /* bigsim, when received a message */
    BCAST,      /* broadcast --> to be deprecated */
    EXEC_COMPLETE,   /* bigsim, when completed an execution */
    SEND_COMP, /* Send completed for Isends */
    RECV_POST, /* Message from receiver that the recv is posted */
    COLL_BCAST, /* Collective impl for bcast */
    COLL_REDUCTION, /* Collective impl for reduction */
    COLL_A2A, /* Collective impl for a2a */
    COLL_A2A_SEND_DONE, 
    COLL_ALLGATHER, /* Collective impl for allgather */
    COLL_ALLGATHER_SEND_DONE, 
    COLL_BRUCK,
    COLL_BRUCK_SEND_DONE,
    COLL_A2A_BLOCKED,
    COLL_A2A_BLOCKED_SEND_DONE,
    COLL_SCATTER_SMALL,
    COLL_SCATTER,
    COLL_SCATTER_SEND_DONE,
    RECV_COLL_POST,
    COLL_COMPLETE
};

struct proc_msg
{
    enum proc_event proc_event_type;
    tw_lpid src;          /* source of this request or ack */
    int iteration;
    TaskPair executed;
    int fwd_dep_count;
    int saved_task;
    MsgID msgId;
    bool incremented_flag; /* helper for reverse computation */
    int model_net_calls;
    unsigned int coll_info;
};

struct Coll_lookup {
  proc_event remote_event, local_event;
};

static void proc_init(
    proc_state * ns,
    tw_lp * lp);
static void proc_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void proc_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void proc_finalize(
    proc_state * ns,
    tw_lp * lp);

//event handler declarations
static void handle_kickoff_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_local_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_recv_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_bcast_event( /* to be deprecated */
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_exec_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_a2a_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_allgather_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_bruck_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_a2a_blocked_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_scatter_send_comp_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_recv_post_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);

//reverse event handler declarations
static void handle_kickoff_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_local_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
   tw_lp * lp);
static void handle_recv_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_bcast_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_exec_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_a2a_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_allgather_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_bruck_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_a2a_blocked_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_scatter_send_comp_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);
static void handle_recv_post_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static tw_stime exec_task(
    proc_state * ns,
    TaskPair task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf *b);

static void exec_task_rev(
    proc_state * ns,
    TaskPair task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf *b);

static int send_msg(
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

static void enqueue_msg(
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

static void delegate_send_msg(
    proc_state *ns,
    tw_lp * lp,
    proc_msg * m,
    tw_bf * b,
    Task * t,
    int taskid,
    tw_stime delay);

static int bcast_msg(
    proc_state * ns,
    int size,
    int iter,
    MsgID *msgId,
    tw_stime timeOffset,
    tw_stime copyTime,
    tw_lp * lp,
    proc_msg *m);

static int exec_comp(
    proc_state * ns,
    int iter,
    int task_id,
    int comm_id,
    tw_stime sendOffset,
    int recv,
    tw_lp * lp);

static void perform_collective(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b);

static void perform_bcast(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_reduction(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_a2a(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_allreduce(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_allgather(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_bruck(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_a2a_blocked(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_scatter_small(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_scatter(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void handle_coll_recv_post_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static void handle_coll_complete_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static int send_coll_comp(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg * m);

static void perform_collective_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b);

static void perform_bcast_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_reduction_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_a2a_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_allreduce_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_allgather_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_bruck_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_a2a_blocked_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_scatter_small_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void perform_scatter_rev(
    proc_state * ns,
    int task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent);

static void handle_coll_recv_post_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static void handle_coll_complete_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static int send_coll_comp_rev(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg * m);


#endif
