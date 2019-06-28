Creating the network (CODES) configuration file
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sample network configuration files can be found in examples/conf

Additional documentation on the format of the CODES config file can be found in the
CODES wiki at https://xgitlab.cels.anl.gov/codes/codes/wikis/home

A brief summary of the format follows.

LPGROUPS, MODELNET_GRP, PARAMS are keywords and should be used as is.

MODELNET_GRP
""""""""""""
repetition
    number of routers that have nodes connecting to them.

server
    number of MPI processes/cores per router

modelnet_*
    number of NICs. For torus, this value has to be 1; for dragonfly,
    it should be router radix divided by 4; for the fat-tree, it should be router
    radix divided by 2. For the dragonfly network, modelnet_dragonfly_router should
    also be specified (as 1). For express mesh, modelnet_express_mesh_router should
    also be specified as 1.

    Similarly, the fat-tree config file requires specifying fattree_switch which
    can be 2 or 3, depending on the number of levels in the fat-tree. Note that the
    total number of cores specified in the CODES config file can be greater than
    the number of MPI processes being simulated (specified in the tracer config
    file).

Common parameters
"""""""""""""""""

packet_size/chunk_size (both should have the same value)
    size of the packets created by NIC for transmission on the network. Smaller the
    packet size, longer the time for which simulation will run (in real time). Larger
    the packet size, the less accurate the predictions are expected to be (in virtual
    time). Packet sizes of 512 bytes to 4096 bytes are commonly used.

modelnet_order
    torus/dragonfly/fattree/slimfly/express_mesh

modelnet_scheduler
    fcfs: packetize messages one by one.

    round-robin: packetize message in a round robin manner.

message_size
    PDES parameter (keep constant at 512)

router_delay
    delay at each router for packet transmission (in nanoseconds)

soft_delay
    delay caused by software stack such as that of MPI (in nanoseconds)

link_bandwidth
    bandwidth of each link in the system (in GB/s)

cn_bandwidth
    bandwidth of connection between NIC and router (in GB/s)

buffer_size/vc_size
    size of channels used to store transient packets at routers (in
    bytes). Typical value is 64*packet_size.

routing
    how are packets being routed. Options depend on the network.

    torus: static/adaptive

    dragonfly: minimal/nonminimal/adaptive

    fat-tree: adaptive/static

Network specific parameters
"""""""""""""""""""""""""""

Torus:

n_dims
    number of dimensions in the torus

dim_length
    length of each dimension

Dragonfly:
    
num_routers
    number of routers within a group.
    
global_bandwidth
    bandwidth of the links that connect groups.

Fat-tree:

ft_type
    always choose 1

num_levels
    number of levels in the fat-tree (2 or 3)

switch_radix
    radix of the switch being used

switch_count
    number of switches at leaf level.