/** \file tracer-driver.c
 * Copyright (c) 2015, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * Written by:
 *     Nikhil Jain <nikhil.jain@acm.org>
 *     Bilge Acun <acun2@illinois.edu>
 *     Abhinav Bhatele <bhatele@llnl.gov>
 *
 * LLNL-CODE-681378. All rights reserved.
 *
 * This file is part of TraceR. For details, see:
 * https://github.com/LLNL/tracer
 * Please also read the LICENSE file for our notice and the LGPL.
 */

/**
 * Trace support for CODES simulations.
 * The simulation will be driven using bigsim traces.
 */

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

#include "bigsim/datatypes.h"
#include "bigsim/CWrapper.h"
#include "bigsim/entities/MsgEntry.h"
#include "bigsim/entities/PE.h"

#if TRACER_OTF_TRACES
#include "bigsim/otf2_reader.h"
#endif

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
int default_mapping;
int total_ranks;
tw_stime *jobTimes;
tw_stime *finalizeTimes;
int num_jobs = 0;
tw_stime soft_delay_mpi = 0;

int* size_replace_by;
int* size_replace_limit;
double time_replace_by = 0;
double time_replace_limit = -1;
double copy_per_byte = 0.0;
double eager_limit = 8192;
int dump_topo_only = 0;
int rank;

#define BCAST_DEGREE  4
#define DEBUG_PRINT 0

/* types of events that will constitute triton requests */
enum proc_event
{
    KICKOFF=1,    /* initial event */
    LOCAL,      /* local event */
    RECV_MSG,   /* bigsim, when received a message */
    BCAST,      /* broadcast --> to be deprecated */
    EXEC_COMP,   /* bigsim, when completed an execution */
    COLL_BCAST, /* Collective impl for bcast */
    COLL_COMPLETE
};

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

struct proc_msg
{
    enum proc_event proc_event_type;
    tw_lpid src;          /* source of this request or ack */
    int iteration;
    TaskPair executed;
    int fwd_dep_count;
    MsgID msgId;
    bool incremented_flag; /* helper for reverse computation */
    int model_net_calls;
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
     (commit_f) NULL,
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

static char lp_io_dir[256] = {'\0'};
const tw_optdef app_opt [] =
{
    TWOPT_GROUP("Model net test case" ),
    TWOPT_CHAR("lp-io-dir", lp_io_dir, "Where to place io output (unspecified -> tracer-out"),
    TWOPT_END()
};

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
    uint64_t seq,
    int dest_id,
    tw_stime timeOffset,
    enum proc_event evt_type,
    tw_lp * lp);

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

static void handle_coll_complete_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static int send_coll_comp(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp);

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

static void handle_coll_complete_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp);

static inline int pe_to_lpid(int pe, int job);
static inline int pe_to_job(int pe);
static inline int lpid_to_pe(int lp_gid);
static inline int lpid_to_job(int lp_gid);

void term_handler (int sig) {
    // Restore the default SIGABRT disposition
    signal(SIGABRT, SIG_DFL);
    // Abort (dumps core)
    abort();
}

