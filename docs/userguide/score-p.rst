Score-P
^^^^^^^

Installation of Score-P
"""""""""""""""""""""""

1. Download from http://www.vi-hps.org/projects/score-p/
#. tar -xvzf scorep-5.0.tar.gz
#. cd scorep-5.0
#. CC=mpicc CFLAGS="-O2" CXX=mpicxx CXXFLAGS="-O2" FC=mpif77 ./configure --without-gui --prefix=<SCOREP_INSTALL>
#. make
#. make install

Generating OTF2 traces with an MPI program using Score-P
""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Detailed instructions are available at https://silc.zih.tu-dresden.de/scorep-current/pdf/scorep.pdf.

1. Add $SCOREP_INSTALL/bin to your PATH for convenience. Example::

    export SCOREP_INSTALL=$HOME/workspace/scoreP/scorep-5.0/install
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
    export SCOREP_MPI_ENABLE_GROUPS=ENV,P2P,COLL,XNONBLOCK

 If Score-P prints a warning about flushing traces during the run, you may avoid them using::

    export SCOREP_TOTAL_MEMORY=256M
    export SCOREP_EXPERIMENT_DIRECTORY=/p/lscratchd/<username>/...

 .. note::
   For larger simulations, performance can get slow. There is a :download:`patch for Score-P 5.0 <scorep-5.0.patch>` that
   adds an option to reduce the number of MPI Probes. After applying the patch, it can be enabled like the other Score-P
   options with ``export SCOREP_REDUCE_PROBE_TEST=1``.

6. Run the binary and traces should be generated in a folder named scorep-\*.