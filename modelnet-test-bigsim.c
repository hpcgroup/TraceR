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
#include "tests/bigsim/entities/MsgEntry.h"

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
};

struct proc_msg
{
    enum proc_event proc_event_type;
    tw_lpid src;          /* source of this request or ack */
    int incremented_flag; /* helper for reverse computation */
    MsgID msg_id;
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
    int recv,
    tw_lp * lp);
              
static int find_task_from_msg(
    proc_state * ns,
    MsgID msg_id);

static int pe_to_lpid(int pe);
static int lpid_to_pe(int pe);

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
    PE_printStat(ns->my_pe);
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
    printf("handle_kickoff_event, lp_gid: %d -- ", (int)lp->gid);
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
    int my_pe_num = lpid_to_pe(lp->gid);
    printf("my_pe_num:%d, lp->gid:%d\n", my_pe_num, (int)lp->gid);

    ns->my_pe = newPE();
    ns->trace_reader = newTraceReader();
    int tot=0, totn=0, emPes=0, nwth=0;
    long long unsigned int startTime=0;
    TraceReader_readTrace(ns->trace_reader, &tot, &totn, &emPes, &nwth, ns->my_pe, my_pe_num, &startTime);
    //Check if codes config file does not match the traces
    if(num_servers != TraceReader_totalWorkerProcs(ns->trace_reader)){
        printf("ERROR: BigSim traces do not match the codes config file..\n");
        MPI_Finalize();
	return;
    }
    //printf("lpid_to_pe(my_pe_num):%d, my_pe_num:%d\n", lpid_to_pe(lp->gid), my_pe_num);
    //Safety check if the pe_to_lpid converter is correct
    assert(pe_to_lpid(my_pe_num) == lp->gid);
    //Safety check if the lpid_to_pe converter is correct
    assert(lpid_to_pe(lp->gid) == my_pe_num);

    // printf("\t\tpe_to_lpid(my_pe_num):%d, lp->gid:%d .....\n", pe_to_lpid(my_pe_num), lp->gid);
    //PE_set_busy(ns->my_pe, true);
    //execute the first task
    exec_task(ns, 0, lp);

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
    model_net_event_rc(net_id, lp, m->msg_id.size);
    return;
}

static void handle_recv_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    int task_id = find_task_from_msg(ns, m->msg_id);
    //printf("handle_recv_event task_id:%d\n", task_id);

    if(task_id>=0){
        if(PE_get_taskDone(ns->my_pe,task_id))
            assert(0);
        assert(PE_getTaskMsgID(ns->my_pe, task_id).pe > 0);
        PE_invertMsgPe(ns->my_pe, task_id);
        //Check if pe is busy
        if(!PE_is_busy(ns->my_pe)){
            PE_set_busy(ns->my_pe, true);
            exec_task(ns,task_id, lp);
        }
        else{
            //buffer the message if it arrives when pe is busy
            PE_addToBuffer(ns->my_pe, task_id);
        }
        return;
    }
    // if first not-executed needs this and not busy start sequential
    int currentTask = PE_get_currentTask(ns->my_pe);
    //printf("currentTask: %d -- PE_get_tasksCount(ns->my_pe):%d\n", currentTask, PE_get_tasksCount(ns->my_pe));
    if(PE_getTaskMsgID(ns->my_pe, currentTask).pe -1 == m->msg_id.pe &&
            PE_getTaskMsgID(ns->my_pe, currentTask).id == m->msg_id.id ){
        if(PE_get_taskDone(ns->my_pe, currentTask))
            assert(0);
        PE_invertMsgPe(ns->my_pe, currentTask);
        //Check if pe is busy
        if(!PE_is_busy(ns->my_pe)){
            PE_set_busy(ns->my_pe, true);
            exec_task(ns, currentTask, lp);
        }
        else{
            //buffer the message if it arrives when pe is busy
            PE_addToBuffer(ns->my_pe, currentTask);
        }
        return;
    }
    else{
        // search in following tasks
        //printf("m->msg_id.pe:%d -- m->msg_id.id:%d\n", m->msg_id.pe, m->msg_id.id);
        for(int i=currentTask+1; i<PE_get_tasksCount(ns->my_pe); i++){
            // if task depends on this message
            if(PE_getTaskMsgID(ns->my_pe, i).pe -1 == m->msg_id.pe &&
                PE_getTaskMsgID(ns->my_pe, i).id == m->msg_id.id ){
                //printf("PE_getTaskMsgID(ns->my_pe, i).pe: %d -- PE_getTaskMsgID(ns->my_pe, i).id: %d\n", PE_getTaskMsgID(ns->my_pe, i).pe, PE_getTaskMsgID(ns->my_pe, i).id );
                if(PE_get_taskDone(ns->my_pe, i))
                    assert(0);
            }
            PE_invertMsgPe(ns->my_pe, i);
            //Check if pe is busy
            if(!PE_is_busy(ns->my_pe)){
                PE_set_busy(ns->my_pe, true);
                exec_task(ns, i, lp);
            }
            else{
                //buffer the message if it arrives when pe is busy
                //PE_addToBuffer(ns->my_pe, currentTask);
            }
            return;
        }
        printf("Could not find the task; this is all wrong..Aborting\n");
        assert(0);
    }
}