int main(int argc, char **argv)
{
    int nprocs;
    int num_nets;
    int *net_ids;
    //printf("\n Config count %d ",(int) config.lpgroups_count);
    g_tw_ts_end = s_to_ns(60*60*24*365); /* one year, in nsecs */
    lp_io_handle handle;

    sync_mode = atoi(&argv[1][(strlen(argv[1])-1)]);

    tw_opt_add(app_opt);
    //g_tw_lookahead = 0.0001;
    g_tw_lookahead = 0.1;
    tw_init(&argc, &argv);

    signal(SIGTERM, term_handler);
    
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
    net_id = net_ids[0];
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
                "modelnet_dragonfly_router", NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_dragonfly", NULL, 1);
        num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_dragonfly_router", NULL, 1);
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
    
    if(net_id == SLIMFLY) {
        num_nics = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_slimfly", NULL, 1);
        num_routers = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "slimfly_router", NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_slimfly", NULL, 1);
        num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "slimfly_router", NULL, 1);
    }

    num_servers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
        "server", NULL, 1);

    total_lps = num_servers + num_nics + num_routers;
    lps_per_rep = num_servers_per_rep + num_nics_per_rep + num_routers_per_rep;

    configuration_get_value_double(&config, "PARAMS", "soft_delay", NULL,
        &soft_delay_mpi);
    if(!rank) 
      printf("Found soft_delay as %f\n", soft_delay_mpi);
    
    configuration_get_value_double(&config, "PARAMS", "copy_per_byte", NULL,
        &copy_per_byte);

    if(!rank) 
      printf("Copy cost per byte is %f ns\n", copy_per_byte);

    configuration_get_value_int(&config, "PARAMS", "dump_topo", NULL,
        &dump_topo_only);

    if(!rank && dump_topo_only) 
      printf("Run to dump topology only\n");

    configuration_get_value_double(&config, "PARAMS", "eager_limit", NULL,
        &eager_limit);

    if(!rank) 
      printf("Eager limit is %f bytes\n", eager_limit);

    int ret;
    if(lp_io_dir[0]) {
      ret = lp_io_prepare(lp_io_dir, 0, &handle, MPI_COMM_WORLD);
    } else {
      ret = lp_io_prepare("tracer-out", LP_IO_UNIQ_SUFFIX, &handle, 
                          MPI_COMM_WORLD);
    }
    assert(ret == 0);

    if(!rank) printf("Begin reading %s\n", tracer_input);

    FILE *jobIn = fopen(tracer_input, "r");
    if(!rank && jobIn == NULL) {
      printf("Unable to open tracer input file %s. Aborting\n", tracer_input);
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    char globalIn[256];
    fscanf(jobIn, "%s", globalIn);

    global_rank = (CoreInf*) malloc(num_servers * sizeof(CoreInf));

    for(int i = 0; i < num_servers; i++) {
        global_rank[i].mapsTo = -1;
        global_rank[i].jobID = -1;
    }

    if(dump_topo_only || strcmp("NA", globalIn) == 0) {
      if(!rank) printf("Using default linear mapping of jobs\n");
      default_mapping = 1;
    } else {
      if(!rank) printf("Reading %s\n", globalIn);
      default_mapping = 0;
      if(rank == 0) {
        int line_data[3], localCount = 0;;
        FILE *gfile = fopen(globalIn, "rb");
        if(gfile == NULL) {
          printf("Unable to open global rank file %s. Aborting\n", globalIn);
          MPI_Abort(MPI_COMM_WORLD, 1);
        }
        while(fread(line_data, sizeof(int), 3, gfile) != 0) {
          global_rank[line_data[0]].mapsTo = line_data[1];
          global_rank[line_data[0]].jobID = line_data[2];
#if DEBUG_PRINT
          printf("Read %d/%d %d %d\n", line_data[0], num_servers,
              global_rank[line_data[0]].mapsTo, global_rank[line_data[0]].jobID);
#endif
          localCount++;
        }
        printf("Read mapping of %d ranks\n", localCount);
        fclose(gfile);
      }
      MPI_Bcast(global_rank, 2 * num_servers, MPI_INT, 0, MPI_COMM_WORLD);
    }

    fscanf(jobIn, "%d", &num_jobs);
    jobs = (JobInf*) malloc(num_jobs * sizeof(JobInf));
    jobTimes = (tw_stime*) malloc(num_jobs * sizeof(tw_stime));
    finalizeTimes = (tw_stime*) malloc(num_jobs * sizeof(tw_stime));
    total_ranks = 0;

    for(int i = 0; i < num_jobs; i++) {
#if TRACER_BIGSIM_TRACES
        char tempTrace[200];
        fscanf(jobIn, "%s", tempTrace);
        sprintf(jobs[i].traceDir, "%s%s", tempTrace, "/bgTrace");
#else
        fscanf(jobIn, "%s", jobs[i].traceDir);
#endif
        fscanf(jobIn, "%s", jobs[i].map_file);
        fscanf(jobIn, "%d", &jobs[i].numRanks);
        fscanf(jobIn, "%d", &jobs[i].numIters);
        total_ranks += jobs[i].numRanks;
        jobs[i].rankMap = (int*) malloc(jobs[i].numRanks * sizeof(int));
        jobs[i].skipMsgId = -1;
        jobTimes[i] = 0;
        finalizeTimes[i] = 0;
        if(!rank) {
          printf("Job %d - ranks %d, trace folder %s, rank file %s, iters %d\n",
            i, jobs[i].numRanks, jobs[i].traceDir, jobs[i].map_file, jobs[i].numIters);
        }
    }

    if(!rank) {
      printf("Done reading meta-information about jobs\n");
    }

    size_replace_limit = new int[num_jobs];
    size_replace_by = new int[num_jobs];
    for(int i = 0; i < num_jobs; i++) {
      size_replace_limit[i] = -1;
      size_replace_by[i] = 0;
    }

    char next = ' ';
    fscanf(jobIn, "%c", &next);
    while(next != ' ') {
      if(next == 'M' || next == 'm') {
        int size_limit, size_by, jobid;
        fscanf(jobIn, "%d %d %d", &jobid, &size_limit, &size_by);
        size_replace_limit[jobid] = size_limit;
        size_replace_by[jobid] = size_by;
        if(!rank)
          printf("Will replace all messages of size greater than %d by %d for job %d\n", 
              size_replace_limit[jobid], size_replace_by[jobid], jobid);
      }
      if(next == 'T' || next == 't') {
        fscanf(jobIn, "%lf %lf", &time_replace_limit, &time_replace_by);
        if(!rank)
          printf("Will replace all methods with exec time greater than %lf by %lf\n", 
              time_replace_limit, time_replace_by);
      }
      if(next == 'E' || next == 'e') {
        double etime;
        int jobid;
        char eName[256];
        fscanf(jobIn, "%d %s %lf", &jobid, eName, &etime);
        if(!rank)
          printf("Will make all events with name %s run for %lf s for job %d\n", 
            eName, etime, jobid);
        addEventSub(jobid, eName, etime, num_jobs);
      }
      next = ' ';
      fscanf(jobIn, "%c", &next);
    }

    int ranks_till_now = 0;
    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
        int num_workers = jobs[i].numRanks;
        jobs[i].rankMap = (int *) malloc(sizeof(int) * num_workers);
        if(default_mapping) {
          for(int local_rank = 0; local_rank < num_workers; local_rank++,
            ranks_till_now++) {
            jobs[i].rankMap[local_rank] = ranks_till_now;
            global_rank[ranks_till_now].mapsTo = local_rank;
            global_rank[ranks_till_now].jobID = i;
          }
        } else {
          if(!rank) printf("Loading map file for job %d from %s\n", i,
              jobs[i].map_file);
          if(rank == 0){ //only rank 0 loads the ranks and broadcasts
            FILE *rfile = fopen(jobs[i].map_file, "rb");
            if(rfile == NULL) {
              printf("Unable to open local rank file %s. Aborting\n",
                  jobs[i].map_file);
              MPI_Abort(MPI_COMM_WORLD, 1);
            }
            fread(jobs[i].rankMap, sizeof(int), num_workers, rfile);
            fclose(rfile);
          }
          MPI_Bcast(jobs[i].rankMap, num_workers, MPI_INT, 0, MPI_COMM_WORLD);
        }
#if DEBUG_PRINT
        if(rank == 0) {
            for(int j = 0; j < num_workers; j++) {
                printf("Job %d %d to %d\n", i, j, jobs[i].rankMap[j]);
            }
        }
#endif
    }

