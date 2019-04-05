User Guide
==========

Below, we provide detailed instructions for how to start doing network
simulations using TraceR.

Quickstart
----------

This is a basic ``mpirun`` command to launch a TraceR simulation in the
optimistic mode::

    mpirun -np <p> ../traceR --sync=3  -- <network_config> <tracer_config>

Some useful options to TraceR.

--sync                 ROSS's PDES type. 1 - sequential, 2 - conservative, 3 - optimistic
--nkp                  number of groups used for clustering LPs; recommended value for lower rollbacks: (total #LPs)/(#MPI processes)
--extramem             number of messages in ROSS's extra message buffer (each message is ~500 bytes, 100K should work for most cases)
--max-opt-lookahead    leash on optimisitc execution in nanoseconds (1 micro second is a good value)
--timer-frequency      frequency with which PE0 should print current virtual time

Creating a TraceR config file
-----------------------------

This is the format for the TraceR config file::

    <global map file>
    <num jobs>
    <Trace path for job0> <map file for job0> <number of ranks in job0> <iterations (use 1 if running in normal mode)>
    <Trace path for job1> <map file for job1> <number of ranks in job1> <iterations (use 1 if running in normal mode)>

If you do not intend to create global or per-job map files, you can use ``NA``
instead of them.

See below to generate global or per-job map files.

Creating the target system configuration
----------------------------------------

Creating the job placement file
-------------------------------

Generating Traces
-----------------

Score-P
^^^^^^^

Installation of Score-P
"""""""""""""""""""""""

1. Download from http://www.vi-hps.org/projects/score-p/
#. tar -xvzf scorep-3.0.tar.gz
#. cd scorep-3.0
#. CC=mpicc CFLAGS="-O2" CXX=mpicxx CXXFLAGS="-O2" FC=mpif77 ./configure --without-gui --prefix=<SCOREP_INSTALL>
#. make
#. make install

Generating OTF2 traces with an MPI program using Score-P
""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Detailed instructions are available at https://silc.zih.tu-dresden.de/scorep-current/pdf/scorep.pdf.

Quick start
"""""""""""

1. Add $SCOREP_INSTALL/bin to your PATH for convenience. Example::
    export SCOREP_INSTALL=/g/g92/jain6/workspace/scoreP/scorep-3.0/install
    export PATH=$SCOREP_INSTALL/bin/$PATH
2. Add the following compile time flags to the application::
    -I$SCOREP_INSTALL/include -I$SCOREP_INSTALL/include/scorep -DSCOREP_USER_ENABLE
3. Add #include <scorep/SCOREP_User.h> to all files where you plan to add any of the following Score-P calls (optional step)::
    SCOREP_RECORDING_OFF(); - stop recording
    SCOREP_RECORDING_ON(); - start recording
    
 Marking special regions: SCOREP_USER_REGION_BY_NAME_BEGIN(regionname, SCOREP_USER_REGION_TYPE_COMMON) and SCOREP_USER_REGION_BY_NAME_END(regionname).
 Region names beginning with TRACER_WallTime\_ are special: using TRACER_WallTime_<any_name> prints current time during simulation with tag <any_name>.

4. for the link step, prefix the linker line with the following::
    LD = scorep --user --nocompiler --noopenmp --nopomp --nocuda --noopenacc --noopencl --nomemory <your_linker>
5. For running, set::
    export SCOREP_ENABLE_TRACING=1
    export SCOREP_ENABLE_PROFILING=0
    export SCOREP_REDUCE_PROBE_TEST=1
    export SCOREP_MPI_ENABLE_GROUPS=ENV,P2P,COLL,XNONBLOCK

 If Score-P prints a warning about flushing traces during the run, you may avoid them using::
    export SCOREP_TOTAL_MEMORY=256M
    export SCOREP_EXPERIMENT_DIRECTORY=/p/lscratchd/<username>/...

6. Run the binary and traces should be generated in a folder named scorep-\*.

BigSim
^^^^^^

