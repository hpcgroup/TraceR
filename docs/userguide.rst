User Guide
==========

Below, we provide detailed instructions for how to start doing network
simulations using TraceR.

.. _userguide-quickstart:

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

Setting up a Simulation
-----------------------

.. _userguide-tracer-config-file:
.. include:: userguide/tracer-config-file.rst

See :ref:`userguide-job-placement-file` below for how to generate global or per-job map files.

.. _userguide-codes-config-file:
.. include:: userguide/codes-config-file.rst

.. _userguide-job-placement-file:
.. include:: userguide/job-placement-file.rst

Generating Traces
-----------------

.. _userguide-score-p:
.. include:: userguide/score-p.rst

.. _userguide-bigsim:
.. include:: userguide/bigsim.rst
