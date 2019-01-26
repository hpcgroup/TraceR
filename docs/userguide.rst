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

ScoreP
^^^^^^

BigSim
^^^^^^

