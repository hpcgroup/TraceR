BigSim
^^^^^^

Installation of BigSim
""""""""""""""""""""""

Compile BigSim/Charm++ for emulation (see the `BigSim manual <https://charm.readthedocs.io/en/latest/bigsim/manual.html>`_
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

Generating AMPI traces with an MPI program using BigSim
"""""""""""""""""""""""""""""""""""""""""""""""""""""""

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