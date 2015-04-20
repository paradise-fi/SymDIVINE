LDFLAGS=-lz3
CPP:=clang++
CFLAGS:=-Wall -pedantic $(CFLAGS) -ferror-limit=5
CXXFLAGS:=$(CFLAGS) -I. -std=c++11 $(CXXFLAGS)
PROG=main
MOD=model

.PHONY: all clean

all: $(MOD)_op.ll $(MOD)_noop.ll

$(MOD)_noop.ll: $(PROG).cpp
	$(CPP) -S -emit-llvm -o $(MOD)_noop.ll $(CXXFLAGS) $(PROG).cpp

$(MOD)_op.ll: $(PROG).cpp
	$(CPP) -S -emit-llvm -O2 -o $(MOD)_op.ll $(CXXFLAGS) $(PROG).cpp

clean:
	rm $(MOD)_op.ll $(MOD)_noop.ll
