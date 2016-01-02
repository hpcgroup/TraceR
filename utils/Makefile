CC = g++

SRCS=$(wildcard *.C)
PGMS=$(SRCS:.C=)

all: $(PGMS)

%: %.C
	$(CC) -O3 -o $@ $<

clean:
	rm -f $(PGMS)
