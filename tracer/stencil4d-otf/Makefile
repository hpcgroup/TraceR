CC	= scorep --user --nocompiler --noopenmp --nopomp --nocuda --noopenacc --noopencl --nomemory mpicxx
OPTS	= -O2 -DWRITE_OTF2_TRACE=1 -I${SCOREP_INSTALL}/include -I${SCOREP_INSTALL}/include/scorep -DSCOREP_USER_ENABLE

all: stencil4d

stencil4d: stencil4d.C
	$(CC) $(OPTS) $(INCS_DEF) -o $@ $<

clean:
	rm -f stencil4d
