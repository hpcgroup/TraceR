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
 * Trace replay utility for application simulation using CODES.
 * The simulation is driven using OTF or Bigsim traces.
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

#include "tracer-driver.h"

static int net_id = 0;
static int num_routers = 0;
static int num_servers = 0;
static int num_nics = 0;
static int num_schedulers = 0;

static int num_routers_per_rep = 0;
static int num_servers_per_rep = 0;
static int num_nics_per_rep = 0;
static int lps_per_rep = 0;
static int total_lps = 0;

typedef struct proc_msg proc_msg;
typedef struct proc_state proc_state;

unsigned int print_frequency = 5000;

#define TRACER_A2A_ALG_CUTOFF 512
#define TRACER_ALLGATHER_ALG_CUTOFF 163840
#define TRACER_BLOCK_SIZE 32

char tracer_input[256];
proc_state *ns_5 = NULL;

//the indexing should match between the define and the array
#define TRACER_A2A 0
#define TRACER_ALLGATHER 1
#define TRACER_BRUCK 2
#define TRACER_BLOCKED 3
Coll_lookup lookUpTable[] = { { COLL_A2A, COLL_A2A_SEND_DONE },
                              { COLL_ALLGATHER, COLL_ALLGATHER_SEND_DONE },
                              { COLL_BRUCK, COLL_BRUCK_SEND_DONE },
                              { COLL_A2A_BLOCKED, COLL_A2A_BLOCKED_SEND_DONE }
                            };
enum tracer_coll_type
{
  TRACER_COLLECTIVE_BCAST=1,
  TRACER_COLLECTIVE_REDUCE,
  TRACER_COLLECTIVE_BARRIER,
  TRACER_COLLECTIVE_ALLTOALL_LARGE,
  TRACER_COLLECTIVE_ALLTOALL_BLOCKED,
  TRACER_COLLECTIVE_ALL_BRUCK,
  TRACER_COLLECTIVE_ALLGATHER_LARGE,
};

JobInf *jobs;
int default_mapping;
tw_stime *jobTimes;
tw_stime *finalizeTimes;
int num_jobs = 0;
tw_stime soft_delay_mpi = 100;
tw_stime nic_delay = 400;
tw_stime rdma_delay = 1000;

int* size_replace_by;
int* size_replace_limit;
double time_replace_by = 0;
double time_replace_limit = -1;
double copy_per_byte = 0.0;
double eager_limit = 8192;
int dump_topo_only = 0;
int rank;

#define DEBUG_PRINT 0

tw_lptype sched_lp = {
     (init_f) sched_init,
     (pre_run_f) NULL,
     (event_f) sched_event,
     (revent_f) sched_rev_event,
     (commit_f) NULL,
     (final_f)  sched_finalize,
     (map_f) codes_mapping,
     sizeof(sched_state),
};

tw_lptype proc_lp = {
     (init_f) proc_init,
     (pre_run_f) NULL,
     (event_f) proc_event,
     (revent_f) proc_rev_event,
     (commit_f) proc_commit,
     (final_f)  proc_finalize, 
     (map_f) codes_mapping,
     sizeof(proc_state),
};

extern const tw_lptype* sched_get_lp_type();
extern const tw_lptype* proc_get_lp_type();
static void sched_add_lp_type();
static void proc_add_lp_type();
static tw_stime ns_to_s(tw_stime ns);
static tw_stime s_to_ns(tw_stime ns);

static char lp_io_dir[256] = {'\0'};
const tw_optdef app_opt [] =
{
    TWOPT_GROUP("Model net test case" ),
    TWOPT_CHAR("lp-io-dir", lp_io_dir, "Where to place io output (unspecified -> tracer-out"),
    TWOPT_UINT("timer-frequency", print_frequency, "Frequency for printing timers, #tasks (unspecified -> 5000"),
    TWOPT_END()
};

static inline int sched_lpid();
static inline int pe_to_lpid(int pe, int job);

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
    g_tw_ts_end = s_to_ns(60*60); /* one hour, in nsecs */
    lp_io_handle handle;

    tw_opt_add(app_opt);
    g_tw_lookahead = 0.1;
    tw_init(&argc, &argv);

    signal(SIGTERM, term_handler);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

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
    sched_add_lp_type();
    
    codes_mapping_setup();
    
    num_servers = codes_mapping_get_lp_count("MODELNET_GRP", 0, "server", 
            NULL, 1);
    num_schedulers = codes_mapping_get_lp_count("SCHEDULER_GRP", 0, "scheduler",
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

    if(net_id == DRAGONFLY_CUSTOM) {
        num_nics = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_dragonfly_custom", NULL, 1);
        num_routers = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_dragonfly_custom_router", NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_dragonfly_custom", NULL, 1);
        num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_dragonfly_custom_router", NULL, 1);
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

    if(net_id == EXPRESS_MESH) {
        num_nics = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_express_mesh", NULL, 1);
        num_routers = codes_mapping_get_lp_count("MODELNET_GRP", 0,
                "modelnet_express_mesh_router", NULL, 1);
        num_nics_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_express_mesh", NULL, 1);
        num_routers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
                "modelnet_express_mesh_router", NULL, 1);
    }

    num_servers_per_rep = codes_mapping_get_lp_count("MODELNET_GRP", 1,
        "server", NULL, 1);

    total_lps = num_servers + num_nics + num_routers + num_schedulers;
    lps_per_rep = num_servers_per_rep + num_nics_per_rep + num_routers_per_rep;


    configuration_get_value_double(&config, "PARAMS", "soft_delay", NULL,
        &soft_delay_mpi);
    if(!rank) 
      printf("Found soft_delay as %f\n", soft_delay_mpi);
    
    configuration_get_value_double(&config, "PARAMS", "nic_delay", NULL,
        &nic_delay);
    if(!rank) 
      printf("Found nic_delay as %f\n", nic_delay);
    
    configuration_get_value_double(&config, "PARAMS", "rdma_delay", NULL,
        &rdma_delay);
    if(!rank) 
      printf("Found rdma_delay as %f\n", rdma_delay);
    
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

    if(dump_topo_only || strcmp("NA", globalIn) == 0) {
      if(!rank) printf("Using default linear mapping of jobs\n");
      default_mapping = 1;
    } else {
      default_mapping = 0;
    }

    fscanf(jobIn, "%d", &num_jobs);
    jobs = (JobInf*) malloc(num_jobs * sizeof(JobInf));
    jobTimes = (tw_stime*) malloc(num_jobs * sizeof(tw_stime));
    finalizeTimes = (tw_stime*) malloc(num_jobs * sizeof(tw_stime));

    // TODO: Added for testing purpose, remove later
    // This block can be used to reduce the count of servers
    // to a specified number, provided in tracer_config file
#if 0
    int simulated_ranks;
    char buf[10];
    if(fgets(buf, 10, jobIn) != NULL) {
        if(sscanf(buf, "%d", &simulated_ranks) < 1) {
            simulated_ranks = 0;
        }
    }
    if((simulated_ranks > 0) && (simulated_ranks < num_servers)) {
        num_servers = simulated_ranks;
    }
