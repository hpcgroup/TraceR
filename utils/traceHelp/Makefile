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

CHARMC ?=  $(HOME)/charm/bin/charmc

all: traceToFile

traceToFile: traceToFile.cpp 
	$(CHARMC) $< -seq -o $@ -lconv-bigsim-logs -lconv-util -lblue-standalone -language c++

clean:
	rm -rf traceToFile
