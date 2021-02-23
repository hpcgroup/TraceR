#ifndef _QOS_MANAGER_H_
#define _QOS_MANAGER_H_

#include <map>

class QoSManager {
public:
  typedef int Job;
  typedef int Rank;
  typedef int ServiceLevel;

private:
  typedef std::map<Rank, ServiceLevel> ServiceLevelMap;
  struct JobQoS {
    ServiceLevel defaultSL;
    ServiceLevel defaultColl; // defaultSL for p2p, default Coll for collective
    ServiceLevelMap serviceLevels;
    ServiceLevelMap serviceLevels_coll;
  };
  typedef std::map<Job, JobQoS> JobQoSMap;

  ServiceLevel overallDefaultSL;
  JobQoSMap jobs;

public:
  QoSManager(ServiceLevel overallDefaultSL): overallDefaultSL(overallDefaultSL) {}
  void setDefaultServiceLevel(ServiceLevel defaultSL) { overallDefaultSL = defaultSL; }
  bool readQoSFileForJob(Job job, const char filename[]);
  ServiceLevel getServiceLevel(Job job, Rank src, Rank dest);
  ServiceLevel getServiceLevel_coll(Job job, Rank src, Rank dest);
};

#endif