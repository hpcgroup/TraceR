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
#include <cassert>

#define LINEAR 1
#define RNODES 2
#define CLUSTERED 3
#define FIXED_SIZES 4
#define FTREE_PODS 5

#define J_LINEAR 1
#define J_BLOCKED 2
#define J_RAND_BLOCKED 3

#define FILL_SPACE 0

using namespace std;
void allocateJob(int job_num, vector<int> &routers);

int rr_group, rr, skip, numGroups, numRouters, totalRouters, totalNodes;
FILE *binout, *job_configs;

int main(int argc, char**argv) {
  if(argc < 11) {
    printf("Correct usage: %s <global_file_name> <node_distribution_type> <num jobs> <nodes per job> <nodes per router> <cores per node> <nodes to skip after> <number of groups> <number of routers per group> <job config>\n",
        argv[0]);
    printf("Node distribution type: 1 - Linear, 2 - Random nodes, 3 - Clusterd\n");
    printf("Within-Job distribution types: 1 - Linear, 2 - Blocked\n");
    printf("job_config.input shows the information needed in job config.");
    exit(1);
  }

  int arg_cnt = 1;
  binout = fopen(argv[arg_cnt++], "wb");
  int node_dist = atoi(argv[arg_cnt++]);
  int num_jobs = atoi(argv[arg_cnt++]);
  int *job_nodes = new int[num_jobs];

  int totalNodesNeeded = 0;
  for(int i = 0; i < num_jobs; i++) {
    job_nodes[i] = atoi(argv[arg_cnt++]);
    totalNodesNeeded += job_nodes[i];
  }
  rr_group = atoi(argv[arg_cnt++]);
  rr = atoi(argv[arg_cnt++]);
  skip = atoi(argv[arg_cnt++]);
  numGroups = atoi(argv[arg_cnt++]);
  numRouters = atoi(argv[arg_cnt++]);
  totalRouters = numGroups * numRouters;
  totalNodes = totalRouters * skip;
  
  srand(7621);
  job_configs = fopen(argv[arg_cnt++], "r");
 
  vector<int> *nodes = new vector<int>[num_jobs];

#if FILL_SPACE
  int *fillSpaceNodes = new int[totalNodesNeeded];
  int leaveOutCount = totalNodes - totalNodesNeeded;
  int leaveEvery = totalNodes/leaveOutCount + ((totalNodes%leaveOutCount)?1:0);
  int inc = 0;
  for(int i = 0; i < totalNodes && inc < totalNodesNeeded; i++) {
    if(i != 0 && (i % leaveEvery == 0))
      printf("Left %d\n", i);
    else 
      fillSpaceNodes[inc++] = i;
  }
  assert(inc == totalNodesNeeded);
#else 
  int *fillSpaceNodes = new int[totalNodes];
  for(int i = 0; i < totalNodes; i++) {
    fillSpaceNodes[i] = (i / skip) * rr_group + (i % skip);
  }
#endif


  if(node_dist == LINEAR) {
    int node = 0;
    int count = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_nodes[i]; j++) {
#if FILL_SPACE
        nodes[i].push_back(fillSpaceNodes[node++]);
#else
        nodes[i].push_back(node++);
#endif
        count++;
        if(count % skip == 0) {
          node += (rr_group - skip);
          count = 0;
        }
      }
    }
  } else if(node_dist == RNODES) {
    int *tempNodes = new int[totalNodes];
    for(int i = 0; i < totalNodes; i++) {
      tempNodes[i] = (i / skip) * rr_group + (i % skip);
    }
    for(int i = 0; i < 4*totalNodes; i++) {
      int r1 = rand() % totalNodes;
      int r2 = rand() % totalNodes;
      int rt = tempNodes[r1];
      tempNodes[r1] = tempNodes[r2];
      tempNodes[r2] = rt;
    }
    int node = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_nodes[i]; j++) {
        nodes[i].push_back(tempNodes[node++]);
      }
    }
    delete [] tempNodes;
  } else if(node_dist == CLUSTERED) {
    for(int i = 0; i < 500; i++) {
      int r1 = rand() % totalNodesNeeded;
      int r2 = rand() % totalNodesNeeded;
      int rt = fillSpaceNodes[r1];
      fillSpaceNodes[r1] = fillSpaceNodes[r2];
      fillSpaceNodes[r2] = rt;
    }
    int node = 0;
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_nodes[i]; j++) {
        nodes[i].push_back(fillSpaceNodes[node++]);
        printf("%d\n",fillSpaceNodes[node-1]);
      }
    }
  } else if(node_dist == FIXED_SIZES) {
    int *tempNode = new int[totalNodes];
    int node = 0;
    int last_half_z_plane = -1;
    int half_plane_size = 10 * 6 * 9;
    int current_z = 0;
    int count = 0;
    for(int i = 0; i < num_jobs; i++) {
      int num_planes = (job_nodes[i]/half_plane_size) + 
                       ((job_nodes[i] % half_plane_size) ? 1 : 0);
      assert(num_planes == 1 || num_planes % 2 == 0);
      int leaveEvery = (num_planes * half_plane_size)/((num_planes * half_plane_size) - job_nodes[i]);
      leaveEvery += (((num_planes * half_plane_size)%((num_planes * half_plane_size) - job_nodes[i]))?1:0);
      int base_node, inc = 0;
      if(num_planes == 1) {
        if(last_half_z_plane != -1) {
          base_node = last_half_z_plane * 12 * 10 * 9 + half_plane_size;
          if(last_half_z_plane == current_z) {
            current_z++;
          }
          last_half_z_plane = -1;
        } else {
          last_half_z_plane = current_z;
          base_node = last_half_z_plane * 12 * 10 * 9;
          current_z++;
        }
        for(int j = 0; (j < (num_planes * half_plane_size)) && (inc < job_nodes[i]); j++) {
          if(j == 0 || (j % leaveEvery != 0)) {
            tempNode[count++] = base_node++;
            inc++;
          } else {
            base_node++;
          }
        }
        assert(inc == job_nodes[i]);
      } else {
        int base_node = current_z * 12 * 10 * 9;
        for(int j = 0; (j < (num_planes * half_plane_size)) && (inc < job_nodes[i]); j++) {
          if(j == 0 || (j % leaveEvery != 0)) {
            tempNode[count++] = base_node++;
            inc++;
          } else {
            base_node++;
          }
        }
        assert(inc == job_nodes[i]);
        current_z += num_planes/2;
      }
    }
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_nodes[i]; j++) {
        nodes[i].push_back(tempNode[node++]);
        //printf("%d %d %d\n", i, j, tempNode[node-1]);
      }
    }
    delete [] tempNode;
  } else if(node_dist == FTREE_PODS) {
    int *tempNode = new int[totalNodes];
    int node = 0;
    int pod_size = 24 * 24;
    int current_pod = 0;
    int count = 0;
    for(int i = 0; i < num_jobs; i++) {
      int num_pods = (job_nodes[i]/pod_size) + 
                       ((job_nodes[i] % pod_size) ? 1 : 0);
      int leaveEvery = (num_pods * pod_size)/((num_pods * pod_size) - job_nodes[i]);
      leaveEvery += (((num_pods * pod_size)%((num_pods * pod_size) - job_nodes[i]))?1:0);
      int inc = 0;
      int base_node = current_pod * pod_size;
      for(int j = 0; (j < (num_pods * pod_size)) && (inc < job_nodes[i]); j++) {
        if(j == 0 || (j % leaveEvery != 0)) {
          tempNode[count++] = base_node++;
          inc++;
        } else {
          base_node++;
        }
      }
      assert(inc == job_nodes[i]);
      current_pod += num_pods;
    }
    for(int i = 0; i < num_jobs; i++) {
      for(int j = 0; j < job_nodes[i]; j++) {
        nodes[i].push_back(tempNode[node++]);
        //printf("%d %d %d\n", i, j, tempNode[node-1]);
      }
    }
    delete [] tempNode;
  } else {
    printf("Unknown node dist\n");
  }

  for(int i = 0; i < num_jobs; i++) {
    allocateJob(i, nodes[i]);
  }
  
  fclose(binout);
}

void allocateJob(int job_num, vector<int> &nodes) {
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
    prod_xyz = rr;
  } else if(map_type == J_BLOCKED || map_type == J_RAND_BLOCKED) {
    assert(prod_xyz == rr);
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
  
  int *mapping = new int[nodes.size()];
  for(int i = 0; i < nodes.size(); i++) {
    mapping[i] = i;
  }
  
  if(map_type == J_RAND_BLOCKED) {
    for(int i = 0; i < 4*nodes.size(); i++) {
      int node1 = rand() % nodes.size();
      int node2 = rand() % nodes.size();
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
  for(int r = 0; (r < nodes.size()) && cont; r++) {
    int i = (nodes[r] / rr_group) * rr_group * rr;
    int useRank = mapping[r] * prod_xyz;
    if(useRank >= numAllocCores) continue;
    for(int k = 0; k < prod_xyz; k++) {
      int global_rank = i + (nodes[r] % rr_group) + k * rr_group;
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
