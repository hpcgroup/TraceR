CC = scorep --user --nocompiler --noopenmp --nopomp --nocuda --noopenacc --noopencl --nomemory mpicxx
OPTS = -O2 -DOTF=1 -I/g/g92/jain6/workspace/scoreP/scorep-3.0/install/include -I/g/g92/jain6/workspace/scoreP/scorep-3.0/install/include/scorep -DSCOREP_USER_ENABLE

all:stencil4d

stencil4d: stencil4d.C
	$(CC) $(OPTS) $(INCS_DEF) -o $@ $<

clean:
	rm -f stencil4d
