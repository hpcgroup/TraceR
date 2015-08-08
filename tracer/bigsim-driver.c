/* 
 *
 * SUMMARY:
 * Trace support for CODES simulations.
 * The simulation will be driven using bigsim traces.
 *
 * Author: Bilge Acun, Nikhil Jain
 *
 *
 */

#include <string.h>
#include <assert.h>
#include <ross.h>
#include <stdbool.h>
#include <time.h>

#include "codes/model-net.h"
#include "codes/lp-io.h"
#include "codes/codes.h"
#include "codes/codes_mapping.h"
#include "codes/configuration.h"
#include "codes/lp-type-lookup.h"

#include "bigsim/datatypes.h"
#include "bigsim/CWrapper.h"
#include "bigsim/entities/MsgEntry.h"

static int net_id = 0;
static int num_routers = 0;
static int num_servers = 0;
static int num_nics = 0;

static int num_routers_per_rep = 0;
static int num_servers_per_rep = 0;
static int num_nics_per_rep = 0;
static int lps_per_rep = 0;
static int total_lps = 0;

typedef struct proc_msg proc_msg;
typedef struct proc_state proc_state;

static int sync_mode = 0;

char tracer_input[256];

typedef struct CoreInf {
    int mapsTo, jobID;
} CoreInf;

CoreInf *global_rank;
JobInf *jobs;
tw_stime *jobTimes;
int num_jobs = 0;

#define DEBUG_PRINT 0

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
    int current_task;
    clock_t sim_start;
    int my_pe_num, my_job;
};

