/** \file tracer-driver.C
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

char tracer_input[256]; /* filename for tracer input file */

CoreInf *global_rank;   /* core to job ID and process ID */
JobInf *jobs;
int default_mapping;
int total_ranks;
tw_stime *jobTimes;
tw_stime *finalizeTimes;
int num_jobs = 0;
tw_stime soft_delay_mpi = 100;
tw_stime nic_delay = 400;
tw_stime rdma_delay = 1000;

int net_id = 0;
int num_servers = 0;

unsigned int print_frequency = 5000;
int* size_replace_by;
int* size_replace_limit;
double time_replace_by = 0;
double time_replace_limit = -1;
double copy_per_byte = 0.0;
double eager_limit = 8192;
int dump_topo_only = 0;
int rank;

#define DEBUG_PRINT 0

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

static char lp_io_dir[256] = {'\0'};
const tw_optdef app_opt [] =
{
    TWOPT_GROUP("Model net test case" ),
    TWOPT_CHAR("lp-io-dir", lp_io_dir, "Where to place io output (unspecified -> tracer-out"),
    TWOPT_UINT("timer-frequency", print_frequency, "Frequency for printing timers, #tasks (unspecified -> 5000"),
    TWOPT_END()
};

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
    
    codes_mapping_setup();
    
    num_servers = codes_mapping_get_lp_count("MODELNET_GRP", 0, "server", 
            NULL, 1);

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

    if (total_ranks > num_servers) {
      if (!rank)
        printf("Job requires %d servers, but the topology only contains %d. Aborting\n", total_ranks, num_servers);
      MPI_Abort(MPI_COMM_WORLD, 1);
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

    e = tw_event_new(lp->gid, kickoff_time, lp);
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
    case COLL_SCATTER_SMALL:
      perform_scatter_small(ns, -1, lp, m, b, 1);
      break;
    case COLL_SCATTER:
      perform_scatter(ns, -1, lp, m, b, 1);
      break;
    case COLL_SCATTER_SEND_DONE:
      handle_scatter_send_comp_event(ns, b, m, lp);
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
    case COLL_SCATTER_SMALL:
      perform_scatter_small_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_SCATTER:
      perform_scatter_rev(ns, -1, lp, m, b, 1);
      break;
    case COLL_SCATTER_SEND_DONE:
      handle_scatter_send_comp_rev_event(ns, b, m, lp);
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

#if TRACER_OTF_TRACES
    PE_printStat(ns->my_pe);
#endif

    if(ns->my_pe->pendingMsgs.size() != 0 ||
       ns->my_pe->pendingRMsgs.size() != 0) {
      printf("%d psize %d pRsize %d\n", ns->my_pe_num, 
        ns->my_pe->pendingMsgs.size(), ns->my_pe->pendingRMsgs.size());
    }

    if(ns->my_pe->pendingReqs.size() != 0 ||
      ns->my_pe->pendingRReqs.size() != 0) {
      printf("%d rsize %d rRsize %d\n", ns->my_pe_num, 
        ns->my_pe->pendingReqs.size(), ns->my_pe->pendingRReqs.size());
    }

    if(ns->my_pe->pendingRCollMsgs.size() != 0) {
      printf("%d rcollsize %d \n", ns->my_pe_num, 
        ns->my_pe->pendingRCollMsgs.size());
    }

    int count = 0;
    std::map<int64_t, std::map<int64_t, std::map<int, int> > >::iterator it =
      ns->my_pe->pendingCollMsgs.begin();
    while(it != ns->my_pe->pendingCollMsgs.end()) {
      count += it->second.size();
      it++;
    }

    if(count != 0) {
      printf("%d collsize %d \n", ns->my_pe_num, count);
    }

    if(jobTime > jobTimes[ns->my_job]) {
        jobTimes[ns->my_job] = jobTime;
    }
    if(finalTime > finalizeTimes[ns->my_job]) {
        finalizeTimes[ns->my_job] = finalTime;
    }

    return;
}

/* convert ns to seconds */
tw_stime ns_to_s(tw_stime ns)
{
    return(ns / (1000.0 * 1000.0 * 1000.0));
}

/* convert seconds to ns */
tw_stime s_to_ns(tw_stime s)
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
    printf("PE%d: handle_kickoff_rev_event. TIME now:%f.\n", ns->my_pe_num, now);
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
    if(1 || ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
      printf("%d Set busy false %d\n", ns->my_pe_num, task_id);
    }
#endif
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
    m->fwd_dep_count = counter;
    m->executed = buffd_task;
    if(buffd_task.taskid != -1){
        exec_task(ns, buffd_task, lp, m, b); //we don't care about the return value?
    }
}

static void handle_exec_rev_event(
		proc_state * ns,
		tw_bf * b,
		proc_msg * m,
		tw_lp * lp)
{
    if(b->c2) return;
    int task_id = m->msgId.id;

    //Reverse the state: set the PE as busy, task is not completed yet
    PE_set_busy(ns->my_pe, true);
#if DEBUG_PRINT
    if(1 || ns->my_pe_num == 1024 || ns->my_pe_num == 11788) {
      printf("%d Rev Set busy true %d\n", ns->my_pe_num, task_id);
    }
#endif

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


//Utility function to convert pe number to tw_lpid number
//Assuming the servers come first in lp registration in terms of global id
int pe_to_lpid(int pe, int job){
    int server_num = jobs[job].rankMap[pe];
    return codes_mapping_get_lpid_from_relative(server_num, NULL, "server", NULL, 0);
}

//Utility function to convert tw_lpid to simulated pe number
//Assuming the servers come first in lp registration in terms of global id
int lpid_to_pe(int lp_gid){
    int server_num = codes_mapping_get_lp_relative_id(lp_gid, 0, NULL);
    return global_rank[server_num].mapsTo;
}
inline int lpid_to_job(int lp_gid){
    int server_num = codes_mapping_get_lp_relative_id(lp_gid, 0, NULL);
    return global_rank[server_num].jobID;;
}
inline int pe_to_job(int pe){
    return global_rank[pe].jobID;;
}

bool isPEonThisRank(int jobID, int i) {
  int lpid = pe_to_lpid(i, jobID);
  int pe = codes_mapping(lpid);
  return pe == rank;
}
