Creating the job placement file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Ranking basics
""""""""""""""

TraceR requires two sets of mapping files (with some what redundant information).
Both types of files provide information about mapping of global rank to jobs and
their local rank. Global rank of a server/core is the logical rank that server LPs
get inside CODES. It increases linearly for servers/cores connected from one switch
to another. Due to the way default server to node mapping works within CODES, if
more than one node is connected to a switch, servers/cores are distributed in a
cyclic manner.

Consider this example config file::

    MODELNET_GRP
    {
        repetitions="8";
        server="4";
        modelnet_dragonfly="4";
        modelnet_dragonfly_router="1";
    }

Servers residing in nodes connected to the first router get global ranks 0-3,
nodes connected to the second router get global ranks 4-7, and so on.

Now, consider another case::

    MODELNET_GRP
    {
        repetitions="8";
        server="8";
        modelnet_dragonfly="4";
        modelnet_dragonfly_router="1";
    }

Servers residing in nodes connected to the first router get global ranks 0-7,
nodes connected to the second router get global ranks 8-15, and so on. However,
there are 8 servers but only 4 nodes, so each node hosts 2 servers. The servers
are distributed in a cyclic manner within a router, i.e. in router 0, server 0
is on node 0, 1 is on node 1, 2 is on node 2, 3 is on node 3, 4 is on node 0, 5
is on node 1, 6 is on node 2, and 7 is on node 3. Similar cyclic distribution is
done within every switch.

Map file requirements
"""""""""""""""""""""

Map files are divided into two sets: global map files and individual job files.
The global file specifies how the global ranks are mapped to individual jobs and
ranks within those jobs. It is a binary file structured as sets of 3 integers:
<global rank> <local rank> <job_id>. A typical write routine looks like this:

.. code::

    for(...)
        fwrite(&global_rank, sizeof(int), 1, binout);
        fwrite(&local_rank, sizeof(int), 1, binout);
        fwrite(&jobid, sizeof(int), 1, binout);
    endfor

For each job, individual job map files are needed. A map file for a job is also a
binary file filled with a series of global ranks. The global ranks are ordered by
using the local ranks as the key. So, if the series of integers is loaded into an
array called local_to_global, local_to_global[i] will contain the global rank of
local rank i.

Job mappers
"""""""""""

In the utils subfolder of the TraceR repository, there are several job mappers
written in C that can be used to generate job map files with various layouts.
Eventually these will likely be rewritten as a Python script. A brief summary
of the generators provided follows.

def_lin_mapping.C
    Generates a linear mapping which is also the default mapping
    when no mapping is specified. If nodes per router is more than 1, then this
    mapping will spread the ranks in a round-robin fashion among the nodes.

node_mapping.C
    Generates a mapping that always places servers with contiguous
    global ranks on a node. That is, if there are 2 servers per node, ranks 0-1 are
    on node 0, ranks 2-3 are on node 1, and so on.

multi_job.C
    Router based various schemes for mapping.

many_job.C
    Nodes based various schemes for mapping.

Commands for execution
""""""""""""""""""""""
./def_lin_mapping <global_map_file> <space separated #ranks in each job>

./node_mapping <global_map_file> <total_ranks in the job> <nodes per router> <cores per node> [optional <nodes with router to skip after>]

The output from these commands will be a global map file, and job{0,1..} files in binary format.

Example::

    ./def_lin_mapping global.bin 32 32 64

The above command generates global.bin with 128 ranks, where the first 32 are mapped to job0,
the next 32 to job1, and last 64 to job2. It also generates job0, job1, and job2 that maps
ranks from these jobs to global ranks.