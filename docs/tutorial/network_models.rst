.. _tutorial-network-models:

Network Models
==============

This guide will give an overview of some of the network models
supported by TraceR, as presented in the HOTI 25 tutorial (slides 22-39).
For a more detailed guide, see the CODES wiki pages on network
models at https://github.com/codes-org/codes/wiki/codes-networks.
Any commands/examples in this section are referring to files
included in the CODES git repository (not TraceR).

Overview
--------

Multiple network models are supported, including dragonfly, fat
tree, express mesh, hyperX, torus, slim fly, and LogP. An abstraction
layer, ``model-net``, sits on top of network models that breaks
messages into packets and offers FIFO, round robin, and priority
queues. To try different networks, simply switch the network configuration
files used when running TraceR. Storage models, MPI simulation, and
workload replay layers are independent of the underlying network
model used.

Simplenet
---------

The Simplenet model uses a latency/bandwidth model where messages are
sent directly from the source to the destination. It uses infinite
queueing, and is easy to setup - a startup delay and link bandwidth
are used for configuration. This model is mostly for debugging and
testing purposes and can be used as a starting point when replaying
MPI traces. It can be used as a baseline network model with no contention
and no routing.

Configuring
^^^^^^^^^^^

Consider this Simplenet configuration file that can be
found in *codes/tests/conf/modelnet-test.conf*::

    LPGROUPS
    {
        MODELNET_GRP
        {
            repetitions="16";
            server="1";
            modelnet_simplenet="1";
        }
    }
    PARAMS
    {
        packet_size="512";
        message_size="384";
        modelnet_order=( "simplenet" );
        # scheduler options
        modelnet_scheduler="fcfs";
        net_startup_ns="1.5";
        # bandwidth is in MiB/s
        net_bw_mbps="20000";
    }

The MODELNET_GRP section is used for mapping entities to
ROSS MPI processes.

Messages are broken into packets by the ``model-net`` layer,
with a size that can be set by the ``packet_size`` param.

The ``message_size`` parameter is a ROSS specific parameter
that is used to set the event size.

``net_startup_ns`` sets the startup delay in nanoseconds.

``net_bw_mbps`` sets the link bandwidth in MB/s between nodes.
There is one link between each pair of nodes.

Running
^^^^^^^

The model shown above can be run in CODES with::

    ./tests/modelnet-test --sync=1 -- tests/conf/modelnet-test.conf

The command runs a simple test in which a simulated MPI rank
sends a message to the next rank, which replies back. This
continues until a certain number of messages is reached.

Dragonfly
---------

The dragonfly network model has a hierarchy with a set of
groups connected with all-to-all links. Within a group there
can be several routers connected with local links, and routers
can have links to routers in other groups for intergroup
connections. Routers will also have compute nodes connected to
to them. The CODES wiki explains dragonfly networks in much
greater detail, and the slides from the HOTI 25 tutorial have
images showing examples of possible dragonfly networks.

Dragonfly networks support minimal, adaptive, non-minimal, and
progressive adaptive routing. They use packet based simulation
with credit based flow control, and use multiple virtual channels
for deadlock prevention.

Configuring
^^^^^^^^^^^

Consider this example configuration that can be found with the
CODES source, *codes/src/network-workloads/dragonfly-custom*::

    LPGROUPS
    {
        MODELNET_GRP
        {
            repetitions="2400";
    # name of this lp changes according to the model
            nw-lp="4";
    # these lp names will be the same for dragonfly custom model
            modelnet_dragonfly_custom="4";
            modelnet_dragonfly_custom_router="1";
        }
    }

``nw-lp`` is a simulated MPI process. For simulating multiple MPI
processes per node, set this to the number of processes times the
number of network nodes.

``modelnet_dragonfly_custom`` is a simulated dragonfly network node.

``modelnet_dragonfly_custom_router`` is a simulated dragonfly network router.

Self messages are messages sent to the same network node. The overhead for sending
self messages can be configured.

