//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// Written by:
//     Nikhil Jain <nikhil.jain@acm.org>
//     Bilge Acun <acun2@illinois.edu>
//     Abhinav Bhatele <bhatele@llnl.gov>
//
// LLNL-CODE-740483. All rights reserved.
//
// This file is part of TraceR. For details, see:
// https://github.com/LLNL/TraceR
// Please also read the LICENSE file for the MIT License notice.
//////////////////////////////////////////////////////////////////////////////

#if TRACER_OTF_TRACES
#include "otf2_reader.h"
#include "CWrapper.h"
#include <cassert>
#define VERBOSE_L1 1
#define VERBOSE_L2 0
#define VERBOSE_L3 0

extern JobInf *jobs;
extern tw_stime soft_delay_mpi;

static OTF2_CallbackCode
callbackDefLocations(void*                 userData,
                     OTF2_LocationRef      location,
                     OTF2_StringRef        name,
                     OTF2_LocationType     locationType,
                     uint64_t              numberOfEvents,
                     OTF2_LocationGroupRef locationGroup )
{
  std::vector<uint64_t>& locations = ((AllData*)userData)->locations;
  locations.push_back(location);
  return OTF2_CALLBACK_SUCCESS;
}


static OTF2_CallbackCode 
callbackDefClockProperties(void * userData,
                          uint64_t timerResolution,
                          uint64_t globalOffset,
                          uint64_t traceLength)
{
  ClockProperties &clockProperties = ((AllData*)userData)->clockProperties;
  clockProperties.ticks_per_second = timerResolution;
  clockProperties.ticksToSecond = TIME_MULT * 1.0/timerResolution;
  if(!g_tw_mynode) 
    printf("Clock Props: %lu %d %f\n", clockProperties.ticks_per_second,
      TIME_MULT, clockProperties.ticksToSecond);
  fflush(stdout);
  clockProperties.time_offset = globalOffset;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackDefString(void * userData,
                  OTF2_StringRef self,
                  const char * s)
{
  ((AllData*)userData)->strings[self] = std::string(s);
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackDefGroup(void* userData,
                OTF2_GroupRef self,
                OTF2_StringRef name,
                OTF2_GroupType groupType,
                OTF2_Paradigm paradigm,
                OTF2_GroupFlag groupFlags,
                uint32_t numberOfMembers,
                const uint64_t* members)
{
  Group g;
  ((AllData*)userData)->groups[self] = g;
  Group &new_g = ((AllData*)userData)->groups[self];
  new_g.type = groupType;
  //OTF2_GROUP_TYPE_COMM_SELF is special
#if VERBOSE_L3
  printf("Add group %llu with %lu members: ", self, numberOfMembers);
  fflush(stdout);
#endif
  for (uint64_t i = 0; i < numberOfMembers; i++) {
    new_g.members.push_back(members[i]); 
    new_g.rmembers[members[i]] = i;
#if VERBOSE_L3
    printf("%llu ", members[i]);
    fflush(stdout);
#endif
  }
#if VERBOSE_L3
    printf("\n");
    fflush(stdout);
#endif
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackDefComm(void * userData,
                OTF2_CommRef self,
                OTF2_StringRef name,
                OTF2_GroupRef group,
                OTF2_CommRef parent)
{
  ((AllData*)userData)->communicators[self] = group;
#if VERBOSE_L3
  printf("Add communication %llu with group %llu\n", self, group);
  fflush(stdout);
#endif
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackDefRegion(void * userData,
                  OTF2_RegionRef self,
                  OTF2_StringRef name,
                  OTF2_StringRef canonicalName,
                  OTF2_StringRef description,
                  OTF2_RegionRole regionRole,
                  OTF2_Paradigm paradigm,
                  OTF2_RegionFlag regionFlag,
                  OTF2_StringRef sourceFile,
                  uint32_t beginLineNumber,
                  uint32_t endLineNumber)
{
  Region r;
  ((AllData*)userData)->regions[self] = r;
  Region& new_r = ((AllData*)userData)->regions[self];
  new_r.name = name;
  new_r.role = regionRole;
  new_r.paradigm = paradigm;
  if(strncmp(((AllData*)userData)->strings[name].c_str(), "TRACER_WallTime", 15) == 0) {
    new_r.isTracerPrintEvt = true;
  } else {
    new_r.isTracerPrintEvt = false;
  }
  if(strncmp(((AllData*)userData)->strings[name].c_str(), "TRACER_Loop", 11) == 0) {
    new_r.isLoopEvt = true;
  } else {
    new_r.isLoopEvt = false;
  }
  if(regionRole == OTF2_REGION_ROLE_BARRIER ||
     regionRole == OTF2_REGION_ROLE_IMPLICIT_BARRIER ||
     regionRole == OTF2_REGION_ROLE_COLL_ONE2ALL ||
     regionRole == OTF2_REGION_ROLE_COLL_ALL2ONE ||
     regionRole == OTF2_REGION_ROLE_COLL_ALL2ALL ||
     regionRole == OTF2_REGION_ROLE_COLL_OTHER ||
     regionRole == OTF2_REGION_ROLE_POINT2POINT) {
    new_r.isCommunication = true;
  }
#if VERBOSE_L3
  printf("Add region %llu name %s role %d paradigm %d\n", self, 
    ((AllData*)userData)->strings[name], regionRole, paradigm);
  fflush(stdout);
#endif
  return OTF2_CALLBACK_SUCCESS;
}

static void 
addUserEvt(void*               userData,
           OTF2_TimeStamp      time)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
  AllData *globalData = (AllData *)userData;
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = (time - ld->lastLogTime) * globalData->clockProperties.ticksToSecond;
  new_task.event_id = TRACER_USER_EVT;
}

// Function only used if NO_COMM_BUILD is defined
#if NO_COMM_BUILD
static void 
addEmptyUserEvt(void* userData)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = 0;
  new_task.event_id = TRACER_USER_EVT;
}
#endif

static OTF2_CallbackCode
callbackEvtBegin( OTF2_LocationRef    location,
                  OTF2_TimeStamp      time,
                  uint64_t            evtPos,
                  void*               userData,
                  OTF2_AttributeList* attributes,
                  OTF2_RegionRef      region )
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
  if(!ld->firstEnter) {
    addUserEvt(userData, time);
  } else {
    ld->firstEnter = false;
  }
  AllData *globalData = (AllData *)userData;
  if(globalData->regions[region].isTracerPrintEvt) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = region;
    new_task.beginEvent = true;
  }
  if(globalData->regions[region].isLoopEvt) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.loopStartEvent = true;
    new_task.event_id = TRACER_LOOP_EVT;
  }
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode
callbackEvtEnd( OTF2_LocationRef    location,
                OTF2_TimeStamp      time,
                uint64_t            evtPos,
                void*               userData,
                OTF2_AttributeList* attributes,
                OTF2_RegionRef      region )
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
  AllData *globalData = (AllData *)userData;
  if(globalData->regions[region].isTracerPrintEvt) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = region;
  }
  if(globalData->regions[region].isLoopEvt) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.loopEvent = true;
    new_task.event_id = TRACER_LOOP_EVT;
  }
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackSendEvt(OTF2_LocationRef locationID,
                OTF2_TimeStamp time,
                uint64_t evtPos,
                void * userData,
                OTF2_AttributeList * attributeList,
                uint32_t receiver,
                OTF2_CommRef communicator,
                uint32_t msgTag,
                uint64_t msgLength)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  AllData *globalData = (AllData *)userData;
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = soft_delay_mpi;
  new_task.event_id = TRACER_SEND_EVT;
  Group& group = globalData->groups[globalData->communicators[communicator]];
  new_task.myEntry.msgId.pe = locationID;
  new_task.myEntry.msgId.id = msgTag;
  new_task.myEntry.msgId.size = msgLength;
  new_task.myEntry.msgId.comm = communicator;
  new_task.myEntry.msgId.coll_type = -1;
  new_task.myEntry.node = group.members[receiver];
  new_task.myEntry.thread = 0;
  new_task.isNonBlocking = false;
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackIsendEvt(OTF2_LocationRef locationID,
                 OTF2_TimeStamp time,
                 uint64_t evtPos,
                 void * userData,
                 OTF2_AttributeList * attributeList,
                 uint32_t receiver,
                 OTF2_CommRef communicator,
                 uint32_t msgTag,
                 uint64_t msgLength,
                 uint64_t requestID)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  AllData *globalData = (AllData *)userData;
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = soft_delay_mpi;
  new_task.event_id = TRACER_SEND_EVT;
  Group& group = globalData->groups[globalData->communicators[communicator]];
  new_task.myEntry.msgId.pe = locationID;
  new_task.myEntry.msgId.id = msgTag;
  new_task.myEntry.msgId.size = msgLength;
  new_task.myEntry.msgId.comm = communicator;
  new_task.myEntry.msgId.coll_type = -1;
  new_task.myEntry.node = group.members[receiver];
  new_task.myEntry.thread = 0;
  new_task.isNonBlocking = true;
  new_task.req_id = requestID;
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackIsendCompEvt(OTF2_LocationRef locationID,
                    OTF2_TimeStamp time,
                    uint64_t evtPos,
                    void * userData,
                    OTF2_AttributeList * attributeList,
                    uint64_t requestID)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = soft_delay_mpi;
  new_task.event_id = TRACER_SEND_COMP_EVT;
  new_task.req_id = requestID;
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackRecvEvt(OTF2_LocationRef locationID,
                OTF2_TimeStamp time,
                uint64_t evtPos,
                void * userData,
                OTF2_AttributeList * attributeList,
                uint32_t sender,
                OTF2_CommRef communicator,
                uint32_t msgTag,
                uint64_t msgLength)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  AllData *globalData = (AllData *)userData;
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = soft_delay_mpi;
  new_task.event_id = TRACER_RECV_EVT;
  Group& group = globalData->groups[globalData->communicators[communicator]];
  new_task.myEntry.msgId.pe = locationID;
  new_task.myEntry.msgId.id = msgTag;
  new_task.myEntry.msgId.size = msgLength;
  new_task.myEntry.msgId.comm = communicator;
  new_task.myEntry.msgId.coll_type = -1;
  new_task.myEntry.node = group.members[sender];
  new_task.myEntry.thread = 0;
  new_task.isNonBlocking = false;
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}