static void handle_exec_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //Mark the task as done
    //For exec complete event msg_id contains the task_id for convenience

    int task_id = m->msg_id.id; 
    printf("PE:%d handle_exec_event for task:%d.\n", lpid_to_pe(lp->gid), task_id);
    PE_printStat(ns->my_pe);
    PE_set_taskDone(ns->my_pe, task_id, true);

    //task is done, execute the forward dependencies
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    for(int i=0; i<fwd_dep_size; i++){
        exec_task(ns, fwd_deps[i], lp);
    }

    //Increment the current task .. TODO: check this
    //PE_set_currentTask(ns->my_pe, task_id+1);
    PE_increment_currentTask(ns->my_pe, task_id);

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

    model_net_event_rc(net_id, lp, m->msg_id.size);
}
static void handle_exec_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //Mark the task as not done
    int task_id = m->msg_id.id; 
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
    printf("PE: %d, exec_task %d\n", lpid_to_pe(lp->gid), task_id);
    //check if the backward dependencies are satisfied
    //if not, do nothing
    //if yes, execute the task
    if(!PE_noUnsatDep(ns->my_pe, task_id)){
        return;
    }
    //check if the task is already done -- safety check?
    if(PE_get_taskDone(ns->my_pe, task_id)){
        printf("PE:%d WARNING: TASK IS ALREADY DONE: %d\n", lpid_to_pe(PE_get_myNum(ns->my_pe)), task_id);
        return;
    }
    //mark the execution time of the task
    unsigned long long execTime = PE_getTaskExecTime(ns->my_pe, task_id);

    //for each entry of the task, create a recv event and send them out to
    //whereever it belongs       
    int msgEntCount= PE_getTaskMsgEntryCount(ns->my_pe, task_id);
    printf("msgEntCount:%d, of task_id:%d\n", msgEntCount, task_id);

    int myPE = PE_get_myNum(ns->my_pe);
    assert(myPE == lpid_to_pe(lp->gid)); 
    int nWth = 1; 
    int myNode = myPE/nWth;
    unsigned long long copyTime = 1;
    unsigned long long delay = 0;

    for(int i=0; i<msgEntCount; i++){
        MsgEntry* taskEntry = PE_getTaskMsgEntry(ns->my_pe, task_id, i);
        //int myPE = PE_get_myEmPE(ns->my_pe);
        int node = MsgEntry_getNode(taskEntry);
        int thread = MsgEntry_getThread(taskEntry);
        unsigned long long sendOffset = MsgEntry_getSendOffset(taskEntry);
        printf("\tENTRY node:%d, thread:%d\n", node, thread);
        
        // if there are intraNode messages
        if (node == myNode || node == -1 || (node <= -100 && (node != -100-myNode || thread != -1)))
        {
            printf("\t\t\tINTRA NODE\n");
            if(node == -100-myNode && thread != -1)
            {
              int destPE = myNode*nWth - 1;
              for(int i=0; i<nWth; i++)
              {        
                destPE++;
                if(i == thread) continue;
                delay += copyTime;
                if(pe_to_lpid(destPE) == lp->gid){
                    exec_comp(ns, task_id, sendOffset+delay, 1, lp);
                }else{
                send_msg(ns, task_id, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(destPE), sendOffset+delay, lp);
                }
              }
            } else if(node != -100-myNode && node <= -100) {
              int destPE = myNode*nWth - 1;
              //if(pe_to_lpid(destPE) == lp->gid) printf("\t\tEROROROROROR\n");
              for(int i=0; i<nWth; i++)
              {
                destPE++;
                //if(pe_to_lpid(destPE) == lp->gid) printf("\t\tEROROROROROR\n");
                delay += copyTime;
                if(pe_to_lpid(destPE) == lp->gid){
                    exec_comp(ns, task_id, sendOffset+delay, 1, lp);
                }else{
                send_msg(ns, task_id, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(destPE), sendOffset+delay, lp);
                }
              }
            } else if(thread >= 0) {
              int destPE = myNode*nWth + thread;
              //if(pe_to_lpid(destPE) == lp->gid) printf("\t\tEROROROROROR\n");
              delay += copyTime;
              if(pe_to_lpid(destPE) == lp->gid){
                  exec_comp(ns, task_id, sendOffset+delay, 1, lp);
              }else{
              send_msg(ns, task_id, MsgEntry_getSize(taskEntry),
                  MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry),
                      pe_to_lpid(destPE), sendOffset+delay, lp);
              }
            } else if(thread==-1) { // broadcast to all work cores
              int destPE = myNode*nWth - 1;
              for(int i=0; i<nWth; i++)
              {
                destPE++;
                //if(pe_to_lpid(destPE) == lp->gid) printf("\t\tEROROROROROR\n");
                delay += copyTime;
                if(pe_to_lpid(destPE) == lp->gid){
                    exec_comp(ns, task_id, sendOffset+delay, 1, lp);
                }else{
                send_msg(ns, task_id, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(destPE), sendOffset+delay, lp);
                }
              }
            }
          }
          
          if(node != myNode)
          {
                if(pe_to_lpid(node) == lp->gid) printf("\tNOT EOROR\n");
                send_msg(ns, task_id, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(node), sendOffset+delay, lp);
          }
   }
    //mark the task as done, create a complete exec event 
    PE_set_taskDone(ns->my_pe, task_id, true);

    //execute the forward dependencies of the task with exec_task
