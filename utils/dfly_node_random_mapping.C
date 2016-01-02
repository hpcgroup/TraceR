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

  int numNodes = numGroups * numRouters * skip;
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

  int currNode = 0;
  for(int r = 0; r < numRouters; r++) {
    for(int j = 0; j < rr_group; j++) {
      if(j >= skip) {
        break;
      }
      for(int g = 0; g < numGroups; g++) {
        int i = g * size_per_group + r * rr_group * rr;
        int useRank = mapping[currNode] * rr;
        if(useRank >= numAllocCores) {
          currNode++;
          continue;
        }
        for(int k = 0; k < rr; k++) {
          int global_rank = i + j + k * rr_group;
          fwrite(&global_rank, sizeof(int), 1, binout);
          fwrite(&useRank, sizeof(int), 1, binout);
          fwrite(&jobid, sizeof(int), 1, binout);
          granks[useRank] = global_rank;
          //fwrite(&global_rank, sizeof(int), 1, out_files);
          printf("%d %d %d\n", global_rank, useRank++, jobid);
          local_rank++;
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
  }
  
  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
    printf("%d\n", granks[i]);
  }

  fclose(binout);
  fclose(out_files);
}
