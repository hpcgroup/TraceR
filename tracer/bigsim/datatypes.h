#ifndef _DATATYPES_H_
#define _DATATYPES_H_

typedef struct JobInf {
    int numRanks;
    char traceDir[256];
    char map_file[256];
    int *rankMap;
    int *offsets;
    int skipMsgId;
} JobInf;

#endif