#endif

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
        if(jobs[i].numRanks > num_servers) {
            printf("Number of ranks required by job %d: %d is greater than simulated ranks: %d. Aborting\n",
                   i, jobs[i].numRanks, num_servers);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        fscanf(jobIn, "%d", &jobs[i].numIters);
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
      if(next == 'S' || next == 's') {
        int size_value, size_by, jobid;
        fscanf(jobIn, "%d %d %d", &jobid, &size_value, &size_by);
        addMsgSizeSub(jobid, size_value, size_by, num_jobs);
        if(!rank)
          printf("Will replace all messages of size %d by %d for job %d\n",
              size_value, size_by, jobid);
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
          printf("Will make all events with name %s run for %lf s for job %d; if scale_all, events will be scaled down\n", 
            eName, etime, jobid);
        addEventSub(jobid, eName, etime, num_jobs);
      }
      next = ' ';
      fscanf(jobIn, "%c", &next);
    }
    fclose(jobIn);

    int ranks_till_now = 0;
    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
        int num_workers = jobs[i].numRanks;
        jobs[i].rankMap = (int *) malloc(sizeof(int) * num_workers);
        if(default_mapping) {
          if(ranks_till_now + num_workers > num_servers) {
              ranks_till_now = 0;
          }
          for(int local_rank = 0; local_rank < num_workers; local_rank++,
            ranks_till_now++) {
            jobs[i].rankMap[local_rank] = ranks_till_now;
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
            TraceReader_setOffsets(t, NULL);
        } else {
            jobs[i].offsets = new int[num_workers];
        }
        MPI_Bcast(jobs[i].offsets, num_workers, MPI_INT, 0, MPI_COMM_WORLD);
        deleteTraceReader(t);
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
    free(jobTimesMax);

#if TRACER_BIGSIM_TRACES
    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
      delete [] jobs[i].offsets;
    }
#else
    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
      closeReader(jobs[i].reader);
      delete jobs[i].allData;
    }
#endif

    for(int i = 0; i < num_jobs && !dump_topo_only; i++) {
      free(jobs[i].rankMap);
    }
    delete [] size_replace_limit;
    delete [] size_replace_by;

    free(finalizeTimes);
    free(jobTimes);
    free(jobs);

    tw_end();

    return 0;
}

const tw_lptype* proc_get_lp_type()
{
    return(&proc_lp);
}

const tw_lptype* sched_get_lp_type()
{
    return(&sched_lp);
}

static void proc_add_lp_type()
{
    lp_type_register("server", proc_get_lp_type());
}

static void sched_add_lp_type()
{
    lp_type_register("scheduler", sched_get_lp_type());
}

static void schedule_jobs(
    sched_state * ss,
    tw_lp * lp)
{
    int last_job = ss->last_scheduled_job;
    for(int j = ss->last_scheduled_job + 1; j < num_jobs; j++) {
        for(int p = 0; p < jobs[j].numRanks; p++) {
            if(ss->busy_lps.find(pe_to_lpid(p, j)) != ss->busy_lps.end()) {
                return;
            }
        }
#if DEBUG_PRINT
        printf("SCHED: Starting job %d\n", j);
        fflush(stdout);
#endif
        for(int p = 0; p < jobs[j].numRanks; p++) {
            /* skew each job start event slightly to help avoid event ties later on */
            tw_stime start_time = g_tw_lookahead + tw_rand_unif(lp->rng);
            tw_event *e = codes_event_new(pe_to_lpid(p, j), start_time, lp);
            proc_msg *m =  (proc_msg*)tw_event_data(e);
            m->proc_event_type = JOB_START;
            m->msgId.id = j;
            m->msgId.pe = p;
            tw_event_send(e);

            ss->busy_lps.insert(pe_to_lpid(p, j));
        }
        ss->last_scheduled_job = j;
        ss->completed_ranks[j] = 0;
    }
}

static void sched_init(
    sched_state * ss,
    tw_lp * lp)
{
    // There should be only one scheduler LP
    assert(lp->gid == sched_lpid());
    if(dump_topo_only) {
        return;
    }

    ss->completed_ranks = std::map<int, unsigned int>();
    ss->busy_lps = std::set<tw_lpid>();
    ss->last_scheduled_job = -1;

    sched_jobs(ss, lp);
}

static void sched_event(
    sched_state * ss,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    if(m->proc_event_type != JOB_END) {
        return;
    }

    int job_num = m->msgId.id;
    ss->completed_ranks[job_num]++;
    if(ss->completed_ranks[job_num] == jobs[job_num].numRanks) {
        ss->completed_ranks.erase(job_num);
        for(int p = 0; p < jobs[job_num].numRanks; p++) {
            ss->busy_lps.erase(pe_to_lpid(p, job_num));
        }
#if DEBUG_PRINT
        printf("SCHED: Job %d finished executing\n", job_num);
        fflush(stdout);
#endif
        m->saved_task = ss->last_scheduled_job;
        schedule_jobs(ss, lp);
    }
}

static void sched_rev_event(
    sched_state * ss,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    if(m->proc_event_type != JOB_END) {
        return;
    }

    int job_num = m->msgId.id;
    if(ss->completed_ranks.find(job_num) == ss->completed_ranks.end()) {
#if DEBUG_PRINT
        printf("SCHED: Reversing job %d finish\n", job_num);
        fflush(stdout);
#endif
        for(int j = ss->last_scheduled_job; j > m->saved_task; j--) {
#if DEBUG_PRINT
            printf("SCHED: Reversing job %d start\n", j);
            fflush(stdout);
#endif
            for(int p = 0; p < jobs[j].numRanks; p++) {
                tw_rand_reverse_unif(lp->rng);
                ss->busy_lps.erase(pe_to_lpid(p, j));
            }
        }

        ss->last_scheduled_job = m->saved_task;

        ss->completed_ranks[job_num] = jobs[job_num].numRanks;
        for(int p = 0; p < jobs[job_num].numRanks; p++) {
            ss->busy_lps.insert(pe_to_lpid(p, job_num));
        }
    }
    ss->completed_ranks[job_num]--;
}

static void sched_finalize(
    sched_state * ss,
    tw_lp * lp)
{
    if(dump_topo_only) {
        return;
    }

    ss->completed_ranks.clear();
}

