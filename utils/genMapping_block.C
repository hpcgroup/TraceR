#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <vector>

using namespace std;

int main(int argc, char**argv) {
  FILE *binout = fopen(argv[1], "wb");
  int jobid = 0;
  FILE* out_files;

  if(argc < 3) {
    printf("Correct usage: %s <global_file_name> <total ranks> \n",
        argv[0]);
    exit(1);
  }

  char dFILE[256];
  sprintf(dFILE, "%s%d", "job", 0);
  out_files = fopen(dFILE, "wb");

  int numAllocCores = atoi(argv[2]);
  int local_rank = 0;
  int d[6], o[6]; 
  for(o[0] = 0; o[0] < 6; o[0] += 2) {
  for(o[1] = 0; o[1] < 6; o[1] += 2) {
  for(o[2] = 0; o[2] < 6; o[2] += 2) {
  for(o[3] = 0; o[3] < 6; o[3] += 2) {
  for(o[4] = 0; o[4] < 6; o[4] += 2) {
  for(o[5] = 0; o[5] < 6; o[5] += 2) {
  for(d[0] = o[0]; d[0] < o[0] + 2; d[0]++) {
  for(d[1] = o[1]; d[1] < o[1] + 2; d[1]++) {
  for(d[2] = o[2]; d[2] < o[2] + 2; d[2]++) {
  for(d[3] = o[3]; d[3] < o[3] + 2; d[3]++) {
  for(d[4] = o[4]; d[4] < o[4] + 2; d[4]++) {
  for(d[5] = o[5]; d[5] < o[5] + 2; d[5]++) {
    int realNode = d[5] + d[4] * 6 + d[3] * 36 + d[2] * 216 + d[1] * 1296 
      + d[0] * 7776;
    int base_global_rank = realNode * 64;
    for(int j = 0; j < 64; j++) {
      int global_rank = base_global_rank + j;
      fwrite(&global_rank, sizeof(int), 1, binout);
      fwrite(&local_rank, sizeof(int), 1, binout);
      fwrite(&jobid, sizeof(int), 1, binout);
      fwrite(&global_rank, sizeof(int), 1, out_files);
      printf("%d %d %d\n", global_rank, local_rank, jobid);
      local_rank++;
      if(local_rank == numAllocCores) {
        break;
      }
    }
    if(local_rank == numAllocCores) {
      fclose(binout);
      fclose(out_files);
      return 0;
    }
  }}}}}}}}}}}}
}
