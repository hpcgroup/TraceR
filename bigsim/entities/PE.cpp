#include "PE.h"
#include <math.h>
#define MAX_LOGS 5000000

PE::PE() {
  busy = false;
  currentTask = 0;
  windowOffset = 0;
  beforeTask = 0;
}

long long PE::startExec(unsigned long long startTime)
{
  unsigned long long tmpExecTime = startTime;

  if(tmpExecTime < currTime) {
    tmpExecTime = currTime;
  } else {
    currTime = tmpExecTime;
  }
/*
  // Warning: assuming all backward dependencies are done when it is the task`s turn
  // no backward dependency on tasks ahead in list
  // while no message dependency or message arrived
  while(currentTask + windowOffset < totalTasksCount && myTasks[currentTask].myMsgId.pe < 0)
  {
    if(!myTasks[currentTask].done)
    {
      // time passed to send messages on time
      printf("[%d-%d] Execute at %llu\n",myNum, currentTask, tmpExecTime);
      tmpExecTime += myTasks[currentTask].exec(tmpExecTime, myNum);
      DEBUGPRINTF3("[%d-%d] Done\n",myNum, currentTask);
      myTasks[currentTask].done = true;
    }
    currentTask++;
  }

  int tasksExecuted = currentTask - beforeTask;
  for(int i=0; i<tasksExecuted; i++)
  {
    for(int j=0; j<myTasks[beforeTask+i].forwDepSize; j++)
    {
      int forwTaskInd = myTasks[beforeTask+i].forwardDep[j] - windowOffset;
      bringIntoMemory(forwTaskInd);
      if(!myTasks[forwTaskInd].done && noUnsatDep(forwTaskInd))
      {
        tmpExecTime = startRecursiveExec(forwTaskInd + windowOffset, tmpExecTime);
      }
    }
  }
*/
  beforeTask = currentTask;
  return tmpExecTime;
}

bool PE::noUnsatDep(int tInd)
{
  for(int i=0; i<myTasks[tInd].backwDepSize; i++)
  {
    int bwInd = myTasks[tInd].backwardDep[i] - windowOffset;
    if(bwInd >= currentTask && !myTasks[bwInd].done)
      return false;
  }
  return true;
}

long long PE::startRecursiveExec(int tInd, unsigned long long beforeExecutedTime)
{
  printf("[%d-%d] Begin Recursive Execute at %llu\n",myNum, tInd - windowOffset, beforeExecutedTime);
  if(myTasks[tInd - windowOffset].myMsgId.pe > 0) {
    return beforeExecutedTime;
  }

  long long tmpExecTime = beforeExecutedTime;

  if(tmpExecTime < currTime) {
    tmpExecTime = currTime;
  } else {
    currTime = tmpExecTime;
  }
/*
  if(Engine::engine.time < currTime)
    Engine::engine.time = currTime;

  DEBUGPRINTF2("[%d-%d] Recursive Execute at %llu\n",myNum, tInd - windowOffset, tmpExecTime);
  tmpExecTime += myTasks[tInd-windowOffset].exec(tmpExecTime, myNum);
  DEBUGPRINTF3("[%d-%d] Done Recursive\n",myNum, tInd-windowOffset);
  myTasks[tInd-windowOffset].done = true;

  for(int i=0; i<myTasks[tInd-windowOffset].forwDepSize; i++)
  {
    int forwTaskInd = myTasks[tInd-windowOffset].forwardDep[i] - windowOffset;
    bringIntoMemory(forwTaskInd);
    if(!myTasks[forwTaskInd].done && noUnsatDep(forwTaskInd) )
    {
      tmpExecTime = startRecursiveExec(forwTaskInd+windowOffset,tmpExecTime);
    }
  }
*/
  return tmpExecTime;
}