#if TRACER_BIGSIM_TRACES
    //Load all summaries on proc 0 and bcast
    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
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
    }
#else
    //Read in global definitions and Open event files
    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
        if(!rank) printf("Read global definition for job %d from %s\n", i,
                jobs[i].traceDir);
        jobs[i].allData = new AllData;
        jobs[i].reader = readGlobalDefinitions(i, jobs[i].traceDir, 
          jobs[i].allData);
    }
#endif


    tw_run();

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
    
    MPI_Reduce(finalizeTimes, jobTimesMax, num_jobs, MPI_DOUBLE, MPI_MAX, 0,
    MPI_COMM_WORLD);
    if(rank == 0) {
        for(int i = 0; i < num_jobs; i++) {
            printf("Job %d Finalize Time %f s\n", i, ns_to_s(jobTimesMax[i]));
        }
    }
    model_net_report_stats(net_id);
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

    if(dump_topo_only) return;

    //Each server read it's trace
    ns->sim_start = clock();
    ns->my_pe_num = lpid_to_pe(lp->gid);
    ns->my_job = lpid_to_job(lp->gid);

    if(ns->my_pe_num == -1) {
        return;
    }

    ns->my_pe = newPE();
    tw_stime startTime=0;
#if TRACER_BIGSIM_TRACES
    ns->trace_reader = newTraceReader(jobs[ns->my_job].traceDir);
    int tot=0, totn=0, emPes=0, nwth=0;
    TraceReader_loadTraceSummary(ns->trace_reader);
    TraceReader_setOffsets(ns->trace_reader, &(jobs[ns->my_job].offsets));

    TraceReader_readTrace(ns->trace_reader, &tot, &totn, &emPes, &nwth,
                         ns->my_pe, ns->my_pe_num,  ns->my_job, &startTime);
#else 
    TraceReader_readOTF2Trace(ns->my_pe, ns->my_pe_num, ns->my_job, &startTime);