Continuing in the same configuration file, look at the PARAM section::

    PARAMS
    {
    # packet size in the network
        packet_size="4096";
        modelnet_order=( "dragonfly_custom","dragonfly_custom_router" );
        # scheduler options
        modelnet_scheduler="fcfs";
    # chunk size in the network (when chunk size = packet size, packets will not be
    # divided into chunks)
        chunk_size="4096";
        # number of routers within each group
        # this is dictated by the dragonfly configuration files
        num_router_rows="6";
        # number of router columns
        num_router_cols="16";
        # number of groups in the network
        num_groups="25";
    # buffer size in bytes for local virtual channels
        local_vc_size="8192";
    # buffer size in bytes for global virtual channels
        global_vc_size="16384";
    # buffer size in bytes for compute node virtual channels
        cn_vc_size="8192";
    # bandwidth in GiB/s for local channels
        local_bandwidth="5.25";
    # bandwidth in GiB/s for global channels
        global_bandwidth="4.69";
    # bandwidth in GiB/s for compute node-router channels
        cn_bandwidth="16.0";
    # ROSS message size
        message_size="592";
    # number of compute nodes connected to router, dictated by dragonfly configuration
    # file
        num_cns_per_router="4";
    # number of global channels per router
        num_global_channels="4";
    # network config file for intra-group connections
        intra-group-connections="../src/network-workloads/conf/dragonfly-custom/intra-9K-custom";
    # network config file for inter-group connections
        inter-group-connections="../src/network-workloads/conf/dragonfly-custom/inter-9K-custom";
    # routing protocol to be used
        routing="prog-adaptive";
    }

``num_router_rows`` and ``num_router_cols`` control the router arrangement within a group
and should match the input network configuration.

``local_vc_size``, ``global_vc_size``, and ``cn_vc_size`` are used to configure the buffer
size of virtual channels.

``num_cns_per_router`` is used to set the number of compute nodes per router.

``intra-group-connections`` and ``inter-group-connections`` are set to network configuration
files that can be custom generated (see scripts/gen-cray-topo/README.txt).

Running
^^^^^^^

To run a dragonfly network simulation, try the following:

1. Download the traces::

    wget https://portal.nersc.gov/project/CAL/doe-miniapps-mpi-traces/AMG/df_AMG_n1728_dumpi.tar.global_vc_size

2. Run the simulation::

    ./src/network-workloads/model-net-mpi-replay --sync=1 --disable_compute=1 --workload_type="dumpi" --workload_file=df_AMG_n1728_dumpi/dumpi-2014.03.03.14.55.50- --num_net_traces=1728 -- ../src/network-workloads/conf/dragonfly-custom/modelnet-test-dragonfly-edison.conf

Fat Tree
--------

The Fat Tree network model can simulate two and three level fat tree networks.
The width of the tree (number of pods) can also be configured. Two forms of
routing are supported; static which uses destination-based look-up tables,
and adaptive which selects the least congested output port. The simulation
is packet-based with credit-based flow control.

Tapering can be used in a fat tree network configuration to connect more nodes to leaf
switches, which reduces the bandwidth, switches, and links at a higher level.

To get higher bandwidth, nodes can connect to multiple ports (multi-rail) in one
or more plane (multi-plane). These configurations can also be tapered to reduce
switches and links at higher levels.

The model supports configurations for multiple rails, multiple plane, and tapering.

Configuring
^^^^^^^^^^^

Consider the first part of this configuration file::

    LPGROUPS
    {
        MODELNET_GRP
        {
            repetitions="198";
            nw-lp="144";
            modelnet_fattree="18";
            fattree_switch="3";
        }
    }

``nw-lp`` is a simulated MPI process.

``modelnet_fattree`` is a simulated fat tree network node.

``fattree_switch`` sets the number of simulated fat tree network
switches. In the above example it is set to 3 (one in each level
of the network).

Now, consider the next section in the configuration file::

    PARAMS
    {
        packet_size="4096";
        message_size="624";
        chunk_size="4096";
        modelnet_scheduler="fcfs"
        modelnet_order=( "fattree" );
        ft_type="0";
        num_levels="3";
        switch_count="198";
        switch_radix="36";
        vc_size="65536";
        cn_vc_size="65536";
        link_bandwidth="12.5";
        cn_bandwidth="12.5";
        routing="static";
        routing_folder="/Fat-Tree/summit";
        dot_file="summit-3564"
        dump_topo="0";
    }

The switch arrangement set with ``ft_type``, ``num_levels``, and ``switch_count``
should match the input network configuration.

``switch_radix`` can be configured.

Static routing requires precomputed destination routing tables, for details
see https://xgitlab.cels.anl.gov/codes/codes/wikis/codes-fattree#enabling-static-routing.

