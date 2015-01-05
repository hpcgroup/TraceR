include Makefile.common

testlib = ${CODES_NET}/src/libcodes-net.a

ifeq ($(ARCH),bgq)
ARCH_FLAGS = -DROSS_CLOCK_bgq=1
else
ARCH_FLAGS = -Wall -DROSS_CLOCK_amd64=1
endif

CODES_BASE_CFLAGS = -D_GNU_SOURCE -g $(ARCH_FLAGS) -DROSS_OPTION_LIST= -DUSE_AVL_TREE -DAVL_NODE_COUNT=262144 -DRAND_NORMAL -DROSS_timing -DROSS_QUEUE_splay -DROSS_RAND_clcg4 -DROSS_NETWORK_mpi -DROSS_GVT_mpi_allreduce -DARCH_x86_64 -DUSE_AVL_TREE -DAVL_NODE_COUNT=262144 -DRAND_NORMAL -DROSS_timing -DROSS_QUEUE_splay -DROSS_RAND_clcg4 -DROSS_NETWORK_mpi -DROSS_GVT_mpi_allreduce -DARCH_x86_64 -I${CODES_BASE}/install/include -I${ROSS}/install/include

CODES_NET_CFLAGS = -I${CODES_NET}/src -I${CODES_NET}/install/include

modelnet_test_bigsim_LDADD = bigsim/CWrapper.o bigsim/TraceReader.o bigsim/entities/PE.o bigsim/entities/Task.o bigsim/entities/MsgEntry.o $(testlib) ${CODES_BASE_LIBS} -L$(CHARMPATH)/lib -lconv-bigsim-logs -lblue-standalone -lconv-util

modelnet_test_bigsim_CFLAGS = ${CODES_NET_CFLAGS} ${CODES_BASE_CFLAGS} 

all: modelnet_test_bigsim
.PHONY: components

modelnet_test_bigsim: modelnet_test_bigsim.o components 
	$(CXX) modelnet_test_bigsim.o ${modelnet_test_bigsim_LDADD} -o modelnet_test_bigsim

modelnet_test_bigsim.o: modelnet-test-bigsim.c
	$(CC) $(CFLAGS) -Ibigsim $(modelnet_test_bigsim_CFLAGS) -c modelnet-test-bigsim.c $(CFLAGS) -o $@

components:
	cd bigsim; make;

clean:
	rm -rf *.o modelnet-test-bigsim
	cd bigsim; make clean;