#endif

    /* skew each kickoff event slightly to help avoid event ties later on */
    kickoff_time = startTime + g_tw_lookahead + tw_rand_unif(lp->rng);
    ns->end_ts = 0;
    ns->my_pe->sendSeq = new int64_t[jobs[ns->my_job].numRanks];
    ns->my_pe->recvSeq = new int64_t[jobs[ns->my_job].numRanks];
    for(int i = 0; i < jobs[ns->my_job].numRanks; i++) {
      ns->my_pe->sendSeq[i] = ns->my_pe->recvSeq[i] = 0;
    }

    e = codes_event_new(lp->gid, kickoff_time, lp);
    m =  (proc_msg*)tw_event_data(e);
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
  fflush(stdout);
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
    case BCAST:
      handle_bcast_event(ns, b, m, lp);
      break;
    case EXEC_COMP:
      handle_exec_event(ns, b, m, lp);
      break;
    case COLL_BCAST:
      perform_bcast(ns, -1, lp, m, b, 1);
      break;
    case COLL_COMPLETE:
      handle_coll_complete_event(ns, b, m, lp);
      break;
    default:
      printf("\n Invalid message type %d event %lld ", 
          m->proc_event_type, m);
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
    case BCAST:
      handle_bcast_rev_event(ns, b, m, lp);
      break;
    case EXEC_COMP:
      handle_exec_rev_event(ns, b, m, lp);
      break;
    case COLL_BCAST:
      perform_bcast_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_COMPLETE:
      handle_coll_complete_rev_event(ns, b, m, lp);
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
    if(dump_topo_only) return;

    tw_stime jobTime = ns->end_ts - ns->start_ts;
    tw_stime finalTime = tw_now(lp);

    if(lpid_to_pe(lp->gid) == 0)
        printf("Job[%d]PE[%d]: FINALIZE in %f seconds.\n", ns->my_job,
          ns->my_pe_num, ns_to_s(tw_now(lp)-ns->start_ts));

    PE_printStat(ns->my_pe);

    if(jobTime > jobTimes[ns->my_job]) {
        jobTimes[ns->my_job] = jobTime;
    }
    if(finalTime > finalizeTimes[ns->my_job]) {
        finalizeTimes[ns->my_job] = finalTime;
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
    ns->start_ts = tw_now(lp);

    int my_pe_num = ns->my_pe_num;
    int my_job = ns->my_job;
    clock_t time_till_now = (double)(clock()-ns->sim_start)/CLOCKS_PER_SEC;
    if(my_pe_num == 0 && (my_job == 0 || my_job == num_jobs - 1)) {
        printf("PE%d - LP_GID:%d : START SIMULATION, TASKS COUNT: %d, FIRST "
        "TASK: %d, RUN TIME TILL NOW=%f s, CURRENT SIM TIME %f\n", my_pe_num, 
        (int)lp->gid, PE_get_tasksCount(ns->my_pe), PE_getFirstTask(ns->my_pe),
        (double)time_till_now, ns->start_ts);
    }
  
    //Safety check if the pe_to_lpid converter is correct
    assert(pe_to_lpid(my_pe_num, my_job) == lp->gid);
    assert(PE_is_busy(ns->my_pe) == false);
    TaskPair pair;
    pair.iter = 0; pair.taskid = PE_getFirstTask(ns->my_pe);
    exec_task(ns, pair, lp, m, b);
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
    TaskPair pair;
    pair.iter = 0; pair.taskid = PE_getFirstTask(ns->my_pe);
    exec_task_rev(ns, pair, lp, m, b);
    return;
}

