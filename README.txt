
Bigsim trace driven simulation for CODES.

modelnet-test-bigsim.c: Contains the main function that starts the simulation.
	Run this in the same way as the other CODES test programs.
	The folder that you are running the simulation should contain the application traces.

bigsim: This folder contains the classes that are needed for reading the traces
	and keeping track of the PE task dependencies.
	This requires CHARM++ to be installed and compiled with CHARM++.
	There is a seperate Makefile inside of this folder for this purpose.
	
	CWrapper: Wrapper functions for calling C++ functions in C. 
	TraceReader: For reading Bigsim traces.
	entities: Contains Task and PE classes.
	events: Contains Event classes.

USAGE:
Sequential: 		        	      modelnet-test-bigsim --sync=1 -- mapping_file_name.conf (optional --nkp)
Parallel Conservative: 		mpirun <args> modelnet-test-bigsim --sync=2 -- mapping_file_name.conf (optional --nkp)
Parallel Optimistic:   		mpirun <args> modelnet-test-bigsim --sync=3 -- mapping_file_name.conf (optional --nkp)