void PE::receiveMsg(MsgID msgId, unsigned long long recvtime)
{
/*
  int sourceEmPe = Engine::engine.pes[msgId.pe].myEmPE;
  int index = Engine::engine.msgDestTask[sourceEmPe][msgId.id];
  unsigned long long newTime;

  DEBUGPRINTF("[%d] Recv message of size %d at %llu, vtime %llu\n",myNum, msgId.size, recvtime,currTime);
  if(index >= 0)
  {
    DEBUGPRINTF2("[%d] Found message in hash table look up at %d, %d Status %d\n",myNum, index, windowOffset, myTasks[index-windowOffset].done);
    if(myTasks[index-windowOffset].done)
      assert(0);
    assert(myTasks[index-windowOffset].myMsgId.pe > 0);
    myTasks[index-windowOffset].myMsgId.pe = -myTasks[index-windowOffset].myMsgId.pe;
    if(!busy)
    {
      if(index == currentTask + windowOffset) {
        newTime = startExec(recvtime);
      } else {
        DEBUGPRINTF2("[%d] start startRecursiveExec at %llu, vtime %llu\n",myNum,recvtime,currTime);
        newTime = startRecursiveExec(index,recvtime);
      }

      if(newTime != recvtime)
      {
        Event* ev = new ExecCompleteEvent(newTime, myNum);
        Engine::engine.addEvent(ev);
        busy=true;
      }
    } else {
      DEBUGPRINTF2("[%d] Processor is busy, buffer the message \n",myNum);
      msgBuffer.push_back(index);
    }
    return;
  }
  DEBUGPRINTF2("[%d] Going beyond hash look up on receiving a message\n", myNum);

  bool gotMsg=false;
  // if first not-executed needs this and not busy start sequential
  if((myTasks[currentTask].myMsgId.pe - 1) == msgId.pe && myTasks[currentTask].myMsgId.id == msgId.id)
  {
    gotMsg=true;
    if(myTasks[currentTask].done)
      assert(0);
    myTasks[currentTask].myMsgId.pe=-myTasks[currentTask].myMsgId.pe;
    if(!busy)
    {
      newTime = startExec(recvtime);
      if(newTime != recvtime)
      {
        Event* ev=new ExecCompleteEvent(newTime, myNum);
        Engine::engine.addEvent(ev);
        busy=true;
      }
    } else {
      msgBuffer.push_back(currentTask+windowOffset);
    }
    return;
  } else {
    // search in following tasks
    DEBUGPRINTF2("[%d] Going beyond the current task while searching for matching task\n",myNum);
    int i;
    for(i = currentTask + 1; i < tasksCount; i++) {
      // if task depends on this message
      if((myTasks[i].myMsgId.pe - 1) == msgId.pe && myTasks[i].myMsgId.id == msgId.id)
      {
        gotMsg=true;
        if(myTasks[i].done)
          assert(0);
        myTasks[i].myMsgId.pe = -myTasks[i].myMsgId.pe;
        if(!busy)
        {
          newTime = startRecursiveExec(i+windowOffset,recvtime); //didn't execute anything yet
          if(newTime != recvtime)
          {
            Event* ev = new ExecCompleteEvent(newTime, myNum);
            Engine::engine.addEvent(ev);
            busy=true;
          }
        } else {
          msgBuffer.push_back(i+windowOffset);
        }
        return;
      }
    }
    printf("Could not find the task; this is all wrong..Aborting\n");
    assert(0);
  }
*/
}

void PE::completeExec(unsigned long long completetime)
{
  busy = false;
  unsigned long long startTime = completetime, newTime;

  printf("[%d] Complete Exec at %llu\n",myNum, completetime);
  if(startTime < currTime) {
    startTime = currTime;
  } else {
    currTime = startTime;
  }

/*
  if(Engine::engine.time < currTime)
    Engine::engine.time = currTime;

  newTime = startTime;
  for(int i = 0; i < msgBuffer.size(); i++)
  {
    // convert global index to local
    int taskInd = msgBuffer[i] - windowOffset;
    bringIntoMemory(taskInd);
    if (taskInd >= currentTask && !myTasks[taskInd].done) {
      if(taskInd == currentTask)
      {
        newTime = startExec(newTime);
      } else {
        newTime = startRecursiveExec(taskInd + windowOffset, newTime);
      }
    }
  }
  msgBuffer.clear();
  if(newTime != startTime)
  {
    Event* ev=new ExecCompleteEvent(newTime, myNum);
    Engine::engine.addEvent(ev);
    busy=true;
  }
  */
}

// no stats yet?
void PE::printStat()
{
  int countTask=0;
  for(int i=0; i<tasksCount; i++)
  {
    // assert(myTasks[i].done);
    if(!myTasks[i].done)
    {
      printf("PE: %d not done:%d\n", myNum, i);
      countTask++;
    }
  }
}

void PE::check()
{
  int countTask=0;
  for(int i=0; i<tasksCount; i++)
  {
    // assert(myTasks[i].done);
    if(!myTasks[i].done)
    {
      countTask++;
    }
  }

  if(countTask != 0) {
    printf("PE:%d not done count:%d ",myNum, countTask);
    int i = 0, count=0;
    while (i < tasksCount) {
      if (!myTasks[i].done) {
        printf(" %d", i);
        i++;
        count = 0;
        while ((i < tasksCount) && (!myTasks[i].done)) {
          i++;
          count++;
        }
        if (count) {
          printf("-%d", i-1);
        }
      }
      i++;
    }
    printf("\n");
  }
  else printf("PE:%d ALL TASKS ARE DONE\n", myNum);

}

void PE::printState()
{
  printf("PE:%d, busy:%d, currentTask:%d totalTasks:%d\n", myNum, busy, currentTask, totalTasksCount);
  printf("msgBuffer: ");
  for(int i=0; i<msgBuffer.size(); i++)
  {
    printf("%d, ",msgBuffer[i]);
  }
  printf("\n tasks from curr: ");
  for(int i=currentTask; i<tasksCount; i++)
  {
    printf("%d, ", myTasks[i].done);
  }
  printf("\n");

}

//BILGE
void PE::invertMsgPe(int tInd){
    myTasks[tInd].myMsgId.pe = -myTasks[tInd].myMsgId.pe;
}
unsigned long long PE::getTaskExecTime(int tInd){
    return myTasks[tInd].execTime;
}
int PE::findTaskFromMsg(MsgID* msgId){
    map<int, int>::iterator it;
    int sPe = msgId->pe;
    int sEmPe = (sPe/numWth)%numEmPes;
    int smsgID = msgId->id;
    it = msgDestLogs[sEmPe].find(smsgID);
    if(it!=msgDestLogs[sEmPe].end())
        return it->second;
    else return -1;
}
