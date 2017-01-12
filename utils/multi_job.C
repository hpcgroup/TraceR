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
#include "hilbert.h"
#include <assert.h>

#define LINEAR 1
#define RROUTER 2
#define CLUSTERED 3
#define FIXED_SIZES 4
#define HILBERT 5

#define J_LINEAR 1
#define J_BLOCKED 2
#define J_RAND_BLOCKED 3

#define FILL_SPACE 1

using namespace std;
void allocateJob(int job_num, vector<int> &routers);

int rr_group, rr, skip, numGroups, numRouters, totalRouters;
FILE *binout, *job_configs;

int main(int argc, char**argv) {
  if(argc < 11) {
    printf("Correct usage: %s <global_file_name> <router_distribution_type> <num jobs> <routers per job> <nodes per router> <cores per node> <nodes to skip after> <number of groups> <number of routers per group> <job config>\n",
        argv[0]);
    printf("Router distribution type: 1 - LINEAR, 2 - Random router\n");
    printf("Within-Job distribution types: 1 - LINEAR, 2 - Blocked, 3 - Blocked random\n");
    exit(1);
  }

  int arg_cnt = 1;
  binout = fopen(argv[arg_cnt++], "wb");
  int router_dist = atoi(argv[arg_cnt++]);
  int num_jobs = atoi(argv[arg_cnt++]);
  int *job_routers = new int[num_jobs];

  int totalRoutersNeeded = 0;
  for(int i = 0; i < num_jobs; i++) {
    job_routers[i] = atoi(argv[arg_cnt++]);
    totalRoutersNeeded += job_routers[i];
  }
  rr_group = atoi(argv[arg_cnt++]);
  rr = atoi(argv[arg_cnt++]);
  skip = atoi(argv[arg_cnt++]);
  numGroups = atoi(argv[arg_cnt++]);
  numRouters = atoi(argv[arg_cnt++]);
  totalRouters = numGroups * numRouters;
  
  srand(7621);
  job_configs = fopen(argv[arg_cnt++], "r");
 
  vector<int> *routers = new vector<int>[num_jobs];

#if FILL_SPACE
  int *fillSpaceRouters = new int[totalRoutersNeeded];
  int leaveOutCount = totalRouters - totalRoutersNeeded;
  int leaveEvery = totalRouters/leaveOutCount + ((totalRouters%leaveOutCount)?1:0);
  int inc = 0;
  for(int i = 0; i < totalRouters && inc < totalRoutersNeeded; i++) {
    if(i != 0 && (i % leaveEvery == 0))
      printf("Left %d\n", i);
    else 
      fillSpaceRouters[inc++] = i;
  }
  assert(inc == totalRoutersNeeded);
#endif

  if(router_dist == LINEAR) {
    int router = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_routers[i]; j++) {
#if FILL_SPACE
        routers[i].push_back(fillSpaceRouters[router++]);
#else
        routers[i].push_back(router++);
#endif
      }
    }
  } else if(router_dist == RROUTER) {
    int *tempRouter = new int[totalRouters];
    for(int i = 0; i < totalRouters; i++) {
      tempRouter[i] = i;
    }
    for(int i = 0; i < 5*totalRouters; i++) {
      int r1 = rand() % totalRouters;
      int r2 = rand() % totalRouters;
      int rt = tempRouter[r1];
      tempRouter[r1] = tempRouter[r2];
      tempRouter[r2] = rt;
    }
    int router = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_routers[i]; j++) {
        routers[i].push_back(tempRouter[router++]);
      }
    }
    delete [] tempRouter;
  } else if(router_dist == HILBERT) {
    int *tempRouter = new int[totalRouters];
    vector<int> hcoords;
    int count = 0;
    for(int i = 0; i < (16 * 16 * 16); i++) {
      hcoords = int_to_Hilbert(i,3);
      if((hcoords[0] >= 12) || (hcoords[1] >= 10) || (hcoords[2] >= 10)) continue;
      int newIndex = hcoords[0] + hcoords[1] * 12 + hcoords[2] * 12 * 10;
      tempRouter[count++] = newIndex;
    }
    int router = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_routers[i]; j++) {
        routers[i].push_back(tempRouter[router++]);
      }
    }
    delete [] tempRouter;
  } else if(router_dist == FIXED_SIZES) {
    int *tempRouter = new int[totalRouters];
    int router = 0;
    int last_half_z_plane = -1;
    int half_plane_size = 10 * 6;
    int current_z = 0;
    int count = 0;
    for(int i = 0; i < num_jobs; i++) {
      int num_planes = (job_routers[i]/half_plane_size) + 
                       ((job_routers[i] % half_plane_size) ? 1 : 0);
      assert(num_planes == 1 || num_planes % 2 == 0);
      int leaveEvery = (num_planes * half_plane_size)/((num_planes * half_plane_size) - job_routers[i]);
      leaveEvery += (((num_planes * half_plane_size)%((num_planes * half_plane_size) - job_routers[i]))?1:0);
      int base_router, inc = 0;
      if(num_planes == 1) {
        if(last_half_z_plane != -1) {
          base_router = last_half_z_plane * 12 * 10 + half_plane_size;
          if(last_half_z_plane == current_z) {
            current_z++;
          }
          last_half_z_plane = -1;
        } else {
          last_half_z_plane = current_z;
          base_router = last_half_z_plane * 12 * 10;
          current_z++;
        }
        for(int j = 0; (j < (num_planes * half_plane_size)) && (inc < job_routers[i]); j++) {
          if(j == 0 || (j % leaveEvery != 0)) {
            tempRouter[count++] = base_router++;
            inc++;
          } else {
            base_router++;
          }
        }
        assert(inc == job_routers[i]);
      } else {
        int base_router = current_z * 12 * 10;
        for(int j = 0; (j < (num_planes * half_plane_size)) && (inc < job_routers[i]); j++) {
          if(j == 0 || (j % leaveEvery != 0)) {
            tempRouter[count++] = base_router++;
            inc++;
          } else {
            base_router++;
          }
        }
        assert(inc == job_routers[i]);
        current_z += num_planes/2;
      }
    }
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_routers[i]; j++) {
        routers[i].push_back(tempRouter[router++]);
        //printf("%d %d %d\n", i, j, tempRouter[router-1]);
      }
    }
    delete [] tempRouter;
  } else if(router_dist == CLUSTERED) {
    for(int i = 0; i < totalRoutersNeeded/25; i++) {
      int r1 = rand() % totalRoutersNeeded;
      int r2 = rand() % totalRoutersNeeded;
      int rt = fillSpaceRouters[r1];
      fillSpaceRouters[r1] = fillSpaceRouters[r2];
      fillSpaceRouters[r2] = rt;
    }
    int router = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_routers[i]; j++) {
        routers[i].push_back(fillSpaceRouters[router++]);
      }
    }
  } else {
    printf("Unknown router distributions\n");
    exit(0);
  }

  for(int i = 0; i < num_jobs; i++) {
    allocateJob(i, routers[i]);
  }
  
  fclose(binout);
}

