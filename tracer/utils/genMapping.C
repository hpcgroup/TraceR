#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <vector>

using namespace std;

int main(int argc, char**argv) {
  int numRanks = atoi(argv[1]);
  FILE *binout = fopen(argv[2], "wb");
  int numJobs = argc - 3;
  vector<int> jobSizes;
  vector<FILE*> out_files;
  int numAllocCores = 0;

  jobSizes.resize(numJobs);
  out_files.resize(numJobs);
  for(int i = 0; i < numJobs; i++) { 
    jobSizes[i] = atoi(argv[i+3]);
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
    printf("%d %d %d\n", i, coresperjob, jobid);
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
