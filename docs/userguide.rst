User Guide
==========

Below, we provide detailed instructions for how to start doing network
simulations using TraceR.

Quickstart
----------

This is a basic ``mpirun`` command to launch a TraceR simulation in the
optimistic mode::

    mpirun -np <p> ../traceR --sync=3  -- <network_config> <tracer_config>

Some useful options to use with TraceR:

--sync                 ROSS's PDES type. 1 - sequential, 2 - conservative, 3 - optimistic
--nkp                  number of groups used for clustering LPs; recommended value for lower rollbacks: (total #LPs)/(#MPI processes)
--extramem             number of messages in ROSS's extra message buffer (each message is ~500 bytes, 100K should work for most cases)
--max-opt-lookahead    leash on optimistic execution in nanoseconds (1 microsecond is a good value)
--timer-frequency      frequency with which PE0 should print current virtual time

Creating a TraceR configuration file
------------------------------------

This is the format for the TraceR config file::

    <global map file>
    <num jobs>
    <Trace path for job0> <map file for job0> <number of ranks in job0> <iterations (use 1 if running in normal mode)>
    <Trace path for job1> <map file for job1> <number of ranks in job1> <iterations (use 1 if running in normal mode)>
    ...
    <Trace path for jobN> <map file for jobN> <number of ranks in jobN> <iterations (use 1 if running in normal mode)>


If you do not intend to create global or per-job map files, you can use ``NA``
instead of them.

Sample TraceR config files can be found in examples/jacobi2d-bigsim/tracer_config (BigSim) or examples/stencil4d-otf/tracer_config (OTF)

See `Creating the job placement file`_ below for how to generate global or per-job map files.

Creating the network (CODES) configuration file
-----------------------------------------------
Sample network configuration files can be found in examples/conf

Additional documentation on the format of the CODES config file can be found in the
CODES wiki at https://xgitlab.cels.anl.gov/codes/codes/wikis/home

A brief summary of the format follows.

LPGROUPS, MODELNET_GRP, PARAMS are keywords and should be used as is.

MODELNET_GRP::

    repetition = number of routers that have nodes connecting to them.

    server = number of MPI processes/cores per router

    modelnet_* = number of NICs. For torus, this value has to be 1; for dragonfly,
    it should be router radix divided by 4; for the fat-tree, it should be router
    radix divided by 2. For the dragonfly network, modelnet_dragonfly_router should
    also be specified (as 1). For express mesh, modelnet_express_mesh_router should
    also be specified as 1.

    Similarly, the fat-tree config file requires specifying fattree_switch which
    can be 2 or 3, depending on the number of levels in the fat-tree. Note that the
    total number of cores specified in the CODES config file can be greater than
    the number of MPI processes being simulated (specified in the tracer config
    file).

Other common parameters::

    packet_size/chunk_size (both should have the same value) = size of the packets
    created by NIC for transmission on the network. Smaller the packet size, longer
    the time for which simulation will run (in real time). Larger the packet size,
    the less accurate the predictions are expected to be (in virtual time). Packet
    sizes of 512 bytes to 4096 bytes are commonly used.

    modelnet_order = torus/dragonfly/fattree/slimfly/express_mesh

    modelnet_scheduler =
        fcfs: packetize messages one by one.
        round-robin: packetize message in a round robin manner.

    message_size = PDES parameter (keep constant at 512)

    router_delay = delay at each router for packet transmission (in nanoseconds)

    soft_delay = delay caused by software stack such as that of MPI (in nanoseconds)

    link_bandwidth = bandwidth of each link in the system (in GB/s)

    cn_bandwidth = bandwidth of connection between NIC and router (in GB/s)

    buffer_size/vc_size = size of channels used to store transient packets at routers (in
    bytes). Typical value is 64*packet_size.

    routing = how are packets being routed. Options depend on the network.
        torus: static/adaptive
        dragonfly: minimal/nonminimal/adaptive
        fat-tree: adaptive/static

Network specific parameters::

    Torus:
        n_dims = number of dimensions in the torus
        dim_length = length of each dimension

    Dragonfly:
        num_routers = number of routers within a group.
        global_bandwidth = bandwidth of the links that connect groups.

    Fat-tree:
        ft_type = always choose 1
        num_levels = number of levels in the fat-tree (2 or 3)
        switch_radix =  radix of the switch being used
        switch_count = number of switches at leaf level.

Creating the job placement file
-------------------------------

See the README in utils for instructions on using the tools to generate the global and job mapping files.

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

.. note::
   This build is used to compile MPI applications so that traces can be
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
