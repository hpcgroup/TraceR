Creating a TraceR configuration file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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