include Makefile.common

ifeq ($(ARCH),bgq)
ARCH_FLAGS = -DROSS_CLOCK_bgq=1
else
ARCH_FLAGS = -Wall -DROSS_CLOCK_amd64=1
endif

CODES_BASE_CFLAGS = -D_GNU_SOURCE -g $(ARCH_FLAGS) -DROSS_OPTION_LIST= \
-DUSE_AVL_TREE -DAVL_NODE_COUNT=262144 -DRAND_NORMAL -DROSS_timing \
-DROSS_QUEUE_splay -DROSS_RAND_clcg4 -DROSS_NETWORK_mpi  \
-DROSS_GVT_mpi_allreduce -DARCH_x86_64 -DUSE_AVL_TREE -DAVL_NODE_COUNT=262144 \
-DRAND_NORMAL -DROSS_timing -DROSS_QUEUE_splay -DROSS_RAND_clcg4 \
-DROSS_NETWORK_mpi -DROSS_GVT_mpi_allreduce -DARCH_x86_64 \
-I${CODES_BASE}/install/include -I${ROSS}/include

CODES_NET_CFLAGS = -I${CODES_NET}/install/include

TRACER_LDADD = bigsim/CWrapper.o bigsim/TraceReader.o \
bigsim/entities/PE.o bigsim/entities/Task.o bigsim/entities/MsgEntry.o

TRACER_LDADD += ${CODES_BASE_LIBS} ${CHARM_LIBS}

TRACER_CFLAGS = ${CODES_NET_CFLAGS} ${CODES_BASE_CFLAGS}

all: traceR
.PHONY: components

traceR: bigsim-driver.o components
	$(CXX) ${LDFLAGS} $< -o $@ ${TRACER_LDADD}

bigsim-driver.o: bigsim-driver.c
	$(CC) $(CFLAGS) -Ibigsim $(TRACER_CFLAGS) -c $< -o $@

components:
	cd bigsim; make;

clean:
	rm -rf *.o traceR
	cd bigsim; make clean;
