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

CC	= scorep --user --nocompiler --noopenmp --nopomp --nocuda --noopenacc --noopencl --nomemory mpicxx
OPTS	= -O2 -DWRITE_OTF2_TRACE=1 -I${SCOREP_INSTALL}/include -I${SCOREP_INSTALL}/include/scorep -DSCOREP_USER_ENABLE

all: stencil4d

stencil4d: stencil4d.C
	$(CC) $(OPTS) $(INCS_DEF) -o $@ $<

clean:
	rm -f stencil4d
