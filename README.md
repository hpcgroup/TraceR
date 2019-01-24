TraceR v2.2
===========

[![Read the Docs](http://readthedocs.org/projects/tracer-codes/badge/?version=latest)](http://tracer-codes.readthedocs.io)

TraceR is a trace replay tool built upon the ROSS-based CODES simulation
framework. TraceR can be used for predicting network performance and
understanding network behavior by simulating messaging in High Performance
Computing applications on interconnection networks.

### Build

* Download and install ROSS:
```
git clone https://github.com/carothersc/ROSS
```
Latest verified commit: 17e9d7b455be32fc2b085f38d9a5b3cc30c37101

* Download and install CODES:
```
Website: https://xgitlab.cels.anl.gov/codes/codes
git clone https://xgitlab.cels.anl.gov/codes/codes.git
```
Latest verfied commit (from master): f28191b7c11bb0a64b6e803587a00de17fee4784

* Trace format choice (pick one of the following): 

1) AMPI-based BigSim format: download and build Charm++.
```
git clone http://charm.cs.uiuc.edu/gerrit/charm
```
Follow instructions in the [Charm++ manual](http://charm.cs.illinois.edu/manuals/html/charm++/A.html). 

Use "charm++" as target for compiling TraceR.
Use "bgampi" as target for buidling AMPI used for collecting traces.
In both of cases above, pass "bigemulator" as a build option.

2) OTF2: download and build scoreP for OTF2 support. (preferred)

Refer to README.OTF2 file in this directory. Simulation of the following most
commonly used collectives using algorithms used in MPICH is supported in this
trace format: Barrier, Bcast, (All)Reduce, Alltoall(v), and Allgather. In
contrast, for BigSim traces, the simulation depends on AMPI's implementation.

* Set the appropriate paths: ROSS_DIR, CODES_DIR in tracer/Makefile.common.
Also set the compiler - CXX and SEQ_CXX.

If using BigSim format, uncomment SELECT_TRACE = -DTRACER_BIGSIM_TRACES=1,
otherwise SELECT_TRACE = -DTRACER_OTF_TRACES=1 should be left uncommented (one of
two). Accordingly, either set CHARMPATH or ensure that otf2-config (which is 
inside the bin directory of scoreP install) is in your path. Then,
```
cd tracer
make
```

### Run

```
mpirun -np <p> ../traceR --sync=3  -- ../conf/<choose here> <tracer_config>
```

Format of trace_config:
```
<global map file>
<num jobs>
<Trace path for job0> <map file for job0> <number of ranks in job0> <iterations (use 1 if running in normal mode)>
<Trace path for job1> <map file for job1> <number of ranks in job1> <iterations (use 1 if running in normal mode)>
...
```
If "global map file" is not needed, use NA for it and "map file for job*".
For generating  global and job map file, please refer to README inside
utils for the format and sample map generation code.

More information on workflow of TraceR and network config files can be found at
docs/UserWriteUp.txt and https://xgitlab.cels.anl.gov/codes/codes/wikis/home.

Example files for BigSim are in examples/jacobi2d-bigsim, while for OTF2 are in examples/stencil4d-otf. Sample run command:
```
mpirun -np 8 ../../tracer/traceR --sync=3 --nkp=16 --extramem=100000 --max-opt-lookahead=1000000 --timer-frequency=1000 -- ../conf/fattree.conf ./tracer_config
```

Parameters:   
--sync: ROSS's PDES type. 1 - sequential, 2 - conservation, 3 - optimistic  
--extramem: number of messages in ROSS's extra message buffer - each message is ~500 bytes - 100K should work for most cases  
--max-opt-lookahead: leash on optimisitc execution in nanoseconds (1 micro second is a good value)  
--timer-frequency: frequency with which PE0 should print current virtual time  
--nkp : number of groups used for clustering LPs; recommended value for lower rollbacks: (total LPs)/(#MPI ranks) 

Please refer to README.OTF for instructions on generating OTF2-MPI trace files.
BigSim-AMPI trace file generation instructions are available at
http://charm.cs.illinois.edu/manuals/html/bigsim/manual-1p.html.

### Reference

Any published work that utilizes this software should include the following
reference:

```
Nikhil Jain, Abhinav Bhatele, Samuel T. White, Todd Gamblin, and Laxmikant
V. Kale. Evaluating HPC networks via simulation of parallel workloads. In
Proceedings of the ACM/IEEE International Conference for High Performance
Computing, Networking, Storage and Analysis, SC '16. IEEE Computer Society,
November 2016. LLNL-CONF-690662.
```

### Release

Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.

Written by:
```
    Nikhil Jain <nikhil.jain@acm.org>
    Bilge Acun <acun2@illinois.edu>
    Abhinav Bhatele <bhatele@llnl.gov>
```
LLNL-CODE-740483. All rights reserved.

This file is part of TraceR. For details, see:
https://github.com/LLNL/TraceR.
Please also read the LICENSE file for the MIT License notice.

SPDX-License-Identifier: MIT
