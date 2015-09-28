CC = g++

all: genMapping dfly_map

genMapping: genMapping.C
	$(CC) -O3 -o $@ $<

dfly_map: genMapping_dfly.C
	$(CC) -O3 -o $@ $<

clean:
	rm -f *.o genMapping dfly_map
