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

  if(argc < 6) {
    printf("Correct usage: %s <global_file_name> <total ranks> <nodes per router> <cores per node> <nodes to skip after>\n",
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
  int local_rank = 0;

  int *localRanks = new int[numAllocCores];
  int *granks = new int[numAllocCores];

  for(int i = 0; i < numAllocCores; i++) {
    localRanks[i] = i;
  }
  srand(1789637);

  for(int i = 0; i < 5*numAllocCores; i++) {
    int t1 = rand() % numAllocCores;
    int t2 = rand() % numAllocCores;
    int tmp = localRanks[t1];
    localRanks[t1] = localRanks[t2];
    localRanks[t2] = tmp;
  }

  
  for(int i = 0; ; i += rr_group*rr) {
    for(int j = 0; j < rr_group; j++) {
      if(j == skip) {
        break;
      }
      for(int k = 0; k < rr; k++) {
        int global_rank = i + j + k * rr_group;
        fwrite(&global_rank, sizeof(int), 1, binout);
        fwrite(&localRanks[local_rank], sizeof(int), 1, binout);
        fwrite(&jobid, sizeof(int), 1, binout);
        granks[localRanks[local_rank]] = global_rank;
#if PRINT_MAP
        printf("%d %d %d\n", global_rank, localRanks[local_rank], jobid);
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
  
  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
  }

  fclose(binout);
  fclose(out_files);
}
