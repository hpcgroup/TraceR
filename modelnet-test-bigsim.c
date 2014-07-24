/* 
 *
 * SUMMARY:
 * Trace support for codes simulations.
 * The simulation will be driven by bigsim traces.
 *
 * Author: Bilge Acun
 *
 */

#include <string.h>
#include <assert.h>
#include <ross.h>
#include <stdbool.h>

#include "codes/model-net.h"
#include "codes/lp-io.h"
#include "codes/codes.h"
#include "codes/codes_mapping.h"
#include "codes/configuration.h"
#include "codes/lp-type-lookup.h"

#include "tests/bigsim/CWrapper.h"

static int net_id = 0;
static int num_routers = 0;
static int num_servers = 0;
static int offset = 2;

static int num_routers_per_rep = 0;
static int num_servers_per_rep = 0;
static int lps_per_rep = 0;
static int total_lps = 0;

typedef struct proc_msg proc_msg;
typedef struct proc_state proc_state;

/* types of events that will constitute triton requests */
enum proc_event
{
    KICKOFF,    /* initial event */
    LOCAL,      /* local event */
    RECV_MSG,   /* bigsim, when received a message */
    EXEC_COMP   /* bigsim, when completed an execution */
};

struct proc_state
{
    int msg_sent_count;   /* requests sent */
    int msg_recvd_count;  /* requests recvd */
    int local_recvd_count; /* number of local messages received */
    tw_stime start_ts;    /* time that we started sending requests */
    tw_stime end_ts;      /* time that we ended sending requests */
    PE* my_pe;          /* bigsim trace timeline, stores the task depency graph*/
    TraceReader* trace_reader; /* for reading the bigsim traces */
    int** msgDestTask;   /* mapping from msgID to destination task for faster access */
};