struct proc_msg
{
    enum proc_event proc_event_type;
    tw_lpid src;          /* source of this request or ack */
    int executed_task;
    int fwd_dep_count;
    MsgID msg_id;
    bool incremented_flag; /* helper for reverse computation */
    tw_stime saved_ts;
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
     (pre_run_f) NULL,
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
static void undone_task(
    proc_state * ns,
    int task_id,
    tw_lp * lp);

static unsigned long long exec_task(
    proc_state * ns,
    int task_id,
    int flag,
    tw_lp * lp);

static int send_msg(
    proc_state * ns,
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
              
static inline int pe_to_lpid(int pe, int job);
static inline int pe_to_job(int pe);
static inline int lpid_to_pe(int lp_gid);
static inline int lpid_to_job(int lp_gid);

int main(int argc, char **argv)
{
    int nprocs;
    int rank;
    int num_nets;
    int *net_ids;
    //printf("\n Config count %d ",(int) config.lpgroups_count);
    g_tw_ts_end = s_to_ns(60*60*24*365); /* one year, in nsecs */
    lp_io_handle handle;

    sync_mode = atoi(&argv[1][(strlen(argv[1])-1)]);

    tw_opt_add(app_opt);
    tw_init(&argc, &argv);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if(!rank) {
        printf("Running in sync mode: %d\n", sync_mode);
    }
    if(argc < 2 && rank == 0)
    {
	printf("\nUSAGE: \n");
        printf("\tSequential: modelnet-test-bigsim --sync=1 -- mapping_file_name.conf (optional --nkp)\n");
	printf("\tParallel Conservative: mpirun <args> modelnet-test-bigsim --sync=2 -- mapping_file_name.conf (optional --nkp)\n");
	printf("\tParallel Optimistic: mpirun <args> modelnet-test-bigsim --sync=3 -- mapping_file_name.conf (optional --nkp)\n\n");
        assert(0);
    }

    strncpy(tracer_input, argv[2], strlen(argv[2]) + 1);

    if(!rank) {
        printf("Config file is %s\n", argv[1]);
        printf("Trace input file is %s\n", tracer_input);
    }

    configuration_load(argv[1], MPI_COMM_WORLD, &config);

    model_net_register();

    net_ids=model_net_configure(&num_nets);
    assert(num_nets==1);
    net_id = *net_ids;
    free(net_ids);

    proc_add_lp_type();
    
    codes_mapping_setup();
    
    num_servers = codes_mapping_get_lp_count("MODELNET_GRP", 0, "server", 
            NULL, 1);

    if(net_id == TORUS) {
        num_nics = codes_mapping_get_lp_count("MODELNET_GRP", 0, "modelnet_torus",
                NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_torus", NULL, 1);
    }

    if(net_id == DRAGONFLY) {
        num_nics = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_dragonfly", NULL, 1);
        num_routers = codes_mapping_get_lp_count("MODELNET_GRP", 0, 
                "dragonfly_router", NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_dragonfly", NULL, 1);
        num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "dragonfly_router", NULL, 1);
    }

    if(net_id == FATTREE) {
        num_nics = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_fattree", NULL, 1);
        num_routers = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "fattree_switch", NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_fattree", NULL, 1);
        num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "fattree_switch", NULL, 1);
    }

    num_servers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
        "server", NULL, 1);

    total_lps = num_servers + num_nics + num_routers;
    lps_per_rep = num_servers_per_rep + num_nics_per_rep + num_routers_per_rep;


    if(lp_io_prepare("modelnet-test", LP_IO_UNIQ_SUFFIX, &handle, MPI_COMM_WORLD) < 0)
    {
        return(-1);
    }


    if(!rank) printf("Begin reading %s\n", tracer_input);

    FILE *jobIn = fopen(tracer_input, "r");
    char globalIn[256];
    fscanf(jobIn, "%s", globalIn);

    global_rank = (CoreInf*) malloc(num_servers * sizeof(CoreInf));

    for(int i = 0; i < num_servers; i++) {
        global_rank[i].mapsTo = -1;
        global_rank[i].jobID = -1;
    }

    if(!rank) printf("Reading %s\n", globalIn);

    if(rank == 0) {
        int line_data[3], localCount = 0;;
        FILE *gfile = fopen(globalIn, "rb");
        while(fread(line_data, sizeof(int), 3, gfile) != 0) {
            global_rank[line_data[0]].mapsTo = line_data[1];
            global_rank[line_data[0]].jobID = line_data[2];
#if DEBUG_PRINT
            printf("Read %d/%d %d %d\n", line_data[0], num_servers,
            global_rank[line_data[0]].mapsTo, global_rank[line_data[0]].jobID);
#endif
            localCount++;
        }
        fclose(gfile);
        printf("Read %d ranks\n", localCount);
    }
    MPI_Bcast(global_rank, 2 * num_servers, MPI_INT, 0, MPI_COMM_WORLD);


    fscanf(jobIn, "%d", &num_jobs);
    jobs = (JobInf*) malloc(num_jobs * sizeof(JobInf));
    jobTimes = (tw_stime*) malloc(num_jobs * sizeof(tw_stime));

    for(int i = 0; i < num_jobs; i++) {
        char tempTrace[200];
        fscanf(jobIn, "%s", tempTrace);
        sprintf(jobs[i].traceDir, "%s%s", tempTrace, "/bgTrace");
        fscanf(jobIn, "%s", jobs[i].map_file);
        fscanf(jobIn, "%d", &jobs[i].numRanks);
        jobs[i].rankMap = (int*) malloc(jobs[i].numRanks * sizeof(int));
        jobs[i].skipMsgId = -1;
        jobTimes[i] = 0;
    }

    //Load all summaries on proc 0 and bcast
    for(int i = 0; i < num_jobs; i++) {
        if(!rank) printf("Loading trace summary for job %d from %s\n", i,
                jobs[i].traceDir);
        TraceReader* t = newTraceReader(jobs[i].traceDir);
        TraceReader_loadTraceSummary(t);
        int num_workers = TraceReader_totalWorkerProcs(t);
        assert(num_workers == jobs[i].numRanks);
        if(rank == 0){ //only rank 0 loads the offsets and broadcasts
            TraceReader_loadOffsets(t);
            jobs[i].offsets = TraceReader_getOffsets(t);
        } else {
            jobs[i].offsets = (int*) malloc(sizeof(int) * num_workers);
        }
        MPI_Bcast(jobs[i].offsets, num_workers, MPI_INT, 0, MPI_COMM_WORLD);

        if(!rank) printf("Loading map file for job %d from %s\n", i,
                jobs[i].map_file);
        jobs[i].rankMap = (int *) malloc(sizeof(int) * num_workers);
        if(rank == 0){ //only rank 0 loads the ranks and broadcasts
            FILE *rfile = fopen(jobs[i].map_file, "rb");
            fread(jobs[i].rankMap, sizeof(int), num_workers, rfile);
            fclose(rfile);
        }
        MPI_Bcast(jobs[i].rankMap, num_workers, MPI_INT, 0, MPI_COMM_WORLD);
#if DEBUG_PRINT
        if(rank == 0) {
            for(int j = 0; j < num_workers; j++) {
                printf("Job %d %d to %d\n", i, j, jobs[i].rankMap[j]);
            }
        }
#endif
    }

    tw_run();
    //model_net_report_stats(net_id);

    if(lp_io_flush(handle, MPI_COMM_WORLD) < 0)
    {
        return(-1);
    }

    tw_stime* jobTimesMax = (tw_stime*) malloc(num_jobs * sizeof(tw_stime));
    MPI_Reduce(jobTimes, jobTimesMax, num_jobs, MPI_DOUBLE, MPI_MAX, 0,
    MPI_COMM_WORLD);

    if(rank == 0) {
        for(int i = 0; i < num_jobs; i++) {
            printf("Job %d Time %f s\n", i, ns_to_s(jobTimesMax[i]));
        }
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
    tw_lp * lp) {
    tw_event *e;
    proc_msg *m;
    tw_stime kickoff_time;
    
    memset(ns, 0, sizeof(*ns));

    //Each server read it's trace
    ns->sim_start = clock();
    ns->my_pe_num = lpid_to_pe(lp->gid);
    ns->my_job = lpid_to_job(lp->gid);

    if(ns->my_pe_num == -1) {
        return;
    }

    ns->my_pe = newPE();
    ns->trace_reader = newTraceReader(jobs[ns->my_job].traceDir);
    int tot=0, totn=0, emPes=0, nwth=0;
    long long unsigned int startTime=0;
    TraceReader_loadTraceSummary(ns->trace_reader);
    TraceReader_setOffsets(ns->trace_reader, &(jobs[ns->my_job].offsets));

    TraceReader_readTrace(ns->trace_reader, &tot, &totn, &emPes, &nwth,
                         ns->my_pe, ns->my_pe_num,  ns->my_job, &startTime);

    /* skew each kickoff event slightly to help avoid event ties later on */
    kickoff_time = startTime + g_tw_lookahead + tw_rand_unif(lp->rng);
    ns->end_ts = 0;

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
    m->saved_ts = ns->end_ts;
    ns->end_ts = tw_now(lp);
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
    ns->end_ts = m->saved_ts;
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
    if(ns->my_pe_num == -1) return;

    tw_stime jobTime = ns->end_ts-ns->start_ts;

    if(lpid_to_pe(lp->gid) == 0)
        printf("Job[%d]PE[%d]: FINALIZE in %f (tw_now %f) seconds.\n", ns->my_job,
          ns->my_pe_num, ns_to_s(jobTime), ns_to_s(tw_now(lp)-ns->start_ts));

    PE_printStat(ns->my_pe);

    if(jobTime > jobTimes[ns->my_job]) {
        jobTimes[ns->my_job] = jobTime;
    }

    return;
}

/* convert ns to seconds */
static tw_stime ns_to_s(tw_stime ns)
{
    return(ns / (1000.0 * 1000.0 * 1000.0));
}

/* convert seconds to ns */
static tw_stime s_to_ns(tw_stime s)
{
    return(s * (1000.0 * 1000.0 * 1000.0));
}

/* handle initial event */
static void handle_kickoff_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{
    //printf("handle_kickoff_event, lp_gid: %d -- ", (int)lp->gid);
    //record when transfers started on this server
    ns->start_ts = tw_now(lp);

    int my_pe_num = ns->my_pe_num;
    int my_job = ns->my_job;
    clock_t time_till_now = (double)(clock()-ns->sim_start)/CLOCKS_PER_SEC;
    if(my_pe_num == 0 && my_job == 0) {
        printf("PE%d - LP_GID:%d : START SIMULATION, TASKS COUNT: %d, FIRST "
        "TASK: %d, TIME TILL NOW=%f s\n", my_pe_num, (int)lp->gid,
        PE_get_tasksCount(ns->my_pe), PE_getFirstTask(ns->my_pe),
        (double)time_till_now);
    }

    //Safety check if the pe_to_lpid converter is correct
    assert(pe_to_lpid(my_pe_num, my_job) == lp->gid);

    exec_task(ns, PE_getFirstTask(ns->my_pe), 1, lp);
}

static void handle_local_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{ }

static void handle_local_rev_event(
	       proc_state * ns,
	       tw_bf * b,
	       proc_msg * m,
	       tw_lp * lp)
{ }

/* reverse handler for kickoff */
static void handle_kickoff_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: handle_kickoff_rev_event. TIME now:%f.\n", ns->my_pe_num, now);
#endif
    PE_set_busy(ns->my_pe, false);
    undone_task(ns, PE_getFirstTask(ns->my_pe), lp);
    ns->msg_sent_count--;
    ns->current_task = PE_getFirstTask(ns->my_pe);
    model_net_event_rc(net_id, lp, m->msg_id.size);
    return;
}

static void handle_recv_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    int task_id = PE_findTaskFromMsg(ns->my_pe, &m->msg_id);
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: handle_recv_event - received from %d id: %d for task: "
        "%d. TIME now:%f.\n", ns->my_pe_num, m->msg_id.pe, m->msg_id.id,
        task_id, now);
