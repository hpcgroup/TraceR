TraceR v1.1
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
Latest verified commit: 8af4b4167859d68a4d838d88d35b41791612a110

* Download and install CODES:
```
Website: https://xgitlab.cels.anl.gov/codes/codes
git clone https://xgitlab.cels.anl.gov/codes/codes.git
```
Latest verfied commit: 2f6fc4ac652b1ef53d8b0df68b1a98ed9bbe7fbf

* Download and build Charm++ with bgampi target:
```
git clone http://charm.cs.uiuc.edu/gerrit/charm
```
Follow instructions in the [Charm++ manual](http://charm.cs.illinois.edu/manuals/html/charm++/A.html). Replace the "charm++" target with bgampi.

* Set the appropriate paths CHARMPATH, ROSS, BASE_DIR/CODES in tracer/Makefile.common and then:
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
<Trace folder for job0> <map file for job0> <number of ranks in job0> <iterations (use 1 if running in normal mode)>
<Trace folder for job1> <map file for job1> <number of ranks in job1> <iterations (use 1 if running in normal mode)>
...
```
If "global map file" is not needed, use NA for it and "map file for job*".
For generating simple global and job map file, use the code in utils.
More information on how to set up network config files can be found in CODES:codes/src/networks/model-net/doc

Example files are in jacobi2d. Sample run command:
```
mpirun -np 8 ../traceR --sync=3  --nkp=16  -- ../conf/tracer-torus.conf tracer_config
```

### Reference

Any published work that utilizes this software should include the following
reference:

```
Bilge Acun, Nikhil Jain, Abhinav Bhatele, Misbah Mubarak, Christopher D.
Carothers, and Laxmikant V. Kale. Preliminary evaluation of a parallel trace
replay tool for HPC network simulations. In Proceedings of the 3rd Workshop on
Parallel and Distributed Agent-Based Simulations, PADABS '15, August 2015.
LLNL-CONF-667225.
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