Slim Fly
--------

The Slim Fly network model has a topology of interconnected router groups build
with MMS graphs. The maximum network diameter is always 2. It uses a packet-based
simulation with credit-based flow control. The forms of routing supported are
minimal with 2 virtual channels, non-minimal with 4 virtual channels, and adaptive
with 4 virtual channels.

Configuring
^^^^^^^^^^^

Consider the params section for a slim fly network configuration file::

    PARAMS
    {
        packet_size="4096";
        chunk_size="4096";
        message_size="592";
        modelnet_order=( "slimfly" );
        modelnet_scheduler="fcfs";
        num_routers="13";
        num_terminals="9";
        global_channels="13";
        local_channels="6";
        generator_set_x=("1","10","9","12","3","4");
        generator_set_x_prime=("6","8","2","7","5","11");
        local_vc_size="25600";
        global_vc_size="25600";
        cn_vc_size="25600";
        local_bandwidth="12.5";
        global_bandwidth="12.5";
        cn_bandwidth="12.5";
        routing="minimal";
        num_vcs="4";
    }

``num_routers``, ``num_terminals``, ``global_channels``, and ``local_channels`` can
be used to confiure the router arrangement within a group.

Generator sets are a set of indices used to calculate connections between routers
in the same subgraph. They must be precomputed. The params ``generator_set_x`` and
``generator_set_x_prime`` are set based on the precomputed indices.

Torus
-----

A torus network is based on a n-dimensional k-ary network topology. The number of
torus dimensions and length of each dimension can be configured. The network model
supports dimension order routing.

Express Mesh and HyperX
-----------------------

The express mesh topology is low-diameter densely connected grids. The model alllows
for specifying the connection gap. A gap of 1 is a HyperX network. A bubble escape
virtual channel is used for deadlock prevention.

Interpreting Simulation Output
------------------------------

Using the example run on the dragonfly network given above, we ge tthe following output::

            Total GVT Computations                       0
            Total All Reduce Calls                       0
            Average Reduction/GVT                      nan
    
    Total bytes sent 13584368 recvd 13584368
    max runtime 449332.124035 ns avg runtime 443706.882419
    max comm time 449332.124035 avg comm time 443706.882419
    max send time 5142770.436275 avg send time 2779472.247926
    max recv time 4149449.596308 avg recv time 2335071.940672
    max wait time 432820.362362 avg wait time 430457.043452
    _P-IO: writing output to dragonfly-simple-33405-1499374633/
    _P-IO: data files:
        dragonfly-simple-33488-1499374633/dragonfly-router-traffic
        dragonfly-simple-33488-1499374633/dragonfly-router-net_stats
        dragonfly-simple-33488-1499374633/dragonfly-msg-stats
        dragonfly-simple-33488-1499374633/model-net-category-all
        dragonfly-simple-33488-1499374633/model-net-category-test
        dragonfly-simple-33488-1499374633/mpi-replay-stats
    Average number of hops traversed 1.709869 average chunk latency 0.925252 us maximum chunk latency 9.312357 us avg message size 812.563110 bytes finished messages 16820 finished chunks 65012

    ADAPTIVE ROUTING STATS 65012 chunks routed minimally 0 chunks routed non-minimally completed packets 65012

    Total packets generated 39722 finished 39722

As shown in the sample output, average and maximum times are reported
for all application runs with statistics on time spent in overall execution,
communication, wait operations, amount of data transferred, and so on.

Enabling lp-io-dir generates detailed network statistics files. The network
statistics (hops traversed, latency, routing, etc) are reported for the
entire network.

Detailed statistics for each MPI rank, network node, router, and port are
generated using the lp-io-dir option.

``--lp-io-dir=my-dir`` can be used to enable statistics generation (each lp
writes its statistics to a summary file).

Statistics Reported by LP-IO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``Dragonfly-msg-stats`` has the number of hops, packet latency, packets
sent/received, and link saturation time reported for each network node.

``Dragonfly-router-stats`` has the link saturation time for each router port.

``Dragonfly-router-traffic`` has the traffic sent for each router port.

Fat tree and slim fly networks have similar statistics files.

``Mpi-replay-stats`` (generated for any network model) has the bytes
sent/received per MPI process, the time spent in communication per
MPI process, and the number of sends and receives per MPI process.