struct proc_msg
{
    enum proc_event proc_event_type;
    tw_lpid src;          /* source of this request or ack */
    int incremented_flag; /* helper for reverse computation */
    MsgID* msg_id;
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

tw_lptype proc_lp = {
     (init_f) proc_init,
     (event_f) proc_event,
     (revent_f) proc_rev_event,
     (final_f)  proc_finalize, 
     (map_f) codes_mapping,
     sizeof(proc_state),
};

extern const tw_lptype* proc_get_lp_type();
static void proc_add_lp_type();
static tw_stime ns_to_s(tw_stime ns);
static tw_stime s_to_ns(tw_stime ns);

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
static void handle_exec_event(
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
static void handle_exec_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

const tw_optdef app_opt [] =
{
    TWOPT_GROUP("Model net test case" ),
    TWOPT_END()
};

//helper function declarations
static void exec_task(
    proc_state * ns,
    int task_id,
    tw_lp * lp);

static int send_msg(
    proc_state * ns,
    int task_id,
    int size,
    int src_pe,
    int id,
    int dest_id,
    unsigned long long timeOffset,
    tw_lp * lp);

static int exec_comp(
    proc_state * ns,
    int task_id,
    unsigned long long sendOffset,
    tw_lp * lp);
              
static int find_task_from_msg(
    proc_state * ns,
    MsgID* msg_id);

static int pe_to_lpid(int pe);

int main(int argc, char **argv)
{
    int nprocs;
    int rank;
    //printf("\n Config count %d ",(int) config.lpgroups_count);
    g_tw_ts_end = s_to_ns(60*60*24*365); /* one year, in nsecs */
    lp_io_handle handle;

    tw_opt_add(app_opt);
    tw_init(&argc, &argv);
    
    if(argc < 2)
    {
	printf("\n Usage: mpirun <args> --sync=2/3 mapping_file_name.conf (optional --nkp) ");
	MPI_Finalize();
	return 0;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
    configuration_load(argv[2], MPI_COMM_WORLD, &config);
    net_id=model_net_set_params();
    proc_add_lp_type();
    
    codes_mapping_setup();
    
    num_servers = codes_mapping_get_group_reps("MODELNET_GRP") * codes_mapping_get_lp_count("MODELNET_GRP", "server");
    if(net_id == DRAGONFLY)
    {
        num_routers = codes_mapping_get_group_reps("MODELNET_GRP") * codes_mapping_get_lp_count("MODELNET_GRP", "dragonfly_router"); 
	offset = 1;
    }

    if(lp_io_prepare("modelnet-test", LP_IO_UNIQ_SUFFIX, &handle, MPI_COMM_WORLD) < 0)
    {
        return(-1);
    }

    tw_run();
    model_net_report_stats(net_id);

    if(lp_io_flush(handle, MPI_COMM_WORLD) < 0)
    {
        return(-1);
    }

    tw_end();
    return 0;
}

const tw_lptype* proc_get_lp_type()
{
    return(&proc_lp);
}

static void proc_add_lp_type()
{
    lp_type_register("server", proc_get_lp_type());
}

static void proc_init(
    proc_state * ns,
    tw_lp * lp)
{
    tw_event *e;
    proc_msg *m;
    tw_stime kickoff_time;
    
    memset(ns, 0, sizeof(*ns));

    /* each server sends a dummy event to itself that will kick off the real
     * simulation
     */

    //printf("\n Initializing servers %d ", (int)lp->gid);
    /* skew each kickoff event slightly to help avoid event ties later on */
    kickoff_time = g_tw_lookahead + tw_rand_unif(lp->rng); 

    e = codes_event_new(lp->gid, kickoff_time, lp);
    m = tw_event_data(e);
    m->proc_event_type = KICKOFF;
    tw_event_send(e);

    return;
}

static void proc_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
   switch (m->proc_event_type)
    {
        case KICKOFF:
            handle_kickoff_event(ns, b, m, lp);
            break;
	case LOCAL:
	    handle_local_event(ns, b, m, lp); 
	    break;
        case RECV_MSG:
            handle_recv_event(ns, b, m, lp);
            break;
        case EXEC_COMP:
            handle_exec_event(ns, b, m, lp);
            break;
        default:
	    printf("\n Invalid message type %d ", m->proc_event_type);
            assert(0);
        break;
    }
}

static void proc_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    switch (m->proc_event_type)
    {
        case KICKOFF:
            handle_kickoff_rev_event(ns, b, m, lp);
            break;
    	case LOCAL:
	        handle_local_rev_event(ns, b, m, lp);    
	        break;
        case RECV_MSG:
            handle_recv_rev_event(ns, b, m, lp);
            break;
        case EXEC_COMP:
            handle_exec_rev_event(ns, b, m, lp);
            break;
        default:
            assert(0);
            break;
    }

    return;
}

static void proc_finalize(
    proc_state * ns,
    tw_lp * lp)
{
    //printf("server %llu recvd %d bytes in %f seconds, %f MiB/s sent_count %d recvd_count %d local_count %d \n", (unsigned long long)lp->gid, PAYLOAD_SZ*ns->msg_recvd_count, ns_to_s(ns->end_ts-ns->start_ts), 
   //     ((double)(PAYLOAD_SZ*NUM_REQS)/(double)(1024*1024)/ns_to_s(ns->end_ts-ns->start_ts)), ns->msg_sent_count, ns->msg_recvd_count, ns->local_recvd_count);
    return;
}

/* convert ns to seconds */
static tw_stime ns_to_s(tw_stime ns)
{
    return(ns / (1000.0 * 1000.0 * 1000.0));
}

/* convert seconds to ns */
static tw_stime s_to_ns(tw_stime ns)
{
    return(ns * (1000.0 * 1000.0 * 1000.0));
}

/* handle initial event */
static void handle_kickoff_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{
    /* record when transfers started on this server */
    ns->start_ts = tw_now(lp);

    num_servers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", "server");
    num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", "dragonfly_router");

    lps_per_rep = num_servers_per_rep * 2 + num_routers_per_rep;

    int opt_offset = 0;
    total_lps = num_servers * 2 + num_routers;

    if(net_id == DRAGONFLY && (lp->gid % lps_per_rep == num_servers_per_rep - 1))
          opt_offset = num_servers_per_rep + num_routers_per_rep; /* optional offset due to dragonfly mapping */

    //Each server read it's trace
    int my_pe_num = (lp->gid/lps_per_rep)*(num_routers_per_rep+num_servers_per_rep) + (lp->gid%lps_per_rep); //TODO: check this
    ns->my_pe = newPE();
    ns->trace_reader = newTraceReader();
    int tot=0, totn=0, emPes=0, nwth=0;
    long long unsigned int startTime=0;
    TraceReader_readTrace(ns->trace_reader, &tot, &totn, &emPes, &nwth, ns->my_pe, my_pe_num, &startTime, ns->msgDestTask);
    //Check if codes config file does not match the traces
    if(num_servers != TraceReader_totalWorkerProcs(ns->trace_reader)){
        printf("ERROR: BigSim traces do not match the codes config file..\n");
        MPI_Finalize();
	    return;
    }
    //PE_set_busy(ns->my_pe, true);
    //execute the first task
    exec_task(ns, startTime, lp);

}
static void handle_local_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    ns->local_recvd_count++;
}
static void handle_local_rev_event(
	       proc_state * ns,
	       tw_bf * b,
	       proc_msg * m,
	       tw_lp * lp)
{
   ns->local_recvd_count--;
}

