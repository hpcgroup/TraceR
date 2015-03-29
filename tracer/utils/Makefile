CC = g++

all: genMapping

genMapping: genMapping.C
	$(CC) -O3 -o genMapping $<

trace: genMapping.C
	$(CC) -o genMapping.trace $< $(CFLAGS) $(TRACE_LIB)

scal: genMapping.C
	$(PREP) $(CC) -o genMapping.scal $< $(CFLAGS) $(SCALIB)

clean:
	rm -f *.o genMapping