/*    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    for(int i=0; i<fwd_dep_size; i++){
        exec_task(ns, fwd_deps[i], lp);
    }
*/
    //complete the task
    exec_comp(ns, task_id, execTime, 0, lp);
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
        //printf("sending message from %d to %d\n", (int)lp->gid, dest_id);
        proc_msg * m_local = malloc(sizeof(proc_msg));
        proc_msg * m_remote = malloc(sizeof(proc_msg));

        m_local->proc_event_type = LOCAL;
        m_local->src = lp->gid;
        m_local->msg_id.size = size;
        m_local->msg_id.pe = src_pe;
        m_local->msg_id.id = id;

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
    int recv,
    tw_lp * lp)
{
    printf("creating an exec_comp...me: %d\n", (int)lp->gid);
/*
    proc_msg * m_local = malloc(sizeof(proc_msg));
    proc_msg * m_remote = malloc(sizeof(proc_msg));

    m_local->proc_event_type = LOCAL;
    m_local->src = lp->gid;
    m_local->msg_id.size = 0;
    m_local->msg_id.pe = lp->gid;
    m_local->msg_id.id = task_id;

    memcpy(m_remote, m_local, sizeof(proc_msg));
    m_remote->proc_event_type = EXEC_COMP;

    model_net_event(net_id, "test", lp->gid, 0, sendOffset, sizeof(proc_msg), (const void*)m_remote, sizeof(proc_msg), (const void*)m_local, lp);
*/

    //If it's a self event use codes_event_new instead of model_net_event 
    tw_event *e;
    proc_msg *m;

    e = codes_event_new(lp->gid, sendOffset, lp);
    m = tw_event_data(e);
    m->msg_id.size = 0;
    m->msg_id.pe = lp->gid;
    m->msg_id.id = task_id;
    if(recv)
        m->proc_event_type = RECV_MSG;
    else 
        m->proc_event_type = EXEC_COMP;
    tw_event_send(e);

    return 0;
}

//returs the task id that the message belongs to using msgDestTask map
static int find_task_from_msg(
        proc_state * ns,
        MsgID msg_id){

    return PE_findTaskFromMsg(ns->my_pe, &msg_id);
}

//utility function to convert pe number to tw_lpid number
//Assuming the servers come last in lp registration in terms of global id
static int pe_to_lpid(int pe){
    int lp_id = 0;
    if(net_id == DRAGONFLY)
        //TODO: check this for correctness
        lp_id = (pe/num_servers_per_rep)*lps_per_rep + (pe%num_servers_per_rep);
    else
        lp_id = pe*offset;
    return lp_id;
}

//utility function to convert tw_lpid to simulated pe number
//Assuming the servers come last in lp registration in terms of global id
static int lpid_to_pe(int lp_gid){
    int my_pe_num = 0;
    if(net_id == DRAGONFLY){
        my_pe_num = ((int)(lp_gid/lps_per_rep))*(num_servers_per_rep)+(lp_gid%lps_per_rep);
    }
    else
        my_pe_num = lp_gid/offset;
    //printf("\t\t\tlpid_to_pe... lp_gid:  %d, my_pe_num: %d\n",lp_gid, my_pe_num);
    return my_pe_num;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ft=c ts=8 sts=4 sw=4 expandtab
 */
