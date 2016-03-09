TraceR v1.0
===========

### Compilation

1. Download and install ROSS:
```git clone https://github.com/carothersc/ROSS
```
Latest verified commit: d62031fb226ffce968299571bd781cefe7270d71

2. Download and install codes-base:
```git clone git://git.mcs.anl.gov/radix/codes-base
```
Latest verfied commit: 7e9a569449ae914d2ee6f21fbd20cc31657a6651

3. Download and install codes-net:
```git clone -b uiuc_tracer git://git.mcs.anl.gov/radix/codes-net
```
Branch user - uiuc_tracer

Use install-libLIBRARIES and install-data-am as target. We need C++ maps that
are implemented inside tracer. Hence, only the library build is possible.

4. Download and build Charm++ with bgampi target:
```git clone http://charm.cs.uiuc.edu/gerrit/charm
```

Follow instructions in the [Charm++ manual](http://charm.cs.illinois.edu/manuals/html/charm++/A.html). Replace the "charm++" target with bgampi.

5. Set the appropriate paths in tracer/Makefile.common and then:
```cd tracer
make
```

### Run

```mpirun -np <p> ../traceR --sync=3  -- ../conf/<choose here> <tracer_config>
```

Format of trace_config:
```
<global_map file>
<num jobs>
<Trace folder for job0> <map file for job0> <number of ranks in job0>
<Trace folder for job1> <map file for job1> <number of ranks in job1>
...
```

If <global_map file> is not needed, use NA for it and <map file for job*>.
For generating simple global and job map file, use the code in utils.

Example files are in jacobi2d. Sample run command:

```mpirun -np 8 ../traceR --sync=3  --nkp=16  -- ../conf/tracer-torus.conf tracer_config
```
