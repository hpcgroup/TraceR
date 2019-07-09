.. _tutorial-workflow:

Expected Workflow
=================

This guide will walk you through the expected workflow for using TraceR.
It will direct you to resources on generating BigSim and OTF2 traces, the
format of a TraceR configuration file, and a basic command for running a
simulation.

TraceR is a replay tool targeted to simulate control flow of application on
prototype systems, i.e., if control flow of an application, which includes
expected computation tasks, communication routines, and their dependencies, is
provided to TraceR, it will mimic the flow on a hypothetical system with a given
compute and communication capability. As of now, the control flow is captured by
either emulating applications using BigSim or by linking with Score-P. CODES
is used for simulating the communication on the network.

1. Write an MPI application. (Avoid global variables so that the application be
   run with virtualization if using BigSim). Included in the TraceR repository are
   two examples: jacobi2d-bigsim and stencil4d-otf. The jacobi2d-bigsim example
   shows how a program would be compiled to generate BigSim traces, and the
   stencil4d-otf example shows how to compile a program for generating OTF2 traces.

   .. note::
      If you're using BigSim, avoid global variables in your MPI application so that it can be run with virtualization.

2. Generate traces. For instructions on generating OTF2 traces, see the user guide
   section on using :ref:`userguide-score-p`, or for using BigSim traces see the section in
   the user guide about :ref:`userguide-bigsim`.

3. After generating traces, 2 files are needed: a tracer config file, and a codes config file.
   Optionally, mapping files can also be provided. See :ref:`userguide-tracer-config-file`, :ref:`userguide-codes-config-file`,
   and :ref:`userguide-job-placement-file` in the user guide for instructions on creating the files.

4. Run the simulation using ``mpirun``. For details on options available, see the
   :ref:`quickstart section of the user guide <userguide-quickstart>`. This command will
   run a simulation in optimistic mode::
    
    mpirun -np <p> ../traceR --sync=3  -- <network_config> <tracer_config>