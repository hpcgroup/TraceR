/*
 * TraceFileReader.cpp
 *
 *  Created on: Oct 18, 2010
 *  Author: Ehsan Totoni
 */

#include "assert.h"
#include "blue.h"
#include "blue_impl.h"
#include <limits.h>
#include <cstdio>
#define TIME_MULT 1000000000

extern BgTimeLineRec* currTline;
extern int currTlineIdx;
void printLogToFile(BgTimeLog* bglog, FILE* file);

int main(int argc, char** argv)
{
  int numPes;	// number of emulation PEs
  int totalWorkerProcs;
  int numX, numY, numZ;
  int numCth;	//communication PEs per node
  int numWth;	//Working PEs per node
  int* allNodeOffsets;
  int numPesToTranslate;

  BgLoadTraceSummary("bgTrace", totalWorkerProcs, numX, numY, numZ, numCth, 
      numWth, numPes);
  printf("totalWorkers:%d, numX:%d, numY:%d numZ:%d numCth:%d numWth:%d "
      "numPes:%d\n", totalWorkerProcs, numX, numY, numZ, numCth, numWth,numPes);
  allNodeOffsets = BgLoadOffsets(totalWorkerProcs, numPes);

  if(argc == 2) {
    numPesToTranslate = atoi(argv[1]);
  } else {
    numPesToTranslate = totalWorkerProcs;
  }
  for (int i = 0; i < numPesToTranslate; ++i) {
    BgTimeLineRec tlinerec; // Time line (list of logs)
    currTline = &tlinerec;  // set global variable
    currTlineIdx = i;	// set global variable
    int status = BgReadProc(i, numWth , numPes, totalWorkerProcs, 
      allNodeOffsets, tlinerec);
    char name[10];
    sprintf(name,"PE%d", i);
    FILE* file = fopen(name, "w");
    for(int logInd = 0; logInd < tlinerec.length(); logInd++)
    {
      BgTimeLog *bglog = tlinerec[logInd];
      printLogToFile(bglog, file);
    }
    fclose(file);
    printf("PE finished %d\n",i);
  }
  return 0;
}

void printLogToFile(BgTimeLog* bglog, FILE* file)
{
  fprintf(file, "[%d][%s:%f] ", bglog->seqno, bglog->name, bglog->execTime);

  fprintf(file,"$P[%d:%d] ", bglog->msgId.pe(), bglog->msgId.msgID());

  fprintf(file,"$S[");
  for(int i = 0; i < bglog->msgs.length(); i++)
  {
    fprintf(file,"(o %f: ", bglog->msgs[i]->sendTime - bglog->startTime);
    fprintf(file,"n %d ", bglog->msgs[i]->dstNode);
    fprintf(file,"t %d ", bglog->msgs[i]->tID);
    fprintf(file,"b %d ", bglog->msgs[i]->msgsize);
    fprintf(file,"i %d)", bglog->msgs[i]->msgID);
  }
  fprintf(file,"] ");

  fprintf(file,"$B[ ");
  for(int i = 0; i < bglog->backwardDeps.length(); i++)
  {
    fprintf(file,"%d ", bglog->backwardDeps[i]->seqno);
  }
  fprintf(file,"] ");

  fprintf(file,"$F[ ");
  for(int i = 0; i < bglog->forwardDeps.length(); i++){
    fprintf(file,"%d ",bglog->forwardDeps[i]->seqno);
  }
  fprintf(file,"] ");

  fprintf(file,"$E[");
  for(int i = 0; i < bglog->evts.length(); i++){
    fprintf(file,"(%f %s)", bglog->evts[i]->rTime, 
      ((char *)bglog->evts[i]->data));
  }
  fprintf(file,"]\n");
}