/* reverse handler for kickoff */
static void handle_kickoff_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    ns->msg_sent_count--;
    model_net_event_rc(net_id, lp, MsgID_getSize(m->msg_id));
    return;
}

static void handle_recv_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    /* safety check that this request got to the right server */
    //printf("\n m->src %d lp->gid %d ", m->src, lp->gid);
    int opt_offset = 0;
 
    if(net_id == DRAGONFLY && (lp->gid % lps_per_rep == num_servers_per_rep - 1))
        opt_offset = num_servers_per_rep + num_routers_per_rep; /* optional offset due to dragonfly mapping */    	
    tw_lpid dest_id = (lp->gid + offset + opt_offset)%(num_servers*2 + num_routers);

    assert(m->src == dest_id);
    int task_id = find_task_from_msg(ns, m->msg_id);
    //Check if the PE is busy
    //buffer the message if it arrives when pe is busy
    if(PE_is_busy(ns->my_pe)){
        //add the task_id to the vector 
        PE_addToBuffer(ns->my_pe, task_id);
        return;
    }
    //Set the PE as busy 
    PE_set_busy(ns->my_pe, true);
    //find which task the message belongs to
    //then call exec_task
    exec_task(ns,task_id, lp);
}
static void handle_exec_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //Mark the task as done
    //For exec complete event msg_id contains the task_id for convenience
    int task_id = MsgID_getID(m->msg_id); 
    PE_set_taskDone(ns->my_pe, task_id, true);

    //Increment the current task .. TODO: check of this is needed
    PE_set_currentTask(ns->my_pe, task_id+1);

    //Task completed, pe is not busy anymore
    PE_set_busy(ns->my_pe, false);

    //Execute the buffered messages that are recevied while the pe is busy
    while(PE_getNextBuffedMsg(ns->my_pe) != -1){
        exec_task(ns, task_id, lp);
    }
}

static void handle_recv_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    int task_id = find_task_from_msg(ns, m->msg_id);
    //Mark the task as not done
    PE_set_taskDone(ns->my_pe, task_id, false);
    //mark it's forward dependencies as not done
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    for(int i=0; i<fwd_dep_size; i++){
        PE_set_taskDone(ns->my_pe, fwd_deps[i], false);
    }
    //decrease the currentTask .. TODO: check this...
    PE_set_currentTask(ns->my_pe, task_id-1);

    model_net_event_rc(net_id, lp, MsgID_getSize(m->msg_id));
}
static void handle_exec_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //Mark the task as not done
    int task_id = MsgID_getID(m->msg_id); 
    PE_set_taskDone(ns->my_pe, task_id, false);
    //mark it's forward dependencies as not done
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    for(int i=0; i<fwd_dep_size; i++){
        PE_set_taskDone(ns->my_pe, fwd_deps[i], false);
    }
    //Decrement the current task .. TODO: check this 
    PE_set_currentTask(ns->my_pe, task_id-1);
}

