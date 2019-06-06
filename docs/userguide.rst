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

    export SCOREP_INSTALL=$HOME/workspace/scoreP/scorep-3.0/install
    export PATH=$SCOREP_INSTALL/bin:$PATH

2. Add the following compile time flags to the application::

    -I$SCOREP_INSTALL/include -I$SCOREP_INSTALL/include/scorep -DSCOREP_USER_ENABLE

3. Add #include <scorep/SCOREP_User.h> to all files where you plan to add any of the following Score-P calls (optional step)::

    SCOREP_RECORDING_OFF(); - stop recording
    SCOREP_RECORDING_ON(); - start recording
    
 Marking special regions: SCOREP_USER_REGION_BY_NAME_BEGIN(regionname, SCOREP_USER_REGION_TYPE_COMMON) and SCOREP_USER_REGION_BY_NAME_END(regionname).
 
 Region names beginning with TRACER_WallTime\_ are special: using TRACER_WallTime_<any_name> prints current time during simulation with tag <any_name>.

 An example using these features is given below:

 .. literalinclude:: code-examples/scorep_user_calls.c
    :language: c

4. For the link step, prefix the linker line with the following::

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

Installation of BigSim
""""""""""""""""""""""

Compile BigSim/Charm++ for emulation (see http://charm.cs.illinois.edu/manuals/html/bigsim/manual-1p.html
for more detail). Use any one of the following commands:

- To use UDP as BigSim/Charm++'s communication layer::

    ./build bgampi net-linux-x86_64 bigemulator --with-production --enable-tracing
    ./build bgampi net-darwin-x86_64 bigemulator --with-production --enable-tracing

  Or explicitly provide the compiler optimization level::

    ./build bgampi net-linux-x86_64 bigemulator -O2

- To use MPI as BigSim/Charm++'s communication layer::

    ./build bgampi mpi-linux-x86_64 bigemulator --with-production --enable-tracing

Note that this build is used to compile MPI applications so that traces can be
generated. Hence, the communication layer used by BigSim/Charm++ is not
important. During simulation, the communication will be replayed using the
network simulator from CODES. However, the computation time captured here can be
important if it is not being explicitly replaced at simulation time using
configuration options. So using appropriate compiler flags is important.

Quick start
"""""""""""

1. Compile your MPI application using BigSim/Charm++.

 Example commands::

    $CHARM_DIR/bin/ampicc -O2 simplePrg.c -o simplePrg_c
    $CHARM_DIR/bin/ampiCC -O2 simplePrg.cc -o simplePrg_cxx

2. Emulation to generate traces. When the binary generated is run,
   BigSim/Charm++ runs the program on the allocated cores as if it were
   running as usual. Users should provide a few additional arguments to
   specify the number of MPI processes in the prototype systems.

 If using UDP as the BigSim/Charm++'s communication layer::

    ./charmrun +p<number of real processes> ++nodelist <machine file> ./pgm <program arguments> +vp<number of processes expected on the future system> +x<x dim> +y<y dim> +z<z dim> +bglog

 If using MPI as the BigSim/Charm++'s communication layer::

    mpirun -n<number of real processes> ./pgm <program arguments> +vp<number of processes expected on the future system> +x<x dim> +y<y dim> +z<z dim> +bglog

 Number of real processes is typically equal to the number cores the emulation
 is being run on.

 *machine file* is the list of systems the emulation should be run on (similar to
 machine file for MPI; refer to Charm++ website for more details).

 *vp* is the number of MPI ranks that are to be emulated. For simple tests, it can
 be the same as the number of real processes, in which case one MPI rank is run on
 each real process (as it happens when a regular program is run). When the
 number of vp (virtual processes) is higher, BigSim launches user level threads
 to execute multiple MPI ranks within a process.

 *+x +y +z* defines a 3D grid of the virtual processes. The product of these three
 dimensions must match the number of vp's. These arguments do not have any
 effect on the emulation, but exist due to historical reasons.

 *+bglog* instructs bigsim to write the logs to files.

3. When this run is finished, you should see many files named *bgTrace\** in the
   directory. The total number of such files equals the number of real processes
   plus one. Their names are bgTrace, bgTrace0, bgTrace1, and so on. 
   Create a new folder and move all *bgTrace* files to that folder.
