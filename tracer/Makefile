include Makefile.common

TRACER_LDADD = bigsim/CWrapper.o bigsim/TraceReader.o \
bigsim/entities/PE.o bigsim/entities/Task.o bigsim/entities/MsgEntry.o \
map_messages.o

TRACER_LDADD += ${CODES_BASE_LIBS} ${CHARM_LIBS}

TRACER_CFLAGS = ${CODES_NET_CFLAGS} ${CODES_BASE_CFLAGS}

all: traceR
.PHONY: components

traceR: bigsim-driver.o components map_messages.o
	$(CXX) ${LDFLAGS} $< -o $@ ${TRACER_LDADD}

bigsim-driver.o: bigsim-driver.c
	$(CC) $(CFLAGS) -Ibigsim $(TRACER_CFLAGS) -c $< -o $@

map_messages.o: map_messages.C
	$(SEQ_CXX) ${CFLAGS} ${TRACER_CFLAGS} -c $< -o $@

components:
	cd bigsim; make;

clean:
	rm -rf *.o traceR
	cd bigsim; make clean;