#endif
    bool isBusy = PE_is_busy(ns->my_pe);

    if(sync_mode == 3){
        if(task_id >= 0 && PE_getTaskMsgID(ns->my_pe, task_id).pe < 0) {
            m->executed_task = -2;
            return;
        }
        m->incremented_flag = isBusy;
    }
    if(task_id>=0){
        //The matching task should not be already done
        if(PE_get_taskDone(ns->my_pe,task_id)){ //TODO: check this
            assert(0);
        }
        assert(PE_getTaskMsgID(ns->my_pe, task_id).pe > 0);
        PE_invertMsgPe(ns->my_pe, task_id);
        if(!PE_noUnsatDep(ns->my_pe, task_id)){
            assert(0);
        }   
        //Add task to the task buffer
        PE_addToBuffer(ns->my_pe, task_id);

        //Check if pe is busy, if not we can execute the next available task in the buffer
        //else do nothing
        m->executed_task = -1;

        if(!isBusy){
#if DEBUG_PRINT
            printf("PE%d: is not busy, executing the next task.\n",
            ns->my_pe_num);
#endif
            int buffd_task = PE_getNextBuffedMsg(ns->my_pe);
            //Store the executed_task id for reverse handler msg
            if(sync_mode == 3){
                m->executed_task = buffd_task;
                m->fwd_dep_count = 0;
            }
            if(buffd_task != -1){
                exec_task(ns, buffd_task, 1, lp);
            }
        }
        return;
    }
    printf("PE%d: Going beyond hash look up on receiving a message %d:%d\n",
      lpid_to_pe(lp->gid), m->msg_id.pe, m->msg_id.id);
    assert(0);
}

