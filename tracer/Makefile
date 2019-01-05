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

TRACER_LDADD = reader/CWrapper.o reader/TraceReader.o reader/otf2_reader.o \
elements/PE.o elements/Task.o elements/MsgEntry.o

all: traceR
.PHONY: components install

PREFIX = ..

SRCS = p2p-events.C coll-events.C tracer-driver.C
OBJS = ${SRCS:.C=.o}

traceR: ${OBJS} components
	${CXX} ${LDFLAGS} ${OBJS} -o $@ ${TRACER_LDADD} ${LIBS}

%.o: %.C
	${CXX} ${CXXFLAGS} -c $< -o $@

components:
	cd reader; make;
	cd elements; make;

install: traceR
	mkdir -p $(PREFIX)/bin
	cp traceR $(PREFIX)/bin

clean:
	rm -rf *.o traceR
	cd reader; make clean;
	cd elements; make clean;
