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
    printf("Correct usage: %s <global_file_name> <total ranks> <skip after> <#skip>\n",
        argv[0]);
    exit(1);
  }

  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", 0);
  out_files = fopen(dFILE, "wb");

  int numAllocCores = atoi(argv[2]);
  int skip_after = atoi(argv[3]);
  int skip = atoi(argv[4]);
  int coresperjob = 0; 
  int count = 0;
  
  for(int i = 0; i < numAllocCores;) {
    fwrite(&i, sizeof(int), 1, binout);
    fwrite(&coresperjob, sizeof(int), 1, binout);
    fwrite(&jobid, sizeof(int), 1, binout);
    fwrite(&i, sizeof(int), 1, out_files);
    //printf("%d %d %d\n", i, coresperjob, jobid);
    coresperjob++;
    count++;
    if(count == skip_after) {
      i += skip;
      count = 0;
    }
    i++;
  }

  fclose(binout);
  fclose(out_files);
}
