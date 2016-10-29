##############################################################################
# Copyright (c) 2015, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory.
#
# Written by:
#     Nikhil Jain <nikhil.jain@acm.org>
#     Bilge Acun <acun2@illinois.edu>
#     Abhinav Bhatele <bhatele@llnl.gov>
#
# LLNL-CODE-681378. All rights reserved.
#
# This file is part of TraceR. For details, see:
# https://github.com/LLNL/tracer
# Please also read the LICENSE file for our notice and the LGPL.
##############################################################################

include Makefile.common

TRACER_LDADD = bigsim/CWrapper.o bigsim/TraceReader.o \
bigsim/entities/PE.o bigsim/entities/Task.o bigsim/entities/MsgEntry.o 

TRACER_LDADD += ${CODES_LIBS} ${CHARM_LIBS} ${OTF_LIBS}

TRACER_CFLAGS = ${CODES_CFLAGS} ${SELECT_TRACE}

all: traceR
.PHONY: components

traceR: tracer-driver.o components
	$(CXX) ${LDFLAGS} $< -o $@ ${TRACER_LDADD}

tracer-driver.o: tracer-driver.C
	$(CXX) $(CFLAGS) ${BASE_INCS} $(TRACER_CFLAGS) -c $< -o $@

components:
	cd bigsim; make;

clean:
	rm -rf *.o traceR
	cd bigsim; make clean;