static void proc_init(
    proc_state * ns,
    tw_lp * lp) {
    tw_event *e;
    proc_msg *m;
    tw_stime start_time;
    
    memset(ns, 0, sizeof(*ns));

    if(dump_topo_only) return;

    ns->sim_start = clock();
    ns->old_pes = std::map<int, PE*>();

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
    case JOB_START:
      handle_job_start_event(ns, b, m, lp);
      break;
    case JOB_END:
      handle_job_end_event(ns, b, m, lp);
      break;
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
    case EXEC_COMPLETE:
      handle_exec_event(ns, b, m, lp);
      break;
    case SEND_COMP:
      handle_send_comp_event(ns, b, m, lp);
      break;
    case RECV_POST:
      handle_recv_post_event(ns, b, m, lp);
      break;
    case COLL_BCAST:
      perform_bcast(ns, -1, lp, m, b, 1);
      break;
    case COLL_REDUCTION:
      perform_reduction(ns, -1, lp, m, b, 1);
      break;
    case COLL_A2A:
      perform_a2a(ns, -1, lp, m, b, 1);
      break;
    case COLL_A2A_SEND_DONE:
      handle_a2a_send_comp_event(ns, b, m, lp);
      break;
    case COLL_ALLGATHER:
      perform_allgather(ns, -1, lp, m, b, 1);
      break;
    case COLL_ALLGATHER_SEND_DONE:
      handle_allgather_send_comp_event(ns, b, m, lp);
      break;
    case COLL_BRUCK:
      perform_bruck(ns, -1, lp, m, b, 1);
      break;
    case COLL_BRUCK_SEND_DONE:
      handle_bruck_send_comp_event(ns, b, m, lp);
      break;
    case COLL_A2A_BLOCKED:
      perform_a2a_blocked(ns, -1, lp, m, b, 1);
      break;
    case COLL_A2A_BLOCKED_SEND_DONE:
      handle_a2a_blocked_send_comp_event(ns, b, m, lp);
      break;
    case RECV_COLL_POST:
      handle_coll_recv_post_event(ns, b, m, lp);
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
    case JOB_START:
      handle_job_start_rev_event(ns, b, m, lp);
      break;
    case JOB_END:
      handle_job_end_rev_event(ns, b, m, lp);
      break;
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
    case EXEC_COMPLETE:
      handle_exec_rev_event(ns, b, m, lp);
      break;
    case SEND_COMP:
      handle_send_comp_rev_event(ns, b, m, lp);
      break;
    case RECV_POST:
      handle_recv_post_rev_event(ns, b, m, lp);
      break;
    case COLL_BCAST:
      perform_bcast_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_REDUCTION:
      perform_reduction_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_A2A:
      perform_a2a_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_A2A_SEND_DONE:
      handle_a2a_send_comp_rev_event(ns, b, m, lp);
      break;
    case COLL_ALLGATHER:
      perform_allgather_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_ALLGATHER_SEND_DONE:
      handle_allgather_send_comp_rev_event(ns, b, m, lp);
      break;
    case COLL_BRUCK:
      perform_bruck_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_BRUCK_SEND_DONE:
      handle_bruck_send_comp_rev_event(ns, b, m, lp);
      break;
    case COLL_A2A_BLOCKED:
      perform_a2a_blocked_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_A2A_BLOCKED_SEND_DONE:
      handle_a2a_blocked_send_comp_rev_event(ns, b, m, lp);
      break;
    case RECV_COLL_POST:
      handle_coll_recv_post_rev_event(ns, b, m, lp);
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

static void proc_commit(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
    if(m->proc_event_type != JOB_END) {
        return;
    }

    int job_num = m->msgId.id;
    PE* pe = ns->old_pes[job_num];
#if DEBUG_PRINT
    printf("PE%d: Committing job %d in commit handler for event %d of task %d/%d\n",
           pe->myNum, pe->jobNum, m->proc_event_type, m->saved_task, pe->tasksCount);
    fflush(stdout);
#endif

    tw_stime jobTime = ns->end_ts ? ns->end_ts - ns->start_ts : 0;
    tw_stime finalTime = tw_now(lp);

    if(m->msgId.pe == 0)
        printf("Job[%d]PE[%d]: FINALIZE in %f seconds.\n", pe->jobNum,
          pe->myNum, ns_to_s(tw_now(lp)-ns->start_ts));

#if TRACER_OTF_TRACES
    PE_printStat(pe);
#endif

    if(pe->pendingMsgs.size() != 0 ||
       pe->pendingRMsgs.size() != 0) {
      printf("%d psize %d pRsize %d\n", pe->myNum,
        pe->pendingMsgs.size(), pe->pendingRMsgs.size());
    }

    if(pe->pendingReqs.size() != 0 ||
      pe->pendingRReqs.size() != 0) {
      printf("%d rsize %d rRsize %d\n", pe->myNum,
        pe->pendingReqs.size(), pe->pendingRReqs.size());
    }

    if(pe->pendingRCollMsgs.size() != 0) {
      printf("%d rcollsize %d \n", pe->myNum,
        pe->pendingRCollMsgs.size());
    }

    int count = 0;
    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      pe->pendingCollMsgs.begin();
    while(it != pe->pendingCollMsgs.end()) {
      count += it->second.size();
      it++;
    }

    if(count != 0) {
      printf("%d collsize %d \n", pe->myNum, count);
    }

    if(jobTime > jobTimes[pe->jobNum]) {
        jobTimes[pe->jobNum] = jobTime;
    }
    if(finalTime > finalizeTimes[pe->jobNum]) {
        finalizeTimes[pe->jobNum] = finalTime;
    }

    for(int i = 0; i < jobs[pe->jobNum].numIters; i++) {
        delete [] pe->taskStatus[i];
        delete [] pe->taskExecuted[i];
        delete [] pe->msgStatus[i];
    }
    deletePE(pe);
    ns->old_pes.erase(job_num);
}

static void proc_finalize(
    proc_state * ns,
    tw_lp * lp)
{
    if(dump_topo_only) return;

    assert(ns->my_pe == NULL);

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

static void handle_job_start_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{
    tw_event *e;
    tw_stime kickoff_time;
    //Each server read it's trace
    int my_job = m->msgId.id;
    int my_pe_num = m->msgId.pe;

    if(my_pe_num == -1) {
        return;
    }

    ns->my_pe = newPE();
    ns->my_pe->jobNum = my_job;
    ns->my_pe->myNum = my_pe_num;

    tw_stime startTime=0;

#if TRACER_BIGSIM_TRACES
    TraceReader* t = newTraceReader(jobs[my_job].traceDir);
    int tot=0, totn=0, emPes=0, nwth=0;
    TraceReader_loadTraceSummary(t);
    TraceReader_setOffsets(t, jobs[my_job].offsets);

    TraceReader_readTrace(t, &tot, &totn, &emPes, &nwth, ns->my_pe, &startTime);
    TraceReader_setOffsets(t, NULL);
    deleteTraceReader(t);
#else
    TraceReader_readOTF2Trace(ns->my_pe, &startTime);
#endif

    /* skew each kickoff event slightly to help avoid event ties later on */
    kickoff_time = startTime + g_tw_lookahead + tw_rand_unif(lp->rng);
    ns->end_ts = 0;
    ns->my_pe->sendSeq = new int64_t[jobs[my_job].numRanks];
    ns->my_pe->recvSeq = new int64_t[jobs[my_job].numRanks];
    for(int i = 0; i < jobs[my_job].numRanks; i++) {
      ns->my_pe->sendSeq[i] = ns->my_pe->recvSeq[i] = 0;
    }

    e = codes_event_new(lp->gid, kickoff_time, lp);
    m =  (proc_msg*)tw_event_data(e);
    m->proc_event_type = KICKOFF;
    tw_event_send(e);
}

static void handle_job_start_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{
    tw_rand_reverse_unif(lp->rng);
    for(int i = 0; i < jobs[ns->my_pe->jobNum].numIters; i++) {
        delete [] ns->my_pe->taskStatus[i];
        delete [] ns->my_pe->taskExecuted[i];
        delete [] ns->my_pe->msgStatus[i];
    }
    deletePE(ns->my_pe);
    ns->my_pe = NULL;
}

static void handle_job_end_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{ }

static void handle_job_end_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{ }

/* handle initial event */
static void handle_kickoff_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp* lp)
{
    ns->start_ts = tw_now(lp);

    int my_pe_num = ns->my_pe->myNum;
    int my_job = ns->my_pe->jobNum;
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
    ns->my_pe->currentTask = -1;
    exec_task(ns, pair, lp, m, b);
}

/* reverse handler for kickoff */
static void handle_kickoff_rev_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp)
{
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: handle_kickoff_rev_event. TIME now:%f.\n", ns->my_pe->myNum, now);
#endif
    PE_set_busy(ns->my_pe, false);
    TaskPair pair;
    pair.iter = 0; pair.taskid = PE_getFirstTask(ns->my_pe);
    exec_task_rev(ns, pair, lp, m, b);
    return;
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
    if(ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
    printf("%d RECD MSG: %d %d %d %lld\n", 
        ns->my_pe->myNum, m->msgId.pe, m->msgId.id, m->msgId.comm,
        m->msgId.seq);
    }
#endif
    MsgKey key(m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq);
    KeyType::iterator it = ns->my_pe->pendingMsgs.find(key);
    assert((it == ns->my_pe->pendingMsgs.end()) || (it->second.size() != 0));
    if(it == ns->my_pe->pendingMsgs.end() || it->second.front() == -1) {
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
      printf("[%d:%d] RECD MSG FOUND TASK: %d %d %d %d - %d\n", ns->my_pe->jobNum,
          ns->my_pe->myNum, m->msgId.pe, m->msgId.id, m->msgId.comm,
          ns->my_pe->recvSeq[m->msgId.pe], task_id);
#endif
    }
#endif
    int iter = m->iteration;
#if DEBUG_PRINT
    tw_stime now = tw_now(lp);
    printf("PE%d: handle_recv_event - received from %d id: %d for task: "
        "%d. TIME now:%f.\n", ns->my_pe->myNum, m->msgId.pe, m->msgId.id,
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
    if(1 || ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
    printf("%d Recv  busy %d %d\n", ns->my_pe->myNum, isBusy, task_id);
    }
#endif
    m->incremented_flag = isBusy;
    m->executed.taskid = -1;
    if(task_id>=0){
        //The matching task should not be already done
        if(PE_get_taskDone(ns->my_pe,iter,task_id)){ //TODO: check this
          printf("[%d:%d] WARNING: MSG RECV TASK IS DONE: %d\n", ns->my_pe->jobNum,
              ns->my_pe->myNum, task_id);
            assert(0);
        }
        PE_invertMsgPe(ns->my_pe, iter, task_id);
        if(m->msgId.size <= eager_limit && ns->my_pe->currIter == 0) {
          b->c1 = 1;
          if(m->msgId.pe != ns->my_pe->myNum) {
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
      ns->my_pe->myNum, m->msgId.pe, m->msgId.id);
    assert(0);
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
#if TRACER_BIGSIM_TRACES
    if(m->executed.taskid == -2) {
        return;
    }
#endif
    bool wasBusy = m->incremented_flag;
    int iter = m->iteration;
    PE_set_busy(ns->my_pe, wasBusy);
#if DEBUG_PRINT
    if(1 || ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
    printf("%d Recv rev busy %d %d\n", ns->my_pe->myNum, wasBusy, m->executed.taskid);
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
    " wasBusy: %d. TIME now:%f\n", ns->my_pe->myNum, m->msgId.id, task_id,
    wasBusy, now);
#endif
    if(b->c1) {
      if(m->msgId.pe != ns->my_pe->myNum) {
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

static void handle_exec_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    //For exec complete event msgId contains the task_id for convenience
    int task_id = m->msgId.id;
    int iter = m->iteration;
    if(task_id != ns->my_pe->currentTask || 
       PE_get_taskDone(ns->my_pe, iter, task_id)) {
      b->c2 = 1;
      return;
    }
    PE_set_busy(ns->my_pe, false);
#if DEBUG_PRINT
    if(1 || ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
      printf("%d Set busy false %d\n", ns->my_pe->myNum, task_id);
    }
#endif
    //Mark the task as done
    PE_set_taskDone(ns->my_pe, iter, task_id, true);

    int counter = 0;
#if TRACER_BIGSIM_TRACES
    int fwd_dep_size = PE_getTaskFwdDepSize(ns->my_pe, task_id);
    int* fwd_deps = PE_getTaskFwdDep(ns->my_pe, task_id);

    if(PE_isLoopEvent(ns->my_pe, task_id) && (PE_get_iter(ns->my_pe) != (jobs[ns->my_pe->jobNum].numIters - 1))) {
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
       (PE_get_iter(ns->my_pe) != (jobs[ns->my_pe->jobNum].numIters - 1))) {
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
    m->fwd_dep_count = counter;
    m->executed = buffd_task;
    if(buffd_task.taskid != -1){
        exec_task(ns, buffd_task, lp, m, b); //we don't care about the return value?
    }
    if(ns->my_pe->currentTask == (ns->my_pe->tasksCount - 1) &&
       ns->my_pe->currIter == (jobs[ns->my_pe->jobNum].numIters - 1) &&
       PE_get_taskDone(ns->my_pe, ns->my_pe->currIter, ns->my_pe->currentTask)) {

        b->c3 = 1;

        // Save the current task and job infromation for reversing
        m->saved_task = m->msgId.id;
        m->msgId.id = ns->my_pe->jobNum;

        // Inform scheduler that the execution of this job has ended
        tw_event *e_sched = codes_event_new(sched_lpid(), g_tw_lookahead + g_tw_lookahead, lp);
        proc_msg *m_sched =  (proc_msg*)tw_event_data(e_sched);
        m_sched->proc_event_type = JOB_END;
        m_sched->src = lp->gid;
        m_sched->msgId.id = ns->my_pe->jobNum;
        m_sched->msgId.pe = ns->my_pe->myNum;
        tw_event_send(e_sched);

        // Send an event to self for cleaning up on commit
        tw_event *e_self = codes_event_new(lp->gid, g_tw_lookahead + g_tw_lookahead, lp);
        proc_msg *m_self =  (proc_msg*)tw_event_data(e_self);
        m_self->proc_event_type = JOB_END;
        m_self->msgId.id = ns->my_pe->jobNum;
        m_self->msgId.pe = ns->my_pe->myNum;
        tw_event_send(e_self);

        // Store this PE for cleanup during commit
        ns->old_pes[ns->my_pe->jobNum] = ns->my_pe;
        ns->my_pe = NULL;
    }
}

static void handle_exec_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    if(b->c2) return;

    if(b->c3) {
        ns->my_pe = ns->old_pes[m->msgId.id];
        ns->old_pes.erase(m->msgId.id);
        m->msgId.id = m->saved_task;
    }
    int task_id = m->msgId.id;

    //Reverse the state: set the PE as busy, task is not completed yet
    PE_set_busy(ns->my_pe, true);
#if DEBUG_PRINT
    if(1 || ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
      printf("%d Rev Set busy true %d\n", ns->my_pe->myNum, task_id);
    }
#endif

#if DEBUG_PRINT
    printf("PE%d: In reverse handler of exec task with task_id: %d\n",
    ns->my_pe->myNum, task_id);
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
#if TRACER_OTF_TRACES
        if(m->fwd_dep_count != 0) assert(0);
#endif
        PE_resizeBuffer(ns->my_pe, m->fwd_dep_count);
        if(m->executed.taskid != -1) {
            PE_addToFrontBuffer(ns->my_pe, &m->executed);
        }
    }
    if(m->executed.taskid != -1) {
      exec_task_rev(ns, m->executed, lp, m, b);
    }
}

#if TRACER_BIGSIM_TRACES

static void handle_send_comp_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_send_comp_rev_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_recv_post_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_recv_post_rev_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}

#elif TRACER_OTF_TRACES

static void handle_send_comp_event(
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

static void handle_send_comp_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    if(b->c1) return;
    if(b->c2) ns->my_pe->pendingReqs[m->msgId.id] = -1;
    if(b->c3) ns->my_pe->pendingReqs[m->msgId.id] = m->executed.taskid;
}

static void handle_recv_post_event(
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
  printf("%d: Recv post recevied %d %d %d %d, found %d %d\n", ns->my_pe->myNum,
      m->msgId.pe, m->msgId.id, m->msgId.comm, m->msgId.seq, b->c2, m->executed.taskid);
#endif
}

static void handle_recv_post_rev_event(
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

static void delegate_send_msg(proc_state *ns,
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
      pe_to_lpid(taskEntry->node, ns->my_pe->jobNum), nic_delay+rdma_delay+delay,
      RECV_MSG, &m_local, lp);
}

#endif 

//executes the task with the specified id
static tw_stime exec_task(
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
        printf("[%d:%d] WARNING: TASK HAS TASK DEP: %d\n", ns->my_pe->jobNum,
          ns->my_pe->myNum, task_id.taskid);
        assert(0);
    }
    //Check if the task is already done -- safety check?
    if(PE_get_taskDone(ns->my_pe, task_id.iter, task_id.taskid)){
        printf("[%d:%d] WARNING: TASK IS ALREADY DONE: %d\n", ns->my_pe->jobNum,
          ns->my_pe->myNum, task_id.taskid);
        assert(0);
    }
#if TRACER_BIGSIM_TRACES
    //Check the task does not have any message dependency
    if(!PE_noMsgDep(ns->my_pe, task_id.iter, task_id.taskid)){
        printf("[%d:%d] WARNING: TASK HAS MESSAGE DEP: %d\n", ns->my_pe->jobNum,
          ns->my_pe->myNum, task_id.taskid);
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
      if(ns->my_pe->myNum ==  1222 || ns->my_pe->myNum == 1217) {
        printf("%d Post Irecv: %d - %d %d %d %lld \n", ns->my_pe->myNum,
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
        if(1 || ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
        printf("%d PUSH recv: %d - %d %d %d %lld %lld %d\n", ns->my_pe->myNum,
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
        if(ns->my_pe->myNum ==  1222 || ns->my_pe->myNum == 1217) {
        printf("%d Recv matched: %d - %d %d %d %lld, %lld %d\n", ns->my_pe->myNum,
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
    if(t->myEntry.node != ns->my_pe->myNum &&
       t->myEntry.msgId.size > eager_limit &&
       (t->event_id == TRACER_RECV_POST_EVT || needPost)) {
      m->model_net_calls++;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, seq,  
        pe_to_lpid(t->myEntry.node, ns->my_pe->jobNum), nic_delay, RECV_POST, lp);
#if DEBUG_PRINT
      printf("%d: Recv post %d %d %d %d\n", ns->my_pe->myNum,
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
    if(1 || ns->my_pe->myNum == 1024 || ns->my_pe->myNum == 11788) {
      printf("%d Set busy true %d\n", ns->my_pe->myNum, task_id.taskid);
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
    int myPE = ns->my_pe->myNum;
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
              if(destPE == ns->my_pe->myNum) {
                exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
              }else{
                m->model_net_calls++;
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    task_id.iter, &taskEntry->msgId, 0 /*not used */,
                    pe_to_lpid(destPE, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG,
                    lp);
              }
            }
          } else if(node != -100-myNode && node <= -100) {
            int destPE = myNode*nWth - 1;
            for(int i=0; i<nWth; i++)
            {
              destPE++;
              delay += copyTime;
              if(destPE == ns->my_pe->myNum){
                exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
              }else{
                m->model_net_calls++;
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    task_id.iter, &taskEntry->msgId,  0 /*not used */,
                    pe_to_lpid(destPE, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG,
                    lp);
              }
            }
          } else if(thread >= 0) {
            int destPE = myNode*nWth + thread;
            delay += copyTime;
            if(destPE == ns->my_pe->myNum){
              exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
            }else{
              m->model_net_calls++;
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  task_id.iter, &taskEntry->msgId,  0 /*not used */,
                  pe_to_lpid(destPE, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG,
                  lp);
            }
          } else if(thread==-1) { // broadcast to all work cores
            int destPE = myNode*nWth - 1;
            for(int i=0; i<nWth; i++)
            {
              destPE++;
              delay += copyTime;
              if(destPE == ns->my_pe->myNum){
                exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 0, sendOffset+delay, 1, lp);
              }else{
                m->model_net_calls++;
                send_msg(ns, MsgEntry_getSize(taskEntry), 
                    task_id.iter, &taskEntry->msgId,  0 /*not used */,
                    pe_to_lpid(destPE, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG,
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
                pe_to_lpid(node, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG, lp);
          }
          else if(node == -1){
            bcast_msg(ns, MsgEntry_getSize(taskEntry),
                task_id.iter, &taskEntry->msgId,
                sendOffset+delay, copyTime, lp, m);
          }
          else if(node <= -100 && thread == -1){
            for(int j=0; j<jobs[ns->my_pe->jobNum].numRanks; j++){
              if(j == -node-100 || j == myNode) continue;
              delay += copyTime;
              m->model_net_calls++;
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  task_id.iter, &taskEntry->msgId,  0 /*not used */,
                  pe_to_lpid(j, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG, lp);
            }

          }
          else if(node <= -100){
            for(int j=0; j<jobs[ns->my_pe->jobNum].numRanks; j++){
              if(j == myNode) continue;
              delay += copyTime;
              m->model_net_calls++;
              send_msg(ns, MsgEntry_getSize(taskEntry),
                  task_id.iter, &taskEntry->msgId,  0 /*not used */,
                  pe_to_lpid(j, ns->my_pe->jobNum), sendOffset+delay, RECV_MSG, lp);
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
      if(MsgEntry_getSize(taskEntry) > eager_limit && node != ns->my_pe->myNum) {
        copyTime = soft_latency;
        isCopying = false;
      }
      sendOffset = soft_delay_mpi;

      if(node == ns->my_pe->myNum) {
        exec_comp(ns, task_id.iter, MsgEntry_getID(taskEntry), 
          taskEntry->msgId.comm, sendOffset+copyTime+delay, 1, lp);
        sendFinishTime = sendOffset + copyTime;
      } else {
#if DEBUG_PRINT
        if(ns->my_pe->myNum ==  1222 || ns->my_pe->myNum == 1217) {
          printf("%d SEND to: %d  %d %d %lld\n", ns->my_pe->myNum, node,
            taskEntry->msgId.id, taskEntry->msgId.comm, ns->my_pe->sendSeq[node]);
        }
#endif

        if(isCopying) {
          m->model_net_calls++;
          send_msg(ns, MsgEntry_getSize(taskEntry),
              task_id.iter, &taskEntry->msgId, ns->my_pe->sendSeq[node]++,
              pe_to_lpid(node, ns->my_pe->jobNum), sendOffset+copyTime+nic_delay+delay,
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
          printf("%d: Send %d %d %d %d, nonblock %d/%d, wait %d, do %d, task %d\n", ns->my_pe->myNum,
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
      tw_output(lp, str, ns->my_pe->jobNum, ns->my_pe->myNum,
          jobs[ns->my_pe->jobNum].allData->strings[jobs[ns->my_pe->jobNum].allData->regions[t->event_id].name].c_str(),
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
    
    if(ns->my_pe->myNum == 0 && (ns->my_pe->currentTask % print_frequency == 0)) {
      char str[1000];
      strcpy(str, "[ %d %d : time at task %d/%d %f ]\n");
      tw_output(lp, str, ns->my_pe->jobNum, ns->my_pe->myNum,
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

static void exec_task_rev(
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
static int send_msg(
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

        /*   model_net_event params:
             int net_id, char* category, tw_lpid final_dest_lp,
             uint64_t message_size, tw_stime offset, int remote_event_size,
             const void* remote_event, int self_event_size,
             const void* self_event, tw_lp *sender */

        model_net_event(net_id, "test", dest_id, size, sendOffset,
          sizeof(proc_msg), &m_remote, 0, NULL, lp);
        ns->msg_sent_count++;
    
    return 0;
}

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

        model_net_event(net_id, "p2p", dest_id, size, sendOffset,
          sizeof(proc_msg), (const void*)&m_remote, sizeof(proc_msg), m_local, 
          lp);
        ns->msg_sent_count++;
}

#if TRACER_BIGSIM_TRACES

static void handle_coll_recv_post_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {} 
static void handle_coll_recv_post_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {} 
static void perform_collective( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b) {}
static void perform_collective_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b) {} 
static void perform_bcast( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
static void perform_bcast_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
static void perform_reduction( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
static void perform_reduction_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void perform_a2a( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void perform_a2a_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
static void handle_a2a_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_a2a_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void perform_allreduce( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
static void perform_allreduce_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void perform_allgather( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void perform_allgather_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void handle_allgather_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_allgather_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void perform_bruck( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void perform_bruck_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {}
static void handle_bruck_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_bruck_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void perform_a2a_blocked( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void perform_a2a_blocked_rev( proc_state * ns, int task_id, tw_lp * lp, proc_msg *m, tw_bf * b, int isEvent) {} 
static void handle_a2a_blocked_send_comp_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_a2a_blocked_send_comp_rev_event( proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_coll_complete_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static void handle_coll_complete_rev_event(proc_state * ns, tw_bf * b, proc_msg * m, tw_lp * lp) {}
static int send_coll_comp(proc_state *, tw_stime, int, tw_lp *, int, proc_msg*) {}
static int send_coll_comp_rev(proc_state *, tw_stime, int, tw_lp *, int, proc_msg *) {}

#elif TRACER_OTF_TRACES

static void enqueue_coll_msg(
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
    //  printf("%d enqueue %d %d %d -- %d %d -- %d %d %d %d \n", ns->my_pe->myNum, dest,
    //  msgId->comm, seq, ns->my_pe->pendingRCollMsgs.size(), it->second.size(),
    //  it2->first.rank, it2->first.comm, it2->first.seq, it->second.size());
    //  fflush(stdout);
    //  assert(0);
    //}
    if(!isEager && (it == ns->my_pe->pendingRCollMsgs.end() || 
        it->second.front() != -1)) {
      b->c16 = 1;
      ns->my_pe->pendingRCollMsgs[key].push_back(index);
      //printf("%d Added %d %d %d\n",  ns->my_pe->myNum, dest, msgId->comm, seq);
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

      model_net_event(net_id, "coll", pe_to_lpid(dest, ns->my_pe->jobNum), size,
          sendOffset + copyTime*(isEager?1:0), sizeof(proc_msg), 
          (const void*)&m_remote, sizeof(proc_msg), &m_local, lp);
      m->model_net_calls++;
      ns->msg_sent_count++;
      if(!isEager) {
        b->c17 = 1;
        it->second.pop_front();
        if(it->second.size() == 0) {
          ns->my_pe->pendingRCollMsgs.erase(it);
        }
      }
    }
}


static void enqueue_coll_msg_rev(
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
    //printf("%d Removing %d %d %d\n", ns->my_pe->myNum, dest, msgId->comm, seq);
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

static void handle_coll_recv_post_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  //printf("%d recv post %d %d %d\n", ns->my_pe->myNum, m->msgId.pe, m->msgId.comm, m->msgId.seq);
  //fflush(stdout);
  CollMsgKey key(m->msgId.pe, m->msgId.comm, m->msgId.seq);
  CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.find(key);
  assert(it == ns->my_pe->pendingRCollMsgs.end() || it->second.size() != 0);
  if(it == ns->my_pe->pendingRCollMsgs.end() || it->second.front() == -1) {
    b->c1 = 1;
    ns->my_pe->pendingRCollMsgs[key].push_back(-1);
    //printf("%d Added recv post %d %d %d\n",  ns->my_pe->myNum, m->msgId.pe, m->msgId.comm, m->msgId.seq);
  } else {
    b->c2 = 1;
    assert(ns->my_pe->currentCollTask >= 0);
    Task *t = &ns->my_pe->myTasks[ns->my_pe->currentCollTask];
    m->model_net_calls = 1;
    assert(ns->my_pe->currentCollSeq == m->msgId.seq);
    assert(ns->my_pe->currentCollComm == m->msgId.comm);
    int index = it->second.front();
    m->coll_info = index;
    //printf("%d Sending coll %d %d\n", ns->my_pe->myNum, index, m->msgId.pe);
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
        t->myEntry.msgId.size * ns->my_pe->currentCollSize <= TRACER_ALLGATHER_ALG_CUTOFF)) {
      size = m->msgId.size;
      m_remote.msgId.size = size;
    }
    model_net_event(net_id, "coll", pe_to_lpid(m->msgId.pe, ns->my_pe->jobNum),
        size, nic_delay, sizeof(proc_msg), 
        (const void*)&m_remote, sizeof(proc_msg), &m_local, lp);
    it->second.pop_front();
    if(it->second.size() == 0) {
      ns->my_pe->pendingRCollMsgs.erase(it);
    }
  }
}

static void handle_coll_recv_post_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
  CollMsgKey key(m->msgId.pe, m->msgId.comm, m->msgId.seq);
  if(b->c1) {
    //printf("%d Removing recv post %d %d %d\n", ns->my_pe->myNum, m->msgId.pe, m->msgId.comm, m->msgId.seq);
    //fflush(stdout);
    assert(ns->my_pe->pendingRCollMsgs.find(key) != ns->my_pe->pendingRCollMsgs.end());
    ns->my_pe->pendingRCollMsgs[key].pop_back();
    if(ns->my_pe->pendingRCollMsgs[key].size() == 0) {
      ns->my_pe->pendingRCollMsgs.erase(key);
    }
  }
  if(b->c2) {
    ns->my_pe->pendingRCollMsgs[key].push_front(m->coll_info);
    for(int i = 0; i < m->model_net_calls; i++) {
      model_net_event_rc(net_id, lp, 0);
    }
  }
}

static void perform_collective(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b) {
  Task *t = &ns->my_pe->myTasks[taskid];
  assert(t->event_id == TRACER_COLL_EVT);
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[t->myEntry.msgId.comm]];
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
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[t->myEntry.msgId.comm]];
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

  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
  std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe->myNum);
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
      &t->myEntry.msgId,  ns->my_pe->currentCollSeq, pe_to_lpid(dest, ns->my_pe->jobNum),
      delay, COLL_BCAST, lp);
    delay += copyTime;
    m->model_net_calls++;
  }
  send_coll_comp(ns, delay, TRACER_COLLECTIVE_BCAST, lp, isEvent, m);
}

static void perform_bcast_rev(
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
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }
  send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_BCAST, lp, isEvent, m);
}

static void perform_reduction(
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

  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
  std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe->myNum);
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
        &t->myEntry.msgId,  ns->my_pe->currentCollSeq, pe_to_lpid(dest, ns->my_pe->jobNum),
        delay+ nic_delay*((t->myEntry.msgId.size>16)?1:0), COLL_REDUCTION, lp);
    m->model_net_calls++;
  }
  delay += copyTime;
  send_coll_comp(ns, delay, TRACER_COLLECTIVE_REDUCE, lp, isEvent, m);
}

static void perform_reduction_rev(
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
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }
  send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_REDUCE, lp, isEvent, m);
}

static void perform_a2a(
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
    Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe->myNum);
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
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];

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
      t->myEntry.msgId.pe = ns->my_pe->myNum;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, 
        ns->my_pe->currentCollSeq, pe_to_lpid(g.members[src], ns->my_pe->jobNum),
        delay, RECV_COLL_POST, lp);
      t->myEntry.msgId.pe = ns->my_pe->currentCollRank;
    }
    delay += copyTime;
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALLTOALL_LARGE, lp, isEvent, m);
  }
}

static void perform_a2a_rev(
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
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }

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

static void handle_a2a_send_comp_event(
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
    tw_event *e = codes_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = ns->my_pe->currentCollRank;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_A2A;
    tw_event_send(e);
  }
}

static void handle_a2a_send_comp_rev_event(
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

static void perform_allreduce(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  perform_reduction(ns, taskid, lp, m, b, isEvent);
}

static void perform_allreduce_rev(
            proc_state * ns,
            int taskid,
            tw_lp * lp,
            proc_msg *m,
            tw_bf * b,
            int isEvent) {
  perform_reduction_rev(ns, taskid, lp, m, b, isEvent);
}

static void perform_allgather(
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
    Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe->myNum);
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
    //printf("%d New coll %d %d %d\n", ns->my_pe->myNum, index, ns->my_pe->currentCollComm,
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
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];

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
    //if(ns->my_pe->myNum == 23) {
    //  CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.begin();
    //  printf("%d enqueue -- %d %d -- %d %d %d \n", ns->my_pe->myNum,
    //  ns->my_pe->pendingRCollMsgs.size(), it->second.size(),
    //  it->first.rank, it->first.comm, it->first.seq);
    //  fflush(stdout);
    //}
    enqueue_coll_msg(TRACER_ALLGATHER, ns, t->myEntry.msgId.size, 
        ns->my_pe->currIter, &t->myEntry.msgId,  ns->my_pe->currentCollSeq, 
        dest, delay + nic_delay + soft_delay_mpi, copyTime, lp, m, b);
    if(t->myEntry.msgId.size > eager_limit) {
      m->model_net_calls++;
      int saved_pe = t->myEntry.msgId.pe;
      t->myEntry.msgId.pe = ns->my_pe->myNum;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, 
        ns->my_pe->currentCollSeq, pe_to_lpid(g.members[src], ns->my_pe->jobNum),
        delay, RECV_COLL_POST, lp);
      t->myEntry.msgId.pe = saved_pe;
      //printf("%d Send MSG to %d %d %lld\n", ns->my_pe->myNum, src, g.members[src], ns->my_pe->currentCollSeq);
    }
    //if(ns->my_pe->myNum == 23) {
    //  CollKeyType::iterator it = ns->my_pe->pendingRCollMsgs.begin();
    //  printf("%d after enqueue -- %d %d -- %d %d %d \n", ns->my_pe->myNum,
    //  ns->my_pe->pendingRCollMsgs.size(), it->second.size(),
    //  it->first.rank, it->first.comm, it->first.seq);
    //  fflush(stdout);
    //}
    delay += copyTime;
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALLGATHER_LARGE, lp, isEvent, m);
  }
}

static void perform_allgather_rev(
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
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }

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

static void handle_allgather_send_comp_event(
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
  //printf("%d Coll send complete %d %d %d\n", ns->my_pe->myNum, ns->my_pe->currentCollComm,
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
    tw_event *e = codes_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = 0;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_ALLGATHER;
    tw_event_send(e);
  }
}

static void handle_allgather_send_comp_rev_event(
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

static void perform_bruck(
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
    Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe->myNum);
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
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];

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
        dest, delay + nic_delay + soft_delay_mpi, copyTime, lp, m, b);
    if(ns->my_pe->currentCollMsgSize > eager_limit) {
      m->model_net_calls++;
      t->myEntry.msgId.pe = ns->my_pe->myNum;
      send_msg(ns, 16, ns->my_pe->currIter, &t->myEntry.msgId, 
        ns->my_pe->currentCollSeq, pe_to_lpid(g.members[src], ns->my_pe->jobNum),
        delay, RECV_COLL_POST, lp, true,  ns->my_pe->currentCollMsgSize);
      t->myEntry.msgId.pe = ns->my_pe->currentCollRank;
    }
    delay += copyTime;
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALL_BRUCK, lp, isEvent, m);
  }
}

