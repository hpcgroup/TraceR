#include "qos-manager.h"

#include <fstream>
#include <stdio.h>

using namespace std;

bool QoSManager::readQoSFileForJob(int job, const char filename[]) {
  ifstream qosFile(filename);
  if (!qosFile) {
    fprintf(stderr, "Cannot read QoS file %s\n", filename);
    return false;
  }
  ServiceLevel defaultSL;
  ServiceLevel defaultColl;

  qosFile >> defaultSL >> defaultColl;
  if(qosFile.fail()) {
    fprintf(stderr, "Error reading default service level from QoS file %s\n", filename);
    return false;
  }
  jobs[job].defaultSL = defaultSL;
  jobs[job].defaultColl = defaultColl;

  Rank rank;
  ServiceLevel sl_p2p, sl_coll;
  while(qosFile >> rank >> sl_p2p >> sl_coll) {
    jobs[job].serviceLevels[rank] = sl_p2p;
    jobs[job].serviceLevels_coll[rank] = sl_coll;
  }
  return true;
}

QoSManager::ServiceLevel QoSManager::getServiceLevel(Job j, Rank s, Rank d) {
  JobQoSMap::iterator job = jobs.find(j);
  if(job != jobs.end()) {
    ServiceLevelMap::iterator src = job->second.serviceLevels.find(s);
    if (src != job->second.serviceLevels.end()) {
      return src->second;
    }
    ServiceLevelMap::iterator dest = job->second.serviceLevels.find(d);
    if (dest != job->second.serviceLevels.end()) {
      return dest->second;
    }
    else {
      return job->second.defaultSL;
    }
  }
  else {
    return overallDefaultSL;
  }
}

QoSManager::ServiceLevel QoSManager::getServiceLevel_coll(Job j, Rank s, Rank d) {
  JobQoSMap::iterator job = jobs.find(j);
  if(job != jobs.end()) {
    ServiceLevelMap::iterator src = job->second.serviceLevels_coll.find(s);
    if (src != job->second.serviceLevels_coll.end()) {
      return src->second;
    }
    ServiceLevelMap::iterator dest = job->second.serviceLevels_coll.find(d);
    if (dest != job->second.serviceLevels_coll.end()) {
      return dest->second;
    }
    else {
      return job->second.defaultSL;
    }
  }
  else {
    return overallDefaultSL;
  }
}