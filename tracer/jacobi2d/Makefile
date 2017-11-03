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

CC = /scratch/nikhil/charm-master/netlrts-linux-x86_64-bigemulator/bin/ampiCC -g

all: jacobi2d 

jacobi2d: jacobi2d.C
	$(CC) -o jacobi2d $<

trace: jacobi2d.C
	$(CC) -o jacobi2d.trace $< $(CFLAGS) $(TRACE_LIB)

scal: jacobi2d.C
	$(PREP) $(CC) -o jacobi2d.scal $< $(CFLAGS) $(SCALIB)

clean:
	rm -f *.o jacobi2d charmrun
