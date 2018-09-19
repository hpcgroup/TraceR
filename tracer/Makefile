##############################################################################
# Copyright (c) 2015, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory.
#
# Written by:
#     Nikhil Jain <nikhil.jain@acm.org>
#     Bilge Acun <acun2@illinois.edu>
#     Abhinav Bhatele <bhatele@llnl.gov>
#
# LLNL-CODE-740483. All rights reserved.
#
# This file is part of TraceR. For details, see:
# https://github.com/LLNL/TraceR
# Please also read the LICENSE file for the MIT License notice.
##############################################################################

include Makefile.common

TRACER_LDADD = bigsim/CWrapper.o bigsim/TraceReader.o bigsim/otf2_reader.o \
bigsim/entities/PE.o bigsim/entities/Task.o bigsim/entities/MsgEntry.o 

all: traceR
.PHONY: components

SRCS = tracer-driver.C p2p-events.C coll-events.C
OBJS = ${SRCS:.C=.o}

traceR: ${OBJS} components
	${CXX} ${LDFLAGS} ${OBJS} -o $@ ${TRACER_LDADD} ${LIBS}

%.o: %.C
	${CXX} ${CXXFLAGS} -c $< -o $@

components:
	cd bigsim; make;

clean:
	rm -rf *.o traceR
	cd bigsim; make clean;