static void perform_bruck_rev(
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
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }

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

static void handle_bruck_send_comp_event(
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
    tw_event *e = codes_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = ns->my_pe->currentCollRank;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_BRUCK;
    tw_event_send(e);
  }
}

static void handle_bruck_send_comp_rev_event(
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

static void perform_a2a_blocked(
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
    Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
    std::map<int, int>::iterator it = g.rmembers.find(ns->my_pe->myNum);
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
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];

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
      delay += copyTime;
    }
  } else {
    b->c15 = 1;
    if(ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm][ns->my_pe->currentCollSeq].size() == 0) {
      ns->my_pe->pendingCollMsgs[ns->my_pe->currentCollComm].erase(ns->my_pe->currentCollSeq);
    }
    send_coll_comp(ns, delay, TRACER_COLLECTIVE_ALLTOALL_BLOCKED, lp, isEvent, m);
  }
}

static void perform_a2a_blocked_rev(
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
  for(int i = 0; i < m->model_net_calls; i++) {
    //TODO use the right size to rc
    model_net_event_rc(net_id, lp, 0);
  }

  if(b->c15) {
    send_coll_comp_rev(ns, 0, TRACER_COLLECTIVE_ALLTOALL_BLOCKED, lp, isEvent, m);
  }
}

static void handle_a2a_blocked_send_comp_event(
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
    tw_event *e = codes_event_new(lp->gid, soft_delay_mpi + codes_local_latency(lp), lp);
    proc_msg *m_new = (proc_msg*)tw_event_data(e);
    m_new->msgId.pe = ns->my_pe->currentCollRank;
    m_new->msgId.comm = ns->my_pe->currentCollComm;
    m_new->msgId.seq = ns->my_pe->currentCollSeq;
    m_new->proc_event_type = COLL_A2A_BLOCKED;
    tw_event_send(e);
  }
}

