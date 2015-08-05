CHARMC ?=  $(HOME)/charm/bin/charmc

all: traceToFile

traceToFile: traceToFile.cpp 
	$(CHARMC) $< -seq -o $@ -lconv-bigsim-logs -lconv-util -lblue-standalone -language c++

clean:
	rm -rf traceToFile
