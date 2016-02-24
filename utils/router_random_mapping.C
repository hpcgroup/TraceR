//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// Written by:
//     Nikhil Jain <nikhil.jain@acm.org>
//     Bilge Acun <acun2@illinois.edu>
//     Abhinav Bhatele <bhatele@llnl.gov>
//
// LLNL-CODE-681378. All rights reserved.
//
// This file is part of TraceR. For details, see:
// https://github.com/LLNL/tracer
// Please also read the LICENSE file for our notice and the LGPL.
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

  int totalRouters = numGroups * numRouters;
  int *mapping = new int[totalRouters];
  int *granks = new int[numAllocCores];
  for(int i = 0; i < totalRouters; i++) {
    mapping[i] = i;
  }

  srand(1331);
  for(int i = 0; i < totalRouters; i++) {
    int node1 = rand() % totalRouters;
    int node2 = rand() % totalRouters;
    int temp = mapping[node1];
    mapping[node1] = mapping[node2];
    mapping[node2] = temp;
  }

  int currNode = 0;
  for(int r = 0; r < numRouters; r++) {
    for(int g = 0; g < numGroups; g++) {
      for(int j = 0; j < rr_group; j++) {
        if(j >= skip) {
          break;
        }
        int i = g * size_per_group + r * rr_group * rr;
        int useRank = mapping[currNode] * skip * rr + j * rr;
        if(useRank >= numAllocCores) continue;
        for(int k = 0; k < rr; k++) {
          int global_rank = i + j + k * rr_group;
          fwrite(&global_rank, sizeof(int), 1, binout);
          fwrite(&useRank, sizeof(int), 1, binout);
          fwrite(&jobid, sizeof(int), 1, binout);
          granks[useRank] = global_rank;
#if PRINT_MAP
          printf("%d %d %d\n", global_rank, useRank++, jobid);
#else
          useRank++;
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
      currNode++;
    }
    if(local_rank == numAllocCores) {
      break;
    }
  }
  
  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
  }

  fclose(binout);
  fclose(out_files);
}