static void handle_exec_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //For exec complete event msg_id contains the task_id for convenience
    int task_id = m->msg_id.id; 
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: handle_exec_event for task_id: %d TIME now:%f.\n",
    ns->my_pe_num, task_id, now);
    PE_printStat(ns->my_pe);
#endif
    PE_set_busy(ns->my_pe, false);
    //Mark the task as done
    PE_set_taskDone(ns->my_pe, task_id, true);

    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    int counter = 0;

    for(int i=0; i<fwd_dep_size; i++){
        if(PE_noUnsatDep(ns->my_pe, fwd_deps[i]) && PE_noMsgDep(ns->my_pe, fwd_deps[i])){
            PE_addToBuffer(ns->my_pe, fwd_deps[i]);
            counter++;
        }
    }
    int buffd_task = PE_getNextBuffedMsg(ns->my_pe);
    //Store the executed_task id for reverse handler msg
    if(sync_mode == 3){
        m->fwd_dep_count = counter;
        m->executed_task = buffd_task;
    }
    if(buffd_task != -1){
        exec_task(ns, buffd_task, 1, lp); //we don't care about the return value?
    }
}

static void undone_task(
            proc_state * ns,
            int task_id,
            tw_lp * lp)
{
    //Mark the task as not done
#if DEBUG_PRINT
    printf("Undo task_id: %d\n", task_id);
#endif
    PE_set_taskDone(ns->my_pe, task_id, false);

    //Deal with the forward dependencies of the task
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);
    for(int i=0; i<fwd_dep_size; i++){
        //if the forward dependency of the task is done
        if(PE_get_taskDone(ns->my_pe, fwd_deps[i])){
            //Recursively mark the forward depencies as not done
            undone_task(ns, fwd_deps[i], lp);
        }
    }

    //Update the currentTask, if this event's task_id is smaller than the currentTask
    //else there is no need to update, currentTask is already behind
    /*
    int currentTask = PE_get_currentTask(ns->my_pe);
    if(currentTask > task_id)
        PE_set_currentTask(ns->my_pe, task_id);
    */
}
static void handle_recv_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    if(m->executed_task == -2) {
        model_net_event_rc(net_id, lp, m->msg_id.size);
        return;
    }
    bool wasBusy = m->incremented_flag;
    PE_set_busy(ns->my_pe, wasBusy);

    int task_id = PE_findTaskFromMsg(ns->my_pe, &m->msg_id);
    PE_invertMsgPe(ns->my_pe, task_id);
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: In reverse handler of recv message with id: %d  task_id: %d."
    " wasBusy: %d. TIME now:%f\n", ns->my_pe_num, m->msg_id.id, task_id,
    wasBusy, now);
