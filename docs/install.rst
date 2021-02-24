Download and Install
====================

TraceR can be downloaded from `GitHub <https://github.com/LLNL/TraceR>`_.

Dependencies
------------

TraceR depends on `CODES <https://github.com/codes-org/codes>`_ and `ROSS <https://github.com/ROSS-org/ROSS>`_.

Build
-----

There are several ways to build TraceR.

1. Use `spack <https://github.com/spack/spack>`_ to build TraceR and its dependencies::

    spack install tracer

2. Build TraceR and its dependencies manually:

* Download and install ROSS and CODES. Set the appropriate paths: ROSS_DIR, and
  CODES_DIR in tracer/Makefile.common.
* Pick between the two trace formats supported by TraceR: OTF2 or BigSim, and
  accordingly build the OTF2 or Charm++ library. If using OTF2 traces
  (default), set SELECT_TRACE = -DTRACER_OTF_TRACES=1, and ensure that
  otf2-config is in your PATH. If using BigSim traces, set SELECT_TRACE =
  -DTRACER_BIGSIM_TRACES=1, and set CHARMPATH to the Charm++ installation in
  tracer/Makefile.common.
* Set the ARCH variable in tracer/Makefile.common or alternatively set the CXX
  and ARCH_FLAGS variables. Then type::

    cd tracer
    make

Trace Formats
^^^^^^^^^^^^^

TraceR supports two different trace formats as input. For each format, you need to build additional software as explained below.

1. Score-P's OTF2 format (default): To use OTF2 traces, you need to download and build the `OTF2 <http://www.vi-hps.org/projects/score-p>`_ library.
2. AMPI-based BigSim format: To use BigSim traces as input to TraceR, you need to download and build `Charm++ <https://github.com/UIUC-PPL/charm>`_.

The instructions to build Charm++ are in the `Charm++ manual
<https://charm.readthedocs.io/en/latest/charm++/manual.html#installing-charm>`_. You should use
the "charm++" target and pass "bigemulator" as a build option.