static OTF2_CallbackCode 
callbackIrecv(OTF2_LocationRef locationID,
                 OTF2_TimeStamp time,
                 uint64_t evtPos,
                 void * userData,
                 OTF2_AttributeList * attributeList,
                 uint64_t requestID)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = soft_delay_mpi;
  new_task.event_id = TRACER_USER_EVT;
  new_task.req_id = requestID;
  new_task.isNonBlocking = true;;
  ((AllData *)userData)->matchRecvIds[requestID] = ld->tasks.size() - 1;
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackIrecvCompEvt(OTF2_LocationRef locationID,
                 OTF2_TimeStamp time,
                 uint64_t evtPos,
                 void * userData,
                 OTF2_AttributeList * attributeList,
                 uint32_t sender,
                 OTF2_CommRef communicator,
                 uint32_t msgTag,
                 uint64_t msgLength,
                 uint64_t requestID)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  AllData *globalData = (AllData *)userData;
  ld->tasks.push_back(Task());
  Task &new_task = ld->tasks[ld->tasks.size() - 1];
  new_task.execTime = soft_delay_mpi;
  new_task.event_id = TRACER_RECV_COMP_EVT;
  Group& group = globalData->groups[globalData->communicators[communicator]];
  new_task.myEntry.msgId.pe = locationID;
  new_task.myEntry.msgId.id = msgTag;
  new_task.myEntry.msgId.size = msgLength;
  new_task.myEntry.msgId.comm = communicator;
  new_task.myEntry.msgId.coll_type = -1;
  new_task.myEntry.node = group.members[sender];
  new_task.myEntry.thread = 0;
  new_task.isNonBlocking = false;
  new_task.req_id = requestID;

  std::map<int, int>::iterator it = ((AllData *)userData)->matchRecvIds.find(requestID);
  assert(it != ((AllData *)userData)->matchRecvIds.end());
  Task &postTask = ld->tasks[it->second];
  postTask.event_id = TRACER_RECV_POST_EVT;
  postTask.myEntry.msgId.pe = locationID;
  postTask.myEntry.msgId.id = msgTag;
  postTask.myEntry.msgId.size = msgLength;
  postTask.myEntry.msgId.comm = communicator;
  postTask.myEntry.msgId.coll_type = -1;
  postTask.myEntry.node = new_task.myEntry.node;
  ((AllData *)userData)->matchRecvIds.erase(it);
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackCollectiveBegin(OTF2_LocationRef locationID,
                        OTF2_TimeStamp time,
                        uint64_t evtPos,
                        void * userData,
                        OTF2_AttributeList * attributeList)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackMeasurement(OTF2_LocationRef locationID,
                        OTF2_TimeStamp time,
                        uint64_t evtPos,
                        void * userData,
                        OTF2_AttributeList * attributeList,
                        OTF2_MeasurementMode mode)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}

