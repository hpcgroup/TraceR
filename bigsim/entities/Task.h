/*
 * Task.h
 *
 */

#ifndef TASK_H_
#define TASK_H_
#include "MsgEntry.h"
#include <cstdlib>
#include <cstdio>

class MsgEntry;
#include <cstring>
#define TIME_MULT 1000000000

class BgPrint{
  public:
    void print(unsigned long long startTime, int PEno)
    {
      printf("[%d, %s] ",PEno, taskName);
      int i=0;
      while(msg[i])
      {
        if(msg[i]=='%' && msg[i+1]=='f')
        {
          printf("%f", startTime/((double)TIME_MULT)+time);
          i+=2;
        }
        else
        {
          printf("%c",msg[i]);
          i++;
        }
      }
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
    unsigned long long exec(unsigned long long startTime, int PEno);
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