//executes the task with the specified id
static void exec_task(
	        proc_state * ns,
                int task_id,
		tw_lp * lp)
{
    //check if the backward dependencies are satisfied
    //if not, do nothing
    //if yes, execute the task
    if(!PE_noUnsatDep(ns->my_pe, task_id)){
        return;
    }
    //mark the execution time of the task
    unsigned long long execTime = PE_getTaskExecTime(ns->my_pe, task_id);

    //for each entry of the task, create a recv event and send them out to
    //whereever it belongs       
    int msgEntCount= PE_getTaskMsgEntryCount(ns->my_pe, task_id);
    MsgEntry** taskEntries = PE_getTaskMsgEntries(ns->my_pe, task_id);

    for(int i=0; i<msgEntCount; i++){
        int myPE = PE_get_myEmPE(ns->my_pe);
        int myNode = myPE/num_servers;
        int node = MsgEntry_getNode(taskEntries[i]);
        int thread = MsgEntry_getThread(taskEntries[i]);
        unsigned long long sendOffset = MsgEntry_getSendOffset(taskEntries[i]);
        unsigned long long copyTime = 0;
        unsigned long long delay = 0;

        // if there are intraNode messages
        if (node == myNode || node == -1 || (node <= -100 && (node != -100-myNode || thread != -1)))
        {
            if(node == -100-myNode && thread != -1)
            {
              int destPE = myNode*num_servers - 1;
              for(int i=0; i<num_servers; i++)
              {        
                destPE++;
                if(i == thread) continue;
                delay += copyTime;
                send_msg(ns, task_id, MsgEntry_getSize(taskEntries[i]), 
                    MsgEntry_getPE(taskEntries[i]), MsgEntry_getID(taskEntries[i]), 
                        pe_to_lpid(destPE), sendOffset+delay, lp);
              }
            } else if(node != -100-myNode && node <= -100) {
              int destPE = myNode*num_servers - 1;
              for(int i=0; i<num_servers; i++)
              {
                destPE++;
                delay += copyTime;
                send_msg(ns, task_id, MsgEntry_getSize(taskEntries[i]), 
                    MsgEntry_getPE(taskEntries[i]), MsgEntry_getID(taskEntries[i]), 
                        pe_to_lpid(destPE), sendOffset+delay, lp);
              }
            } else if(thread >= 0) {
              int destPE = myNode*num_servers + thread;
              delay += copyTime;
              send_msg(ns, task_id, MsgEntry_getSize(taskEntries[i]),
                  MsgEntry_getPE(taskEntries[i]), MsgEntry_getID(taskEntries[i]),
                      pe_to_lpid(destPE), sendOffset+delay, lp);
            } else if(thread==-1) { // broadcast to all work cores
              int destPE = myNode*num_servers - 1;
              for(int i=0; i<num_servers; i++)
              {
                destPE++;
                delay += copyTime;
                send_msg(ns, task_id, MsgEntry_getSize(taskEntries[i]), 
                    MsgEntry_getPE(taskEntries[i]), MsgEntry_getID(taskEntries[i]), 
                        pe_to_lpid(destPE), sendOffset+delay, lp);
              }
            }
          }
          if(node != myNode)
          {
                send_msg(ns, task_id, MsgEntry_getSize(taskEntries[i]), 
                    MsgEntry_getPE(taskEntries[i]), MsgEntry_getID(taskEntries[i]), 
                        pe_to_lpid(myPE), sendOffset+delay, lp);
          }
   }
    //mark the task as done, create a complete exec event 
    //PE_set_taskDone(ns->my_pe, task_id, true);

    //execute the forward dependencies of the task with exec_task
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    for(int i=0; i<fwd_dep_size; i++){
        exec_task(ns, fwd_deps[i], lp);
    }
    //complete the task
    exec_comp(ns, task_id, execTime, lp);
}
//creates and sends the message
static int send_msg(
        proc_state * ns,
        int task_id,
        int size,
        int src_pe,
        int id,
        int dest_id,
        unsigned long long sendOffset,
        tw_lp * lp) {

        proc_msg * m_local = malloc(sizeof(proc_msg));
        proc_msg * m_remote = malloc(sizeof(proc_msg));

        m_local->proc_event_type = LOCAL;
        m_local->src = lp->gid;
        m_local->msg_id = newMsgID(size, src_pe, id);

        memcpy(m_remote, m_local, sizeof(proc_msg));
        m_remote->proc_event_type = RECV_MSG;
        /* send the message */
        /*   model_net_event params:
             int net_id,
             char* category,
             tw_lpid final_dest_lp,
             uint64_t message_size,
             tw_stime offset,
             int remote_event_size,
             const void* remote_event,
             int self_event_size,
             const void* self_event,
             tw_lp *sender
        */
        model_net_event(net_id, "test", dest_id, size, sendOffset, sizeof(proc_msg), (const void*)m_remote, sizeof(proc_msg), (const void*)m_local, lp);
        ns->msg_sent_count++;
    
    return 0;
}
static int exec_comp(
    proc_state * ns,
    int task_id,
    unsigned long long sendOffset,
    tw_lp * lp)
{
    proc_msg * m_local = malloc(sizeof(proc_msg));
    proc_msg * m_remote = malloc(sizeof(proc_msg));

    m_local->proc_event_type = LOCAL;
    m_local->src = lp->gid;
    m_local->msg_id = newMsgID(0, lp->gid, task_id);

    memcpy(m_remote, m_local, sizeof(proc_msg));
    m_remote->proc_event_type = EXEC_COMP;

    model_net_event(net_id, "test", lp->gid, 0, sendOffset, sizeof(proc_msg), (const void*)m_remote, sizeof(proc_msg), (const void*)m_local, lp);

    return 0;
}

//returs the task id that the message belongs to using msgDestTask map
static int find_task_from_msg(
        proc_state * ns,
        MsgID* msg_id){

        int** msgDests = ns->msgDestTask;
        int task_id = msgDests[MsgID_getPE(msg_id)][MsgID_getID(msg_id)];
        return task_id;
}

//utility function to convert pe number to tw_lpid number
//Assuming the servers come last in lp registration
static int pe_to_lpid(int pe){
    int lp_id = 0;
    if(net_id == DRAGONFLY)
        //TODO: check this for correctness
        lp_id = (pe/num_servers_per_rep)*lps_per_rep + num_servers_per_rep + num_routers_per_rep + pe%num_servers_per_rep;
    else
        lp_id = pe*offset+offset-1;
    return lp_id;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ft=c ts=8 sts=4 sw=4 expandtab
 */