static OTF2_CallbackCode 
callbackCollectiveEnd(OTF2_LocationRef locationID,
                         OTF2_TimeStamp time,
                         uint64_t evtPos,
                         void * userData,
                         OTF2_AttributeList * attributeList,
                         OTF2_CollectiveOp collectiveOp,
                         OTF2_CommRef communicator,
                         uint32_t root,
                         uint64_t sizeSent,
                         uint64_t sizeReceived)
{
  LocationData* ld = (LocationData*)(((AllData *)userData)->ld);
#if NO_COMM_BUILD
  addEmptyUserEvt(userData);
#else
  AllData *globalData = (AllData *)userData;
  Group& group = globalData->groups[globalData->communicators[communicator]];

  // Return without processing for MPI_COMM_SELF groups
  if( OTF2_GROUP_TYPE_COMM_SELF == group.type ) {
      ld->lastLogTime = time;
      return OTF2_CALLBACK_SUCCESS;
  }

  if(collectiveOp == OTF2_COLLECTIVE_OP_BCAST) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.pe = group.members[root];
    new_task.myEntry.msgId.size = sizeReceived;
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = collectiveOp;
    new_task.myEntry.node = root;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_REDUCE) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.pe = group.members[root];
    new_task.myEntry.msgId.size = sizeSent;
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = collectiveOp;
    new_task.myEntry.node = root;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_ALLTOALL) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.size = sizeSent/group.members.size();
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = OTF2_COLLECTIVE_OP_ALLTOALL;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_ALLTOALLV) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.size = sizeSent/group.members.size();
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = OTF2_COLLECTIVE_OP_ALLTOALLV;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_ALLREDUCE) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.pe = group.members[0];
    new_task.myEntry.msgId.size = sizeSent/group.members.size();
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = collectiveOp;
    new_task.myEntry.node = 0;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_BARRIER) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.pe = group.members[0];
    new_task.myEntry.msgId.size = 0;
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = OTF2_COLLECTIVE_OP_ALLREDUCE;
    new_task.myEntry.node = 0;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_ALLGATHER ||
            collectiveOp == OTF2_COLLECTIVE_OP_ALLGATHERV) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.size = sizeReceived/group.members.size();
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = OTF2_COLLECTIVE_OP_ALLGATHER;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } else if(collectiveOp == OTF2_COLLECTIVE_OP_SCATTER) {
    ld->tasks.push_back(Task());
    Task &new_task = ld->tasks[ld->tasks.size() - 1];
    new_task.execTime = 0;
    new_task.event_id = TRACER_COLL_EVT;
    new_task.myEntry.msgId.pe = group.members[root];
    new_task.myEntry.msgId.size = sizeReceived;
    new_task.myEntry.msgId.comm = communicator;
    new_task.myEntry.msgId.coll_type = OTF2_COLLECTIVE_OP_SCATTER;
    new_task.myEntry.node = root;
    new_task.myEntry.thread = 0;
    new_task.isNonBlocking = false;
  } 
