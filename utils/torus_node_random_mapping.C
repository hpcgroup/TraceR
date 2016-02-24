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

  if(argc < 5) {
    printf("Correct usage: %s <global_file_name> <total ranks> <number of ranks per node> <number of nodes>\n",
        argv[0]);
    exit(1);
  }

  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", 0);
  out_files = fopen(dFILE, "wb");

  int numAllocCores = atoi(argv[2]);
  int rr = atoi(argv[3]);
  int numNodes = atoi(argv[4]);
  int local_rank = 0;

  int *mapping = new int[numNodes];
  int *granks = new int[numAllocCores];
  for(int i = 0; i < numNodes; i++) {
    mapping[i] = i;
  }

  srand(1331);
  for(int i = 0; i < numNodes; i++) {
    int node1 = rand() % numNodes;
    int node2 = rand() % numNodes;
    int temp = mapping[node1];
    mapping[node1] = mapping[node2];
    mapping[node2] = temp;
  }

  for(int currNode = 0; currNode < numNodes; currNode++) {
    int i = currNode * rr;
    int useRank = mapping[currNode] * rr;
    if(useRank >= numAllocCores) continue;
    for(int k = 0; k < rr; k++) {
      int global_rank = i + k;
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
  
  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
  }

  fclose(binout);
  fclose(out_files);
}
