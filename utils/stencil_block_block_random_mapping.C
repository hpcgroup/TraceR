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
    printf("Correct usage: %s <global_file_name> <total ranks> <nodes per router> <cores per node> <nodes to skip after> <number of groups> <number of routers per group> <stencil dims> <block dims> <router shape>\n",
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
  int box_x = atoi(argv[8]);
  int box_y = atoi(argv[9]);
  int box_z = atoi(argv[10]);
  int s_x = atoi(argv[11]);
  int s_y = atoi(argv[12]);
  int s_z = atoi(argv[13]);
  int r_x = atoi(argv[14]);
  int r_y = atoi(argv[15]);
  int r_z = atoi(argv[16]);
  int local_rank = 0;
  int size_per_group = numRouters * rr_group * rr;
  int *localRanks = new int[numAllocCores];
  int *granks = new int[numAllocCores];
  int bx = r_x;
  int by = r_y;
  int bz = r_z;
  int prod_xyz = s_x * s_y * s_z;
  int prod_xy = s_x * s_y;

  int bigger_box =  skip * prod_xyz;
  int bbx = box_x/(r_x*s_x);
  int bby = box_y/(r_y*s_y);
  int bbz = box_z/(r_z*s_z);

  for(int i = 0; i < numAllocCores; i++) {
    int bigger_box_num = i / bigger_box;
    int my_x = (bigger_box_num % bbx) * r_x * s_x;
    int my_y = ((bigger_box_num / bbx) % bby) * r_y * s_y;
    int my_z = (bigger_box_num / (bbx * bby)) * r_z * s_z;

    int box_num = (i % bigger_box) / prod_xyz;
    int box_rank = ((i % bigger_box)) % prod_xyz;
    my_x += (box_num % bx) * s_x;
    my_y += ((box_num / bx) % by) * s_y;
    my_z += (box_num / (bx*by)) * s_z;

    my_x += box_rank % s_x;
    my_y += (box_rank / s_x) % s_y;
    my_z += box_rank / (s_x * s_y);

    int my_rank = my_x + my_y * box_x + my_z * box_x * box_y;
    localRanks[i] = my_rank;
  }
  
  int totalRouters = numGroups * numRouters;
  int *mapping = new int[totalRouters];
  for(int i = 0; i < totalRouters ; i++) {
    mapping[i] = i;
  }

  srand(1331);
  for(int i = 0; i < numRouters; i++) {
    int node1 = rand() % numRouters;
    int node2 = rand() % numRouters;
    int temp = mapping[node1];
    mapping[node1] = mapping[node2];
    mapping[node2] = temp;
  }

  bool cont = true;
  int currRouter = 0;
  for(int g = 0; cont && (g < numGroups); g++) {
    for(int r = 0; (r < numRouters) && cont; r++) {
      for(int j = 0; cont && (j < rr_group); j++) {
        if(j >= skip) {
          break;
        }
        int i = g * size_per_group + r * rr_group * rr;
        int useRank = mapping[currRouter] * skip * rr + j * rr;
        if(useRank >= numAllocCores) continue;
        for(int k = 0; k < rr; k++) {
          int global_rank = i + j + k * rr_group;
          fwrite(&global_rank, sizeof(int), 1, binout);
          fwrite(&localRanks[useRank], sizeof(int), 1, binout);
          fwrite(&jobid, sizeof(int), 1, binout);
          granks[localRanks[useRank]] = global_rank;
#if PRINT_MAP
          printf("%d %d %d\n", global_rank, localRanks[useRank++], jobid);
#else
          useRank++;
#endif
          local_rank++;
          if(local_rank == numAllocCores) {
            cont = false;
            break;
          }
        }
      }
      currRouter++;
    }
  }

  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
  }
  fclose(binout);
  fclose(out_files);
}
