//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// Written by:
//     Nikhil Jain <nikhil.jain@acm.org>
//     Bilge Acun <acun2@illinois.edu>
//     Abhinav Bhatele <bhatele@llnl.gov>
//
// LLNL-CODE-740483. All rights reserved.
//
// This file is part of TraceR. For details, see:
// https://github.com/LLNL/TraceR
// Please also read the LICENSE file for the MIT License notice.
//////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <vector>

using namespace std;

int main(int argc, char**argv) {
  FILE *binout = fopen(argv[1], "wb");
  int jobid = 0;
  FILE* out_files;

  if(argc < 8) {
    printf("Correct usage: %s <global_file_name> <total ranks> <number of nodes per router> <number of ranks per node> <skip after node> <number of groups> <number of routers per group>\n",
        argv[0]);
    exit(1);
  }

  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", 0);
  out_files = fopen(dFILE, "wb");

  int numAllocCores = atoi(argv[2]);
  int rr_group = atoi(argv[3]);
  int rr = atoi(argv[4]);
  int skip = atoi(argv[5]);
  int numGroups = atoi(argv[6]);
  int numRouters = atoi(argv[7]);
  int local_rank = 0;
  int size_per_group = numRouters * rr_group * rr;
 
  for(int r = 0; r < numRouters; r++) {
    for(int g = 0; g < numGroups; g++) {
      for(int j = 0; j < rr_group; j++) {
        if(j >= skip) {
          break;
        }
        int i = g * size_per_group + r * rr_group * rr;
        for(int k = 0; k < rr; k++) {
          int global_rank = i + j + k * rr_group;
          fwrite(&global_rank, sizeof(int), 1, binout);
          fwrite(&local_rank, sizeof(int), 1, binout);
          fwrite(&jobid, sizeof(int), 1, binout);
          fwrite(&global_rank, sizeof(int), 1, out_files);
#if PRINT_MAP
          printf("%d %d %d\n", global_rank, local_rank, jobid);
#endif
          local_rank++;
          if(local_rank == numAllocCores) {
            break;
          }
        }
        if(local_rank == numAllocCores) {
          break;
        }
      }
      if(local_rank == numAllocCores) {
        break;
      }
    }
    if(local_rank == numAllocCores) {
      break;
    }
  }

  fclose(binout);
  fclose(out_files);
}