static void handle_a2a_blocked_send_comp_rev_event(
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


static void handle_coll_complete_event(
    proc_state * ns,
    tw_bf * b,
    proc_msg * m,
    tw_lp * lp) {
  if(ns->my_pe->currentCollSeq == -1) {
    b->c3 = 1;
    return;
  }
  Task *t = &ns->my_pe->myTasks[m->executed.taskid];
  //printf("%d coll complete %d %d\n", ns->my_pe->myNum, ns->my_pe->currentCollComm,
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
     (t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLTOALLV)
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
      tw_output(lp, str, ns->my_pe->jobNum, ns->my_pe->myNum,
          jobs[ns->my_pe->jobNum].allData->strings[jobs[ns->my_pe->jobNum].allData->regions[t->event_id].name].c_str(),
          tw_now(lp)/((double)TIME_MULT));
    }

    if(ns->my_pe->myNum == 0 && (ns->my_pe->currentTask % print_frequency == 0)) {
      char str[1000];
      strcpy(str, "[ %d %d : time at task %d %f ]\n");
      tw_output(lp, str, ns->my_pe->jobNum, ns->my_pe->myNum,
          ns->my_pe->currentTask, tw_now(lp)/((double)TIME_MULT));
    }
  } else if(t->myEntry.msgId.coll_type == OTF2_COLLECTIVE_OP_ALLREDUCE &&
      m->msgId.coll_type == TRACER_COLLECTIVE_REDUCE) {
    b->c2 = 1;
    perform_bcast(ns, m->executed.taskid, lp, m, b, 0);
  }
}

