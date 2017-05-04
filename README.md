TraceR v2
===========

TraceR is a trace replay tool built upon the ROSS-based CODES simulation
framework. TraceR can be used for predicting network performance and
understanding network behavior by simulating messaging in High Performance
Computing applications on interconnection networks.

### Build

* Download and install ROSS:
```
git clone https://github.com/carothersc/ROSS
```
Latest verified commit: de26e581a312fd122a4c06917bf0f3d21f38a24b

* Download and install CODES:
```
Website: https://xgitlab.cels.anl.gov/codes/codes
git clone https://xgitlab.cels.anl.gov/codes/codes.git
```
Latest verfied commit (from master): 8f56c9608eb07922971242d76bae88ad0c7aa789

Pending pull request (with new features): https://xgitlab.cels.anl.gov/codes/codes/merge_requests/21

* Trace format choice (TraceR v2 only works with OTF2 right now, so chose 2 here): 

1) AMPI-based BigSim format: download and build Charm++ with bgampi target:
```
git clone http://charm.cs.uiuc.edu/gerrit/charm
```
Follow instructions in the [Charm++ manual](http://charm.cs.illinois.edu/manuals/html/charm++/A.html). 

Use "charm++" as target for compiling TraceR.
Use "bgampi" as target for buidling AMPI used for collecting traces.
In both of cases above, pass "bigemulator" as a build option.

2) OTF2: download and build scoreP for OTF2 support:

Refer to README.OTF2 file in this directory.

* Set the appropriate paths: ROSS, BASE_DIR/CODES in tracer/Makefile.common.
Also set compilers and flags - CC, CXX, SEQ_CXX, CFLAGS, CXXFLAGS, LDFLAGS. 

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
docs/UserWriteUp.txt and in CODES:codes/src/networks/model-net/doc

Example files for BigSim are in jacobi2d, while for OTF2 are in  Sample run command:
```
mpirun -np 8 ../traceR --sync=3  --nkp=16  -- ../conf/tracer-torus.conf tracer_config
```

Please refer to README.OTF for instructions on generating OTF2-MPI trace files.
BigSim-AMPI trace file generation instructions are available at
http://charm.cs.illinois.edu/manuals/html/bigsim/manual-1p.html.

### Reference

Any published work that utilizes this software should include the following
reference:

```
Nikhil Jain, Abhinav Bhatele, Sam White, Todd Gamblin, and Laxmikant Kale.
Evaluating HPC Networks via Simulation of Parallel Workloads. SC 2016.

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
LLNL-CODE-681378. All rights reserved.

This file is part of TraceR. For details, see:
https://github.com/LLNL/tracer.
Please also read the LICENSE file for our notice and the LGPL.
