#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <vector>

using namespace std;

int main(int argc, char**argv) {
  FILE *binout = fopen(argv[1], "wb");
  int numJobs = argc - 2;
  vector<int> jobSizes;
  vector<FILE*> out_files;
  int numAllocCores = 0;

  if(argc < 3) {
    printf("Correct usage: %s <global_file_name> <ranks in each job>\n",
        argv[0]);
    exit(1);
  }

  jobSizes.resize(numJobs);
  out_files.resize(numJobs);
  for(int i = 0; i < numJobs; i++) { 
    jobSizes[i] = atoi(argv[i+2]);
    numAllocCores += jobSizes[i];
    char dFILE[256];
    sprintf(dFILE, "%s%d", "job", i);
    out_files[i] = fopen(dFILE, "wb");
  }

  int coresperjob = 0;
  int jobid = 0;
  for(int i = 0; i < numAllocCores; i++) {
    fwrite(&i, sizeof(int), 1, binout);
    fwrite(&coresperjob, sizeof(int), 1, binout);
    fwrite(&jobid, sizeof(int), 1, binout);
#if PRINT_MAP
    printf("%d %d %d\n", i, coresperjob, jobid);
#endif
    fwrite(&i, sizeof(int), 1, out_files[jobid]);
    coresperjob++;
    if(coresperjob == jobSizes[jobid]) {
      jobid++;
      coresperjob = 0;
    }
  }

  fclose(binout);
  for(int i = 0; i < numJobs; i++) { 
    fclose(out_files[i]);
  }
}