static void handle_recv_event(
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
    printf("[%d:%d] RECD MSG: %d %d %d %d\n", ns->my_job,
        ns->my_pe_num, m->msgId.pe, m->msgId.id, m->msgId.comm,
        m->msgId.seq);
#endif
    MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
    KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
    if(it == ns->my_pe->pendingMsgs.end()) {
      task_id = -1;
      ns->my_pe->pendingMsgs[key].push_back(task_id);
      b->c2 = 1;
      return;
    } else {
      b->c3 = 1;
      task_id = it->second.front();
      it->second.pop_front();
      if(it->second.size() == 0) {
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
        if(m->msgId.size <= eager_limit) {
          b->c1 = 1;
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
            if(sync_mode == 3){
                m->executed = buffd_task;
                m->fwd_dep_count = 0;
            }
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

static void handle_bcast_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {

  tw_stime soft_latency = codes_local_latency(lp);
  m->model_net_calls = 0;
  int num_sends = bcast_msg(ns, m->msgId.size, m->iteration, &m->msgId,
      0, soft_latency, lp, m);

  if(!num_sends) num_sends++;

  tw_event*  e = codes_event_new(lp->gid, num_sends * soft_latency + codes_local_latency(lp), lp);
  proc_msg * msg = (proc_msg*)tw_event_data(e);
  memcpy(&msg->msgId, &m->msgId, sizeof(m->msgId));
  msg->iteration = m->iteration;
  msg->proc_event_type = RECV_MSG;
  tw_event_send(e);
}

static void handle_exec_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //For exec complete event msgId contains the task_id for convenience
    int task_id = m->msgId.id;
    int iter = m->iteration;
    PE_set_busy(ns->my_pe, false);
    //Mark the task as done
    PE_set_taskDone(ns->my_pe, iter, task_id, true);

    int counter = 0;
#if TRACER_BIGSIM_TRACES
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);

    if(PE_isLoopEvent(ns->my_pe, task_id) && (PE_get_iter(ns->my_pe) != (jobs[ns->my_job].numIters - 1))) {
      b->c1 = 1;
      PE_mark_all_done(ns->my_pe, iter, task_id);
      PE_inc_iter(ns->my_pe);
      TaskPair pair;
      pair.iter = PE_get_iter(ns->my_pe); pair.taskid = PE_getFirstTask(ns->my_pe);
      PE_addToBuffer(ns->my_pe, &pair);
      counter++;
    } else {
      for(int i=0; i<fwd_dep_size; i++){
        if(PE_noUnsatDep(ns->my_pe, iter, fwd_deps[i]) && PE_noMsgDep(ns->my_pe, iter, fwd_deps[i])){
          TaskPair pair;
          pair.iter = iter; pair.taskid = fwd_deps[i];
          PE_addToBuffer(ns->my_pe, &pair);
          counter++;
        }
      }
    }
#else 
    if(ns->my_pe->loop_start_task != -1 && 
       PE_isLoopEvent(ns->my_pe, task_id) && 
       (PE_get_iter(ns->my_pe) != (jobs[ns->my_job].numIters - 1))) {
      b->c1 = 1;
      PE_mark_all_done(ns->my_pe, iter, task_id);
      PE_inc_iter(ns->my_pe);
      TaskPair pair;
      pair.iter = PE_get_iter(ns->my_pe); 
      pair.taskid = ns->my_pe->loop_start_task;
      PE_addToBuffer(ns->my_pe, &pair);
      counter = 1;
    } else {
      if(task_id != PE_get_tasksCount(ns->my_pe) - 1) {
        TaskPair pair;
        pair.iter = iter; pair.taskid = task_id + 1;
        PE_addToBuffer(ns->my_pe, &pair);
        counter = 1;
      }
    }
#endif

    TaskPair buffd_task = PE_getNextBuffedMsg(ns->my_pe);
    //Store the executed_task id for reverse handler msg
    if(sync_mode == 3){
        m->fwd_dep_count = counter;
        m->executed = buffd_task;
    }
    if(buffd_task.taskid != -1){
        exec_task(ns, buffd_task, lp, m, b); //we don't care about the return value?
    }
}

static void handle_recv_rev_event(
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
    if(m->executed.taskid == -2) {
        return;
    }
    bool wasBusy = m->incremented_flag;
    int iter = m->iteration;
    PE_set_busy(ns->my_pe, wasBusy);

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
      PE_addTaskExecTime(ns->my_pe, task_id, -1 * m->msgId.size * copy_per_byte);
    }

    if(!wasBusy){
        //if the task that I executed was not me
        if(m->executed.taskid != task_id && m->executed.taskid != -1){
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

static void handle_bcast_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  codes_local_latency_reverse(lp);
  codes_local_latency_reverse(lp);
  for(int i = 0; i < m->model_net_calls; i++) {
    model_net_event_rc(net_id, lp, 0);
  }
}

static void handle_exec_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    int task_id = m->msgId.id;

    //Reverse the state: set the PE as busy, task is not completed yet
    PE_set_busy(ns->my_pe, true);

#if DEBUG_PRINT
    printf("PE%d: In reverse handler of exec task with task_id: %d\n",
    ns->my_pe_num, task_id);
#endif
    
    //mark the task as not done
    int iter = m->iteration;
    PE_set_taskDone(ns->my_pe, iter, task_id, false);
     
    if(b->c1) {
      PE_dec_iter(ns->my_pe);
    }
    
    if(m->fwd_dep_count > PE_getBufferSize(ns->my_pe)) {
        PE_clearMsgBuffer(ns->my_pe);
    } else {
        PE_resizeBuffer(ns->my_pe, m->fwd_dep_count);
        if(m->executed.taskid != -1) {
            PE_addToFrontBuffer(ns->my_pe, &m->executed);
        }
    }
    if(m->executed.taskid != -1) {
      exec_task_rev(ns, m->executed, lp, m, b);
    }
}

//executes the task with the specified id
static tw_stime exec_task(
            proc_state * ns,
            TaskPair task_id,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b)
{
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

    m->model_net_calls = 0;
#if TRACER_OTF_TRACES
    Task *t = &ns->my_pe->myTasks[task_id.taskid];

    //delegate to routine that handles collectives
    if(t->event_id == TRACER_COLL_EVT) {
      b->c11 = 1;
      perform_collective(ns, task_id.taskid, lp, m, b);
      return 0;
    }

    //else continue
    if(t->event_id == TRACER_RECV_EVT && !PE_noMsgDep(ns->my_pe, task_id.iter, task_id.taskid)) {
      MsgKey key(t->myEntry.node, t->myEntry.msgId.id, t->myEntry.msgId.comm, ns->my_pe->recvSeq[t->myEntry.node]);
      ns->my_pe->recvSeq[t->myEntry.node]++;
      KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
      if(it == ns->my_pe->pendingMsgs.end()) {
        ns->my_pe->pendingMsgs[key].push_back(task_id.taskid);
#if DEBUG_PRINT
        printf("[%d:%d] PUSH TASK: %d - %d %d %d %d\n", ns->my_job,
          ns->my_pe_num, task_id.taskid, t->myEntry.node, t->myEntry.msgId.id,
            t->myEntry.msgId.comm, ns->my_pe->recvSeq[t->myEntry.node]-1);
#endif
        b->c21 = 1;
        return 0;
      } else {
        b->c22 = 1;
        ns->my_pe->pendingMsgs[key].pop_front();
        if(it->second.size() == 0) {
          ns->my_pe->pendingMsgs.erase(it);
        }
      }
    }
#endif

    //Executing the task, set the pe as busy
    PE_set_busy(ns->my_pe, true);
    //Mark the execution time of the task
    tw_stime time = PE_getTaskExecTime(ns->my_pe, task_id.taskid);

#if TRACER_BIGSIM_TRACES
    //For each entry of the task, create a recv event and send them out to
    //whereever it belongs       
    int msgEntCount= PE_getTaskMsgEntryCount(ns->my_pe, task_id.taskid);
    int myPE = ns->my_pe_num;
    int nWth = PE_get_numWorkThreads(ns->my_pe);  
    int myNode = myPE/nWth;
    tw_stime soft_latency = codes_local_latency(lp);
    tw_stime delay = soft_latency; //intra node latency
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
    tw_stime soft_latency = codes_local_latency(lp);
    tw_stime delay = soft_latency; //intra node latency
    double sendFinishTime = 0;

    if(t->event_id == TRACER_SEND_EVT) {
      b->c23 = 1;
      MsgEntry *taskEntry = &t->myEntry;
      tw_stime copyTime = copy_per_byte * MsgEntry_getSize(taskEntry);
      if(MsgEntry_getSize(taskEntry) > eager_limit) {
        copyTime = soft_latency;
      }
      int node = MsgEntry_getNode(taskEntry);
      tw_stime sendOffset = soft_delay_mpi;

      if(node == ns->my_pe_num) {
        exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 
          taskEntry->msgId.comm, sendOffset+delay, 1, lp);
      } else {
        m->model_net_calls++;
#if DEBUG_PRINT
        printf("[%d:%d] SEND TO: %d - %d %d %d\n", ns->my_job, ns->my_pe_num,
          node, taskEntry->msgId.id, taskEntry->msgId.comm, ns->my_pe->sendSeq[node]);
#endif
          
        send_msg(ns, MsgEntry_getSize(taskEntry),
            task_id.iter, &taskEntry->msgId, ns->my_pe->sendSeq[node]++,
            pe_to_lpid(node, ns->my_job), sendOffset+delay, RECV_MSG, lp);
      }
      sendFinishTime = delay;
    }
   
    //print event
    if(t->event_id >= 0) {
      char str[1000];
      if(t->beginEvent) {
        strcpy(str, "[%d %d : Begin %s %f]\n");
      } else {
        strcpy(str, "[%d %d : End %s %f]\n");
      }
      tw_output(lp, str, ns->my_job, ns->my_pe_num, 
          jobs[ns->my_job].allData->strings[jobs[ns->my_job].allData->regions[t->event_id].name].c_str(),
          tw_now(lp)/((double)TIME_MULT));
    }

    if(t->loopStartEvent) {
      ns->my_pe->loop_start_task = task_id.taskid;
    }
#endif

    //Complete the task
    tw_stime finish_time = codes_local_latency(lp) + 
                              ((sendFinishTime > time) ? sendFinishTime : time);
    exec_comp(ns, task_id.iter, task_id.taskid, 0, finish_time, 0, lp);
    if(PE_isEndEvent(ns->my_pe, task_id.taskid)) {
      ns->end_ts = tw_now(lp);
    }
    //Return the execution time of the task
    return time;
}

static void exec_task_rev(
    proc_state * ns,
    TaskPair task_id,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b) {

#if TRACER_OTF_TRACES
  if(b->c11) {
    perform_collective_rev(ns, task_id.taskid, lp, m, b);
    return;
  }

  if(b->c21 || b->c22) {
    Task *t = &ns->my_pe->myTasks[task_id.taskid];
    ns->my_pe->recvSeq[t->myEntry.node]--;
    MsgKey key(t->myEntry.node, t->myEntry.msgId.id, 
        t->myEntry.msgId.comm, ns->my_pe->recvSeq[t->myEntry.node]);
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

  codes_local_latency_reverse(lp);
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }
#if TRACER_OTF_TRACES
  if(b->c23) {
    Task *t = &ns->my_pe->myTasks[task_id.taskid];
    MsgEntry *taskEntry = &t->myEntry;
    ns->my_pe->sendSeq[MsgEntry_getNode(taskEntry)]--;
  }
#endif
  codes_local_latency_reverse(lp);
}

//Creates and sends the message
static int send_msg(
        proc_state * ns,
        int size,
        int iter,
        MsgID *msgId,
        uint64_t seq,
        int dest_id,
        tw_stime sendOffset,
        enum proc_event evt_type,
        tw_lp * lp) {
        proc_msg* m_remote = (proc_msg *)malloc(sizeof(proc_msg));

        m_remote->proc_event_type = evt_type;
        m_remote->src = lp->gid;
        m_remote->msgId.size = size;
        m_remote->msgId.pe = msgId->pe;
        m_remote->msgId.id = msgId->id;
#if TRACER_OTF_TRACES
        m_remote->msgId.comm = msgId->comm;
        m_remote->msgId.seq = seq;
#endif
        m_remote->iteration = iter;

        /*   model_net_event params:
             int net_id, char* category, tw_lpid final_dest_lp,
             uint64_t message_size, tw_stime offset, int remote_event_size,
             const void* remote_event, int self_event_size,
             const void* self_event, tw_lp *sender */

        model_net_event(net_id, "test", dest_id, size, sendOffset,
          sizeof(proc_msg), (const void*)m_remote, 0, NULL, lp);
        ns->msg_sent_count++;
        free(m_remote);
    
    return 0;
}

static void perform_collective(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b) {
  Task *t = &ns->my_pe->myTasks[taskid];
  assert(t->event_id == TRACER_COLL_EVT);
  if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST) {
    perform_bcast(ns, taskid, lp, m, b, 0);
  } else {
    assert(0);
  }
}

