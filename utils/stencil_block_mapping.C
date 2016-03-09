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
    printf("Correct usage: %s <global_file_name> <total ranks> <nodes per router> <cores per node> <nodes to skip after> <stencil dims> <block dims>\n",
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
  int box_x = atoi(argv[6]);
  int box_y = atoi(argv[7]);
  int box_z = atoi(argv[8]);
  int s_x = atoi(argv[9]);
  int s_y = atoi(argv[10]);
  int s_z = atoi(argv[11]);

  int *localRanks = new int[numAllocCores];
  int bx = box_x/s_x;
  int by = box_y/s_y;
  int bz = box_z/s_z;
  int prod_xyz = s_x * s_y * s_z;
  int prod_xy = s_x * s_y;

  for(int i = 0; i < numAllocCores; i++) {
    int box_num = i / prod_xyz;
    int box_rank = i % prod_xyz;
    int x = box_num % bx;
    int y = (box_num / bx) % by;
    int z = box_num / (bx*by);

    int my_x = x * s_x + box_rank % s_x;
    int my_y = y * s_y + (box_rank / s_x) % s_y;
    int my_z = z * s_z + box_rank / (s_x * s_y);

    int my_rank = my_x + my_y * box_x + my_z * box_x * box_y;
   
    localRanks[i] = my_rank;
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
        fwrite(&global_rank, sizeof(int), 1, out_files);
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

  fclose(binout);
  fclose(out_files);
}
