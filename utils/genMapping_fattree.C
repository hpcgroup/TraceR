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
    printf("Correct usage: %s <global_file_name> <total ranks> <rr group> <#rr>\n",
        argv[0]);
    exit(1);
  }

  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", 0);
  out_files = fopen(dFILE, "wb");

  int numAllocCores = atoi(argv[2]);
  int rr_group = atoi(argv[3]);
  int rr = atoi(argv[4]);
  int numRouters = atoi(argv[5]);
  int numNodes = atoi(argv[6]);
  int *mapping = new int[numNodes];
  int *granks = new int[numAllocCores];
  for(int i = 0; i < numNodes; i++) {
    mapping[i] = (i/144) + (i % 144) * 324;
  }

  int currNode = 0;
  int local_rank = 0;
  for(int r = 0; r < numRouters; r++) {
    int i = r * rr_group * rr;
    for(int j = 0; j < rr_group; j++) {
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
  
  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&granks[i], sizeof(int), 1, out_files);
    printf("%d\n", granks[i]);
  }

  fclose(binout);
  fclose(out_files);
}