#endif
  ld->lastLogTime = time;
  return OTF2_CALLBACK_SUCCESS;
}


OTF2_Reader * readGlobalDefinitions(int jobID, char* tracefileName, AllData *allData)
{
  int size, rank;
  MPI_Comm_size( MPI_COMM_WORLD, &size );
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );

  OTF2_Reader* reader = OTF2_Reader_Open(tracefileName);
  OTF2_MPI_Reader_SetCollectiveCallbacks( reader, MPI_COMM_WORLD );
  uint64_t number_of_locations;
  OTF2_Reader_GetNumberOfLocations( reader, &number_of_locations );
  OTF2_GlobalDefReader* global_def_reader = OTF2_Reader_GetGlobalDefReader( reader );
  OTF2_GlobalDefReaderCallbacks* global_def_callbacks = OTF2_GlobalDefReaderCallbacks_New();
  OTF2_GlobalDefReaderCallbacks_SetStringCallback(global_def_callbacks,
      &callbackDefString);
  OTF2_GlobalDefReaderCallbacks_SetLocationCallback( global_def_callbacks,
      &callbackDefLocations );
  OTF2_GlobalDefReaderCallbacks_SetClockPropertiesCallback(global_def_callbacks,
      &callbackDefClockProperties);
  OTF2_GlobalDefReaderCallbacks_SetGroupCallback(global_def_callbacks,
      &callbackDefGroup);
  OTF2_GlobalDefReaderCallbacks_SetCommCallback(global_def_callbacks,
      &callbackDefComm);    
  OTF2_GlobalDefReaderCallbacks_SetRegionCallback(global_def_callbacks, 
      &callbackDefRegion);
  //Unused
  //OTF2_GlobalDefReaderCallbacks_SetAttributeCallback(global_def_callbacks,
  //callbackDefAttribute);
  OTF2_Reader_RegisterGlobalDefCallbacks( reader,
      global_def_reader,
      global_def_callbacks,
      allData );
  OTF2_GlobalDefReaderCallbacks_Delete( global_def_callbacks );

  uint64_t definitions_read = 0;
  OTF2_Reader_ReadAllGlobalDefinitions( reader,
      global_def_reader,
      &definitions_read );

  std::vector<uint64_t>& locations = allData->locations;
  assert(number_of_locations == locations.size());
  int count_local_loc = 0;
  for ( size_t i = 0; i < locations.size(); i++ )
  {
    if(isPEonThisRank(jobID, i)) {
      OTF2_Reader_SelectLocation( reader, locations[ i ] );
      count_local_loc++;
    }
  }