static void perform_collective_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b) {
  Task *t = &ns->my_pe->myTasks[taskid];
  if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST) {
    perform_bcast_rev(ns, taskid, lp, m, b, 0);
  } else {
    assert(0);
  }
}

static void perform_bcast(
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
    std::map<int64_t, std::map<int64_t, std::vector<int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.find(t->myEntry.msgId.comm);
    if(it == ns->my_pe->pendingCollMsgs.end()) {
      recvCount = 0;
    } else {
      std::map<int64_t, std::vector<int> >::iterator cIt =
        it->second.find(collSeq);
      if(cIt == it->second.end()) {
        recvCount = 0;
      } else {
        recvCount = cIt->second[0];
      }
    }
  } else {
    int comm = m->msgId.comm;
    int64_t collSeq = m->msgId.seq;
    if(comm != ns->my_pe->currentCollComm ||
       collSeq != ns->my_pe->currentCollSeq) {
      ns->my_pe->pendingCollMsgs[comm][collSeq].push_back(1);
      b->c12 = 1;
      return;
    }
    t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  }

  int amIroot = (ns->my_pe->myNum == t->myEntry.msgId.pe);

  if(recvCount == 0 && !amIroot) {
    b->c13 = 1;
    return;
  }

  int numValidChildren = 0;
  int myChildren[BCAST_DEGREE];
  int thisTreePe, index = ns->my_pe_num, maxSize = jobs[ns->my_job].numRanks;

  Group &g = jobs[ns->my_job].allData->groups[jobs[ns->my_job].allData->communicators[ns->my_pe->currentCollComm]];
  if(jobs[ns->my_job].numRanks != g.members.size()) {
    index = std::find(g.members.begin(), g.members.end(), ns->my_pe_num)
      - g.members.begin();
    maxSize = g.members.size();
  }

  thisTreePe = (index - t->myEntry.node + maxSize) % maxSize;

  for(int i = 0; i < BCAST_DEGREE; i++) {
    int next_child = BCAST_DEGREE * thisTreePe + i + 1;
    if(next_child >= maxSize) {
      break;
    }
    myChildren[i] = (t->myEntry.node + next_child) % maxSize;
    numValidChildren++;
  }

  tw_stime delay = 0, copyTime = codes_local_latency(lp);
  m->model_net_calls = 0;
  for(int i = 0; i < numValidChildren; i++) {
    int dest = myChildren[i];
    if(jobs[ns->my_job].numRanks != g.members.size()) {
      dest = g.members[dest];
    }
    send_msg(ns, t->myEntry.msgId.size, ns->my_pe->currIter,
      &t->myEntry.msgId,  ns->my_pe->currentCollSeq, pe_to_lpid(dest, ns->my_job),
      soft_delay_mpi + delay, COLL_BCAST, lp);
    delay += copyTime;
    m->model_net_calls++;
  }
  send_coll_comp(ns, delay, OTF2_COLLECTIVE_OP_BCAST, lp);
}