#endif

    if(!wasBusy){
        //if the task that I executed was not me
        if(m->executed_task != task_id && m->executed_task != -1){
            PE_addToFrontBuffer(ns->my_pe, m->executed_task);    
            PE_removeFromBuffer(ns->my_pe, task_id);
        }
    }
    else{
        assert(m->executed_task == -1);
        PE_removeFromBuffer(ns->my_pe, task_id);
    }

    model_net_event_rc(net_id, lp, m->msg_id.size);
}
static void handle_exec_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    int task_id = m->msg_id.id;

    //Reverse the state: set the PE as busy, task is not completed yet
    PE_set_busy(ns->my_pe, true);

#if DEBUG_PRINT
    printf("PE%d: In reverse handler of exec task with task_id: %d\n",
    ns->my_pe_num, task_id);
#endif
    
    //mark the task as not done
    PE_set_taskDone(ns->my_pe, task_id, false);
    
    if(m->fwd_dep_count > PE_getBufferSize(ns->my_pe))
        PE_clearMsgBuffer(ns->my_pe);
    else{
        PE_resizeBuffer(ns->my_pe, m->fwd_dep_count);
        if(m->executed_task != -1)
            PE_addToFrontBuffer(ns->my_pe, m->executed_task);
    }
}

//executes the task with the specified id
static unsigned long long exec_task(
            proc_state * ns,
            int task_id,
            int flag,   //determines if the completion event will be sent
            tw_lp * lp)
{
    //Check if the backward dependencies are satisfied
    //If not, do nothing yet
    //If yes, execute the task
    if(!PE_noUnsatDep(ns->my_pe, task_id)){
        //printf("RETURNING ZERO\n");
        assert(0);
    }
    //Check if the task is already done -- safety check?
    if(PE_get_taskDone(ns->my_pe, task_id)){
        //printf("PE:%d WARNING: TASK IS ALREADY DONE: %d\n", lpid_to_pe(PE_get_myNum(ns->my_pe)), task_id);
        assert(0);
    }
    //Check the task does not have any message dependency
    if(!PE_noMsgDep(ns->my_pe, task_id)){
        assert(0);
        //printf("PE:%d Task msg dep is not satisfied: %d\n", lpid_to_pe(PE_get_myNum(ns->my_pe)), task_id);
    }

    //Executing the task, set the pe as busy
    PE_set_busy(ns->my_pe, true);

    //Mark the execution time of the task
    unsigned long long time = PE_getTaskExecTime(ns->my_pe, task_id);
    unsigned long long* execTime = &time;

    //For each entry of the task, create a recv event and send them out to
    //whereever it belongs       
    int msgEntCount= PE_getTaskMsgEntryCount(ns->my_pe, task_id);
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: exec_task task_id: %d, num entries: %d, EXEC_TIME: %llu. TIME"
    " now:%f \n", ns->my_pe_num, task_id, msgEntCount, *execTime, now);
