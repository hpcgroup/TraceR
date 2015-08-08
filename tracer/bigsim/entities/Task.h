/*
 * Task.h
 *
 */

#ifndef TASK_H_
#define TASK_H_
#include "MsgEntry.h"
#include <cstdlib>
#include <cstdio>
#include <ross.h>

class MsgEntry;
#include <cstring>
#define TIME_MULT 1000000000

class BgPrint{
  public:
    void print(tw_lp * lp, unsigned long long startTime, int PEno, int jobNo)
    {
      char str[1000];
      strcpy(str, "[%d %d : %s] ");
      strcat(str, msg);
      tw_output(lp, str, jobNo, PEno, taskName, startTime/((double)TIME_MULT)+time);
    }
    char* msg;
    double time;
    char taskName[20];
    void copy(BgPrint *b){
      time = b->time;
      strcpy(taskName, b->taskName);
      msg = new char[strlen(b->msg)+1];
      strcpy(msg, b->msg);
    }

};

// represents each DEP ~ SEB
class Task {
  public:
    Task();
    ~Task();
    void printEvt(tw_lp * lp, unsigned long long startTime, int PEno, int jobNo);
    MsgID myMsgId;
    bool done;
    short charmEP;
    int* forwardDep; //backward dependent tasks
    int forwDepSize;	// size of forwardDep array

    int* backwardDep;	//forward dependent tasks
    int backwDepSize;	// size of backwDep array

    unsigned long long execTime;	//execution time of the task

    int msgEntCount; // number of msg entries
    MsgEntry* myEntries; // outgoing messages of task

    int bgPrintCount;
    BgPrint* myBgPrints;
    void copyFromTask(Task* t);
};

#endif /* TASK_H_ */