#if VERBOSE_L3
  printf("Processor %d files %d\n", rank, count_local_loc);
#endif

  jobs[jobID].localDefs = 
    OTF2_Reader_OpenDefFiles( reader ) == OTF2_SUCCESS;
  
  OTF2_Reader_OpenEvtFiles( reader );
  return reader;
}

void readLocationTasks(int jobID, OTF2_Reader *reader, AllData *allData, 
    uint32_t loc, LocationData* ld)
{
  std::vector<uint64_t>& locations = allData->locations;
  if(jobs[jobID].localDefs) {
    OTF2_DefReader* def_reader =
      OTF2_Reader_GetDefReader( reader, locations[ loc ] );
    if ( def_reader )
    {
      uint64_t def_reads = 0;
      OTF2_Reader_ReadAllLocalDefinitions( reader,
          def_reader,
          &def_reads );
      OTF2_Reader_CloseDefReader( reader, def_reader );
    }
  }
  OTF2_EvtReader* evt_reader =
    OTF2_Reader_GetEvtReader( reader, locations[ loc ] );
  OTF2_EvtReaderCallbacks* event_callbacks = OTF2_EvtReaderCallbacks_New();
  OTF2_EvtReaderCallbacks_SetEnterCallback( event_callbacks,
      &callbackEvtBegin);
  OTF2_EvtReaderCallbacks_SetLeaveCallback( event_callbacks,
      &callbackEvtEnd );
  OTF2_EvtReaderCallbacks_SetMpiSendCallback( event_callbacks,
      &callbackSendEvt );
  OTF2_EvtReaderCallbacks_SetMpiIsendCallback( event_callbacks,
      &callbackIsendEvt );
  OTF2_EvtReaderCallbacks_SetMpiIsendCompleteCallback( event_callbacks,
      &callbackIsendCompEvt);
  OTF2_EvtReaderCallbacks_SetMpiRecvCallback( event_callbacks,
      &callbackRecvEvt);
  OTF2_EvtReaderCallbacks_SetMpiIrecvRequestCallback( event_callbacks,
      &callbackIrecv);
  OTF2_EvtReaderCallbacks_SetMpiIrecvCallback( event_callbacks,
      &callbackIrecvCompEvt);
  OTF2_EvtReaderCallbacks_SetMpiCollectiveBeginCallback(
      event_callbacks, &callbackCollectiveBegin);
  OTF2_EvtReaderCallbacks_SetMpiCollectiveEndCallback(
      event_callbacks, &callbackCollectiveEnd);
  OTF2_EvtReaderCallbacks_SetMeasurementOnOffCallback(
      event_callbacks, &callbackMeasurement);
  
  allData->ld = ld;
  ld->lastLogTime = 0;
  ld->firstEnter = true;
  OTF2_Reader_RegisterEvtCallbacks( reader,
      evt_reader,
      event_callbacks,
      allData );
  OTF2_EvtReaderCallbacks_Delete( event_callbacks );
  uint64_t events_read = 0;
  OTF2_Reader_ReadAllLocalEvents( reader,
      evt_reader,
      &events_read );
#if VERBOSE_L3
  for(int e = 0; e < ld->tasks.size(); e++) {
    printf("[%d] type: %d, exec time %f\n", e, 
        ld->tasks[e].event_id, ld->tasks[e].execTime);
  }
#endif
  OTF2_Reader_CloseEvtReader( reader, evt_reader );
}

void closeReader(OTF2_Reader *reader) {
  OTF2_Reader_CloseDefFiles( reader );
  OTF2_Reader_CloseEvtFiles( reader );
  OTF2_Reader_Close( reader );
}
#endif
