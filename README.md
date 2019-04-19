TraceR v2.2
===========

[![Read the Docs](http://readthedocs.org/projects/tracer-codes/badge/?version=master)](http://tracer-codes.readthedocs.io)
[![Build Status](https://travis-ci.com/LLNL/TraceR.svg?branch=master)](https://travis-ci.com/LLNL/TraceR)

TraceR is a trace replay tool built upon the ROSS-based CODES simulation
framework. TraceR can be used for predicting network performance and
understanding network behavior by simulating messaging in High Performance
Computing applications on interconnection networks.


### Build

TraceR depends on [CODES](https://xgitlab.cels.anl.gov/codes/codes) and [ROSS](https://github.com/ROSS-org/ROSS). There are several ways to build TraceR:

1. Use [spack](https://github.com/spack/spack) to build TraceR and its dependencies:
```
    spack install tracer
```

2. Build TraceR and its dependencies manually:

* Download and install ROSS and CODES. Set the appropriate paths: ROSS_DIR, and
  CODES_DIR in tracer/Makefile.common.
* Pick between the two trace formats supported by TraceR: OTF2 or BigSim, and
  accordingly build the OTF2 or Charm++ library. If using OTF2 traces
  (default), set SELECT_TRACE = -DTRACER_OTF_TRACES=1, and ensure that
  otf2-config is in your PATH. If using BigSim traces, set SELECT_TRACE =
  -DTRACER_BIGSIM_TRACES=1, and set CHARMPATH to the Charm++ installation in
  tracer/Makefile.common.
* Set the ARCH variable in tracer/Makefile.common or alternatively set the CXX
  and ARCH_FLAGS variables. Then type:
```
cd tracer
make
```

More detailed build instructions are available on TraceR's [documentation](https://tracer-codes.readthedocs.io/en/master/index.html#document-install) pages.

Refer to TraceR's [User Guide](https://tracer-codes.readthedocs.io/en/master/userguide.html) for details on how to do network simulations using TraceR.


###  Citing TraceR

Any published work that utilizes TraceR should cite the following paper:

Nikhil Jain, Abhinav Bhatele, Samuel T. White, Todd Gamblin, and Laxmikant V. Kale. [Evaluating HPC networks via simulation of parallel workloads](http://doi.ieeecomputersociety.org/10.1109/SC.2016.13). In Proceedings of the ACM/IEEE International Conference for High Performance Computing, Networking, Storage and Analysis, SC '16. IEEE Computer Society, November 2016. LLNL-CONF-690662.


### License

TraceR is distributed under the terms of the MIT license.

Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.

Written by:
```
    Nikhil Jain <nikhil.jain@acm.org>
    Bilge Acun <acun2@illinois.edu>
    Abhinav Bhatele <bhatele@llnl.gov>
```
LLNL-CODE-740483. All rights reserved.

SPDX-License-Identifier: MIT
