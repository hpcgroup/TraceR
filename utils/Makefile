CC = g++

SRCS=$(wildcard *.C)
PGMS=$(SRCS:.C=)

all: $(PGMS)

%: %.C
	$(CC) -O3 ${EXTRA} -o $@ $<

clean:
	rm -f $(PGMS)