static void perform_bcast_rev(
    proc_state * ns,
    int taskid,
    tw_lp * lp,
    proc_msg *m,
    tw_bf * b,
    int isEvent) {
  Task *t;
  if(!isEvent) {
    t = &ns->my_pe->myTasks[taskid];
    ns->my_pe->currentCollComm = ns->my_pe->currentCollTask =
    ns->my_pe->currentCollSeq = -1;
    ns->my_pe->collectiveSeq[t->myEntry.msgId.comm]--;
  } else {
    if(b->c12) {
      ns->my_pe->pendingCollMsgs[m->msgId.comm].erase(m->msgId.seq);
      return;
    }
  }

  if(b->c13) return;
  
  codes_local_latency_reverse(lp);
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }
}

static void handle_coll_complete_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {
  Task *t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
  m->executed.taskid = ns->my_pe->currentCollTask;
  m->msgId.seq = ns->my_pe->currentCollSeq;
  m->msgId.comm = ns->my_pe->currentCollComm;
  if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST &&
     m->msgId.coll_type == OTF2_COLLECTIVE_OP_BCAST) {
    exec_comp(ns, ns->my_pe->currIter, ns->my_pe->currentCollTask, 0,
      codes_local_latency(lp), 0, lp);
    ns->my_pe->currentCollTask = ns->my_pe->currentCollComm =
      ns->my_pe->currentCollSeq = -1;
  }
}