#endif

    int myPE = ns->my_pe_num;
    int nWth = PE_get_numWorkThreads(ns->my_pe);  
    int myNode = myPE/nWth;
    unsigned long long copyTime = codes_local_latency(lp); //TODO! 
    unsigned long long delay = 0; //intra node latency

    for(int i=0; i<msgEntCount; i++){
        MsgEntry* taskEntry = PE_getTaskMsgEntry(ns->my_pe, task_id, i);
        int node = MsgEntry_getNode(taskEntry);
        int thread = MsgEntry_getThread(taskEntry);
        unsigned long long sendOffset = MsgEntry_getSendOffset(taskEntry);
        //printf("\tENTRY node:%d, thread:%d\n", node, thread);
        
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
                    exec_comp(ns, MsgEntry_getID(taskEntry), sendOffset+delay, 1, lp);
                }else{
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(destPE, ns->my_job), sendOffset+delay, lp);
                }
              }
            } else if(node != -100-myNode && node <= -100) {
              int destPE = myNode*nWth - 1;
              for(int i=0; i<nWth; i++)
              {
                destPE++;
                delay += copyTime;
                if(destPE == ns->my_pe_num){
                    exec_comp(ns, MsgEntry_getID(taskEntry), sendOffset+delay, 1, lp);
                }else{
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(destPE, ns->my_job), sendOffset+delay, lp);
                }
              }
            } else if(thread >= 0) {
              int destPE = myNode*nWth + thread;
              delay += copyTime;
              if(destPE == ns->my_pe_num){
                  exec_comp(ns, MsgEntry_getID(taskEntry), sendOffset+delay, 1, lp);
              }else{
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry),
                      pe_to_lpid(destPE, ns->my_job), sendOffset+delay, lp);
              }
            } else if(thread==-1) { // broadcast to all work cores
              int destPE = myNode*nWth - 1;
              for(int i=0; i<nWth; i++)
              {
                destPE++;
                delay += copyTime;
                if(destPE == ns->my_pe_num){
                    exec_comp(ns, MsgEntry_getID(taskEntry), sendOffset+delay, 1, lp);
                }else{
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                        pe_to_lpid(destPE, ns->my_job), sendOffset+delay, lp);
                }
              }
            }
          }
          
          if(node != myNode)
          {
                if(node >= 0){
                    send_msg(ns, MsgEntry_getSize(taskEntry), 
                        MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                            pe_to_lpid(node, ns->my_job), sendOffset+delay, lp);
                }
                else if(node == -1){
                    for(int j=0; j<jobs[ns->my_job].numRanks; j++){
                        if(j == myNode) continue;
                        delay += copyTime;
                        send_msg(ns, MsgEntry_getSize(taskEntry), 
                             MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                                pe_to_lpid(j, ns->my_job), sendOffset+delay, lp);
                    }
                }
                else if(node <= -100 && thread == -1){
                    for(int j=0; j<jobs[ns->my_job].numRanks; j++){
                        if(j == -node-100 || j == myNode) continue;
                        delay += copyTime;
                        send_msg(ns, MsgEntry_getSize(taskEntry), 
                             MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                                pe_to_lpid(j, ns->my_job), sendOffset+delay, lp);
                    }

                }
                else if(node <= -100){
                    for(int j=0; j<jobs[ns->my_job].numRanks; j++){
                        if(j == myNode) continue;
                        delay += copyTime;
                        send_msg(ns, MsgEntry_getSize(taskEntry), 
                             MsgEntry_getPE(taskEntry), MsgEntry_getID(taskEntry), 
                                pe_to_lpid(j, ns->my_job), sendOffset+delay, lp);
                    }
                }
                else{
                    printf("message not supported yet! node:%d thread:%d\n",node,thread);
                }
          }
    }

    PE_execPrintEvt(lp, ns->my_pe, task_id, tw_now(lp));

    //Complete the task
    if(*execTime == 0)
        exec_comp(ns, task_id, codes_local_latency(lp), 0, lp);
    else
        exec_comp(ns, task_id, *execTime, 0, lp);
    //Return the execution time of the task
    return *execTime;
}


