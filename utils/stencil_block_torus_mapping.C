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
#include <assert.h>

using namespace std;

int main(int argc, char**argv) {
  FILE *binout = fopen(argv[1], "wb");
  int jobid = 0;
  FILE* out_files;

  if(argc < 3) {
    printf("Correct usage: %s <global_file_name> <total ranks> <stencil dims> <block dims> \n",
        argv[0]);
    exit(1);
  }

  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", 0);
  out_files = fopen(dFILE, "wb");

  int numAllocCores = atoi(argv[2]);
  int box_x = atoi(argv[3]);
  int box_y = atoi(argv[4]);
  int box_z = atoi(argv[5]);
  int s_x = atoi(argv[6]);
  int s_y = atoi(argv[7]);
  int s_z = atoi(argv[8]);
  int local_rank = 0;
  int *localRanks = new int[numAllocCores];
  int *granks = new int[numAllocCores];
  int prod_xyz = s_x * s_y * s_z;
  int prod_xy = s_x * s_y;

  int bigger_box =  64 * prod_xyz;
  int bbx = box_x/(4*s_x);
  int bby = box_y/(4*s_y);
  int bbz = box_z/(4*s_z);
  int bx = 4;
  int by = 4;
  int bz = 4;

  for(int i = 0; i < numAllocCores; i++) {
    localRanks[i] = -1;
    granks[i] = -1;
  }
  for(int i = 0; i < numAllocCores; i++) {
    int bigger_box_num = i / bigger_box;
    int my_x = (bigger_box_num % bbx) * 4 * s_x;
    int my_y = ((bigger_box_num / bbx) % bby) * 4 * s_y;
    int my_z = (bigger_box_num / (bbx * bby)) * 4 * s_z;

    int box_num = (i % bigger_box) / prod_xyz;
    int box_rank = ((i % bigger_box)) % prod_xyz;
    my_x += (box_num % bx) * s_x;
    my_y += ((box_num / bx) % by) * s_y;
    my_z += (box_num / (bx*by)) * s_z;

    my_x += box_rank % s_x;
    my_y += (box_rank / s_x) % s_y;
    my_z += box_rank / (s_x * s_y);

    int my_rank = my_x + my_y * box_x + my_z * box_x * box_y;
    assert(localRanks[i] == -1);
    localRanks[i] = my_rank;
  }
  for(int i = 0; i < numAllocCores; i++) {
    assert(localRanks[i] != -1);
  }

  int d[6], o[6]; 
  bool cont = true;
  for(o[0] = 0; cont && (o[0] < 6); o[0] += 2) {
  for(o[1] = 0; cont && (o[1] < 6); o[1] += 2) {
  for(o[2] = 0; cont && (o[2] < 6); o[2] += 2) {
  for(o[3] = 0; cont && (o[3] < 6); o[3] += 2) {
  for(o[4] = 0; cont && (o[4] < 6); o[4] += 2) {
  for(o[5] = 0; cont && (o[5] < 6); o[5] += 2) {
  for(d[0] = o[0]; cont && (d[0] < o[0] + 2); d[0]++) {
  for(d[1] = o[1]; cont && (d[1] < o[1] + 2); d[1]++) {
  for(d[2] = o[2]; cont && (d[2] < o[2] + 2); d[2]++) {
  for(d[3] = o[3]; cont && (d[3] < o[3] + 2); d[3]++) {
  for(d[4] = o[4]; cont && (d[4] < o[4] + 2); d[4]++) {
  for(d[5] = o[5]; cont && (d[5] < o[5] + 2); d[5]++) {
    int realNode = d[5] + d[4] * 6 + d[3] * 36 + d[2] * 216 + d[1] * 1296 + d[0] * 7776;
    int base_global_rank = realNode * 16;
    for(int j = 0; j < 16; j++) {
      int global_rank = base_global_rank + j;
      fwrite(&global_rank, sizeof(int), 1, binout);
      fwrite(&localRanks[local_rank], sizeof(int), 1, binout);
      fwrite(&jobid, sizeof(int), 1, binout);
      assert(granks[localRanks[local_rank]] == -1);
      granks[localRanks[local_rank]] = global_rank;
#if PRINT_MAP
        printf("%d %d %d\n", global_rank, localRanks[local_rank], jobid);
#endif
      local_rank++;
      if(local_rank == numAllocCores) {
        cont = false;
        break;
      }
    }
  }}}}}}}}}}}}

  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
  }
  fclose(binout);
  fclose(out_files);
}
