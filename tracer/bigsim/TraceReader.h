/*
 * TraceFileReader.h
 *
 *  Created on: Oct 18, 2010
 *      Author: ehsan
 */

#ifndef TRACEFILEREADER_H_
#define TRACEFILEREADER_H_
#include "assert.h"
#include "blue.h"
#include "blue_impl.h"
#include "entities/PE.h"
#include "entities/Task.h"
#include <string>
#include <iostream>
#include <fstream>
using namespace std;
class PE;
class Node;
class Task;

class TraceReader {
public:
    TraceReader(char *);
    ~TraceReader();
    void loadOffsets();
    void loadTraceSummary();
    void readTrace(int* tot, int* numnodes, int* empes, int* nwth, PE* pe,
        int penum, int jobnum, double* startTime);
    void setTaskFromLog(Task *t, BgTimeLog* bglog, int taskPE, int emPE, int jobPEindex, PE* pe);

    int numEmPes;	// number of emulation PEs, there is a trace file for each of them
    int totalWorkerProcs;
    int totalNodes;
    int numWth;	//Working PEs per node
    int* allNodeOffsets;
    char tracePath[256];

    int fileLoc; // each worker needs separate file offset
    int firstLog; // first log of window to read for each worker
    int totalTlineLength; // apparently totalTlineLength should be kept for each PE!
};

#endif /* TRACEFILEREADER_H_ */
