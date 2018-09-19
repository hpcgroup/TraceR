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

include ../Makefile.common

RM := rm -rf

CPP_SRCS = TraceReader.C CWrapper.C otf2_reader.C
OBJS = TraceReader.o CWrapper.o otf2_reader.o
CPP_DEPS = TraceReader.d CWrapper.d otf2_reader.d

CXXFLAGS += -I../

reader: ${OBJS}

%.o: %.C
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	${SEQ_CXX} ${CXXFLAGS} -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	${SEQ_CXX} ${CXXFLAGS} -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
endif
	@echo 'Finished building: $<'
	@echo ' '

clean:
	-$(RM) $(OBJS) $(C++_DEPS) $(CC_DEPS) $(CPP_DEPS) $(EXECUTABLES) $(CXX_DEPS) $(C_UPPER_DEPS)
	-@echo ' '