static void handle_coll_complete_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {
  ns->my_pe->currentCollTask = m->executed.taskid;
  ns->my_pe->currentCollSeq = m->msgId.seq;
  ns->my_pe->currentCollComm = m->msgId.comm;
}

//Creates and initiates bcast_msg
static int bcast_msg(
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
      delay += copyTime;
      m->model_net_calls++;
    }
    return numValidChildren;
}

static int exec_comp(
    proc_state * ns,
    int iter,
    int task_id,
    int comm_id,
    tw_stime sendOffset,
    int recv,
    tw_lp * lp)
{
    //If it's a self event use codes_event_new instead of model_net_event 
    tw_event *e;
    proc_msg *m;

    if(sendOffset < g_tw_lookahead) {
      sendOffset += g_tw_lookahead;
    }
    e = codes_event_new(lp->gid, sendOffset, lp);
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
    }
    else 
        m->proc_event_type = EXEC_COMP;
    tw_event_send(e);

    return 0;
}

static int send_coll_comp(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp)
{
    tw_event *e;
    proc_msg *m;

    if(sendOffset < g_tw_lookahead) {
      sendOffset += g_tw_lookahead;
    }
    e = codes_event_new(lp->gid, sendOffset, lp);
    m = (proc_msg*)tw_event_data(e);
    m->msgId.coll_type = collType;
    m->proc_event_type = COLL_COMPLETE;
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

bool isPEonThisRank(int jobID, int i) {
  int lpid = pe_to_lpid(i, jobID);
  int pe = codes_mapping(lpid);
  return pe == rank;
}
