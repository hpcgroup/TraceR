.. _tutorial-simulation-basics:

Four Steps to Simulations
=========================

Creating a network simulation can be broken down into 4 steps:

1. Prototype the system design
------------------------------

An overview of setup using network parameters was given
in :ref:`tutorial-network-models`.

2. Workload selection
---------------------

There are two types of workloads that can be used in a simulation,
synthetic workloads and HPC application traces.

Synthetic Workloads
^^^^^^^^^^^^^^^^^^^

Synthetic workloads follow specific communication patterns with a
constant injection rate. Often they are used to stress the network
topology to identify best and worst case performance. Examples of
synthetic workloads include uniform random, all to all, bisection
pairing, and bit permutation. These workloads don't require simulation
of MPI operations, and could be used to generate background traffic
that can simulate interference with an application trace caused by
a production HPC system having a significant fraction of network nodes
being occupied.

**Uniform Random**: A network node is equally likely to send to any other
network node (traffic distributed throughout the network).

**All to All**: Each network node communicates with all other network nodes.

**Nearest Neighbor**: A network node communicates with nearby network nodes
(or the ones that are at minimal number of hops).

**Permutation Traffic**: Source node sends all traffic to a single destination
based on a permutation matrix.

**Bisection Pairing**: Node 0 communicates with Node 'n', Node 1 with 'n-1',
and so on.

HPC Application Traces
^^^^^^^^^^^^^^^^^^^^^^

Application traces are captured by running an MPI program. They are
useful for network performance prediction of production HPC applications.
Trace sizes can be large for long running or communication intensive
applications, but they have the potential to capture computation-communication
interplay. These workloads require accurate simulation of MPI operations, and
simulation results can be complex to analyze.

3. Workload creation
--------------------

A workload can be created by capturing application traces from
running an MPI program. Options for capturing a trace include
using DUMPI, :ref:`userguide-score-p`, and :ref:`userguide-bigsim`.

Information in a Typical Trace
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A typical trace captured (e.g. in DUMPI, OTF2, BigSim) for an
MPI program contains information on the operations that occur
at different times with critical information for the operation.
The table below gives an example of a typical trace.

===========================   ================   =========================================================
Time stamp, t (rounded off)   Operation type     Operation data (only critical information is highlighted)
===========================   ================   =========================================================
t = 10                        MPI_Bcast          root, size of bcast, communicator
t = 10.5                      MPI_Irecv          source, tag, communicator, req ID
t = 10.51                     user_computation   optional region name - "boundary updates"
t = 12.51                     MPI_Isend          dest, tag, communicator, req ID
t = 12.53                     user_computation   optional region name - "core updates"
t = 22.53                     MPI_Waitall        req IDs
t = 25                        MPI_Barrier        communicator
===========================   ================   =========================================================

Effect of Replaying Traces
^^^^^^^^^^^^^^^^^^^^^^^^^^

As shown in the table below, replaying a trace can result in
different results from the original run due to different configurations
resulting in operations taking more or less time to run.

====================   =================   ===============   ============   ================
Original time stamps   Original duration   New time stamps   New duration   Operation type
====================   =================   ===============   ============   ================
10                     0.5                 10                0.2            MPI_Bcast
10.5                   0.01                10.2              0.01           MPI_Irecv
10.51                  2                   10.21             2              user_computation
12.51                  0.02                12.21             0.02           MPI_Isend
12.53                  10                  12.23             10             user_computation
22.53                  2.47                22.23             0.03           MPI_Waitall
25                     1                   22.26             1.7            MPI_Barrier
====================   =================   ===============   ============   ================

In addition to the affect of the network configuration, different trace
formats may result in different results.

As an example, DUMPI stores all the information passed to MPI calls. The
simulation then decides which request to fulfill, allowing accurate resolution
for the target systems. If the control flow of the program can change
significantly due to the ordering of operations, then simulations are not
entirely correct.

On the other hand, OTF2 stores only the information that is used (e.g. which
request was satisfied). This accurately mimics the control flow of the trace
run, but does not accurately represent execution for the target system.

These differences are artifacts of leveraging existing tools not originally
intended for Parallel Discrete Event Simulation (PDES).

4. Execution
------------

The user guide :ref:`userguide-quickstart` section shows the
arguments taken by TraceR and some of the options available
to control execution of a simulation.