static void handle_coll_complete_rev_event(
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
  Group &g = jobs[ns->my_pe->jobNum].allData->groups[jobs[ns->my_pe->jobNum].allData->communicators[ns->my_pe->currentCollComm]];
  if(m->msgId.coll_type == TRACER_COLLECTIVE_ALLTOALL_LARGE || 
     m->msgId.coll_type == TRACER_COLLECTIVE_ALLGATHER_LARGE || 
     m->msgId.coll_type == TRACER_COLLECTIVE_ALL_BRUCK ||
     m->msgId.coll_type == TRACER_COLLECTIVE_ALLTOALL_BLOCKED) {
    ns->my_pe->currentCollSize = g.members.size();
    ns->my_pe->currentCollPartner = m->fwd_dep_count;
    if(m->msgId.coll_type == TRACER_COLLECTIVE_ALL_BRUCK) {
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

static int send_coll_comp(
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
    e = codes_event_new(lp->gid, sendOffset + soft_delay_mpi, lp);
    msg = (proc_msg*)tw_event_data(e);
    msg->msgId.coll_type = collType;
    msg->proc_event_type = COLL_COMPLETE;
    msg->executed.taskid = taskid;
    tw_event_send(e);
    return 0;
}

static int send_coll_comp_rev(
    proc_state * ns,
    tw_stime sendOffset,
    int collType,
    tw_lp * lp,
    int isEvent,
    proc_msg *m)
{
  if(isEvent) ns->my_pe->currentCollTask = m->executed.taskid;
}
#endif

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
    int thisTreePe = (ns->my_pe->myNum - msgId->pe + jobs[ns->my_pe->jobNum].numRanks) %
                      jobs[ns->my_pe->jobNum].numRanks;

    for(int i = 0; i < BCAST_DEGREE; i++) {
      int next_child = BCAST_DEGREE * thisTreePe + i + 1;
      if(next_child >= jobs[ns->my_pe->jobNum].numRanks) {
        break;
      }
      myChildren[i] = (msgId->pe + next_child) % jobs[ns->my_pe->jobNum].numRanks;
      numValidChildren++;
    }
    
    tw_stime delay = copyTime;

    for(int i = 0; i < numValidChildren; i++) {
      send_msg(ns, size, iter, msgId,  0 /*not used */, 
        pe_to_lpid(myChildren[i], ns->my_pe->jobNum), sendOffset + delay, BCAST, lp);
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
    m->msgId.pe = ns->my_pe->myNum;
    m->msgId.id = task_id;
#if TRACER_OTF_TRACES
    m->msgId.comm = comm_id;
    if(recv) {
      m->msgId.seq = ns->my_pe->sendSeq[ns->my_pe->myNum]++;
    }
#endif
    m->iteration = iter;
    if(recv) {
        m->proc_event_type = RECV_MSG;
#if DEBUG_PRINT
        if(ns->my_pe->myNum ==  1222 || ns->my_pe->myNum == 1217) {
          printf("%d Sending to %d %d %d %lld\n", ns->my_pe->myNum, ns->my_pe->myNum,
            m->msgId.id, m->msgId.comm, m->msgId.seq);
        }
#endif
    }
    else 
        m->proc_event_type = EXEC_COMPLETE;
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

//Assuming that the scheduler is the last to be registered
static inline int sched_lpid(){
    return total_lps - 1;
}

bool isPEonThisRank(int jobID, int i) {
  int lpid = pe_to_lpid(i, jobID);
  int pe = codes_mapping(lpid);
  return pe == rank;
}