//Creates and sends the message
static int send_msg(
        proc_state * ns,
        int size,
        int src_pe,
        int id,
        int dest_id,
        unsigned long long sendOffset,
        tw_lp * lp) {
        proc_msg* m_local = malloc(sizeof(proc_msg));
        proc_msg* m_remote = malloc(sizeof(proc_msg));

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

        /*
        int payload = size;
        int chunk_size = 0;
        //calculate the message payload by rounding the size with the chunk size
        if(size <= chunk_size )
            payload = chunk_size;
        else
            payload = size + chunk_size - size%chunk_size;
        */

        //printf("\t...sending message from %d to %d, size: %d, id:%d with offset: %llu \n", lpid_to_pe(lp->gid), lpid_to_pe(dest_id), size, m_local->msg_id.id, sendOffset);
        model_net_event(net_id, "test", dest_id, size, sendOffset,  sizeof(proc_msg), (const void*)m_remote, sizeof(proc_msg), (const void*)m_local, lp);
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
    /*
    if(recv)
        printf("\tPE:%d Sending and RECV_MSG event to myself for msg id: %d\n", lpid_to_pe(lp->gid), task_id);
    else printf("\tPE:%d Sending and EXEC_COMP event to myself for task: %d\n", lpid_to_pe(lp->gid), task_id);
    */
    //If it's a self event use codes_event_new instead of model_net_event 
    tw_event *e;
    proc_msg *m;
    
    e = codes_event_new(lp->gid, sendOffset + g_tw_lookahead, lp);
    m = (proc_msg*)tw_event_data(e);
    m->msg_id.size = 0;
    m->msg_id.pe = ns->my_pe_num;
    m->msg_id.id = task_id;
    if(recv) {
#if DEBUG_PRINT
        printf("PE%d: Send to PE: %d id: %d at time %f for time %f\n",
        ns->my_pe_num, ns->my_pe_num, task_id, tw_now(lp),
        tw_now(lp) + sendOffset + g_tw_lookahead);
#endif
        m->proc_event_type = RECV_MSG;
    }
    else 
        m->proc_event_type = EXEC_COMP;
    tw_event_send(e);

    return 0;
}

//Utility function to convert pe number to tw_lpid number
//Assuming the servers come first in lp registration in terms of global id
static inline int pe_to_lpid(int pe, int job){
    int server_num = jobs[job].rankMap[pe];
    return (server_num / num_servers_per_rep) * lps_per_rep +
            (server_num % num_servers_per_rep);
}

//Utility function to convert tw_lpid to simulated pe number
//Assuming the servers come first in lp registration in terms of global id
static inline int lpid_to_pe(int lp_gid){
    int server_num =  ((int)(lp_gid / lps_per_rep))*(num_servers_per_rep) +
                      (lp_gid % lps_per_rep);
    return global_rank[server_num].mapsTo;
}
static inline int lpid_to_job(int lp_gid){
    int server_num =  ((int)(lp_gid / lps_per_rep))*(num_servers_per_rep) +
                      (lp_gid % lps_per_rep);
    return global_rank[server_num].jobID;;
}
static inline int pe_to_job(int pe){
    return global_rank[pe].jobID;;
}