void allocateJob(int job_num, vector<int> &routers) {
  int numAllocCores, map_type;
  fscanf(job_configs, "%d %d", &numAllocCores, &map_type);
  int box_x, box_y, box_z;
  fscanf(job_configs, "%d %d %d", &box_x, &box_y, &box_z);
  int s_x, s_y, s_z;
  fscanf(job_configs, "%d %d %d", &s_x, &s_y, &s_z);
  int r_x, r_y, r_z;
  fscanf(job_configs, "%d %d %d", &r_x, &r_y, &r_z);

  int *localRanks = new int[numAllocCores];
  int *granks = new int[numAllocCores];
  int bx = r_x;
  int by = r_y;
  int bz = r_z;
  int prod_xyz = s_x * s_y * s_z;
  int prod_xy = s_x * s_y;

  if(map_type == J_LINEAR) {
    for(int i = 0; i < numAllocCores; i++) {
      localRanks[i] = i;
    }
  } else if(map_type == J_BLOCKED || map_type == J_RAND_BLOCKED) {
    int bigger_box =  r_x*r_y*r_z * prod_xyz;
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
  }
  
  int *mapping = new int[routers.size()];
  for(int i = 0; i < routers.size(); i++) {
    mapping[i] = i;
  }

  if(map_type == J_RAND_BLOCKED) {
    for(int i = 0; i < 4*routers.size(); i++) {
      int node1 = rand() % routers.size();
      int node2 = rand() % routers.size();
      int temp = mapping[node1];
      mapping[node1] = mapping[node2];
      mapping[node2] = temp;
    }
  }

  bool cont = true;
  int local_rank = 0, ZERO=0;
  
  char gFILE[256];
  sprintf(gFILE, "%s%d", "global", job_num);
  FILE* global_file = fopen(gFILE, "wb");
  for(int r = 0; (r < routers.size()) && cont; r++) {
    for(int j = 0; cont && (j < rr_group); j++) {
      if(j >= skip) {
        break;
      }
      int i = routers[r] * rr_group * rr;
      int useRank = mapping[r] * skip * prod_xyz + j * prod_xyz;
      if(useRank >= numAllocCores) continue;
      for(int k = 0; k < prod_xyz; k++) {
        int global_rank = i + j + k * rr_group;
        fwrite(&global_rank, sizeof(int), 1, binout);
        fwrite(&localRanks[useRank], sizeof(int), 1, binout);
        fwrite(&job_num, sizeof(int), 1, binout);
        fwrite(&global_rank, sizeof(int), 1, global_file);
        fwrite(&localRanks[useRank], sizeof(int), 1, global_file);
        fwrite(&ZERO, sizeof(int), 1, global_file);
        granks[localRanks[useRank]] = global_rank;
#if PRINT_MAP
        printf("%d %d %d\n", global_rank, localRanks[useRank++], job_num);
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
  }
  fclose(global_file);

  FILE* out_file;
  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", job_num);
  out_file = fopen(dFILE, "wb");

  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_file);
  }
  fclose(out_file);
  delete [] localRanks;
  delete [] granks;
  delete [] mapping;
}
