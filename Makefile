LLVM_CXX_FLAGS=$(shell llvm-config --cppflags) -UNDEBUG
LDFLAGS=$(shell llvm-config --libs core jit native) $(shell llvm-config --ldflags --libs all) -ltinfo # -lbdd

CFLAGS:=-Wall -O3 -pedantic -g $(CFLAGS)
CXXFLAGS:=$(CFLAGS) $(LLVM_CXX_FLAGS) -I. -std=c++11 -I`pwd`/extlibs/z3-unstable/src/api -I`pwd`/extlibs/z3-unstable/src/api/c++ $(CXXFLAGS)
TARGET=symdivine
OBJS=$(shell find llvmsym toolkit -name '*.cpp' -a ! -name '*bdd*' -a ! -name "*emptystore*" | sed s\/.cpp\$\/.o\/)
HEADERS=$(shell find llvmsym toolkit -name '*.h' ! -name '*.tpp')

.PHONY: all clean cscope z3

all:		$(TARGET)

$(TARGET):	$(TARGET).cpp $(OBJS) $(HEADERS) extlibs/z3-unstable/build/libz3.a
	$(CXX) $(TARGET).cpp $(OBJS) extlibs/z3-unstable/build/libz3.a $(CXXFLAGS) $(LDFLAGS) -lrt -fopenmp -o $(TARGET) 

$(TARGET)_without_z3: $(TARGET).cpp $(OBJS) $(HEADERS)
	$(CXX) $(TARGET).cpp $(OBJS) $(CXXFLAGS) $(LDFLAGS) -lz3 -o $(TARGET) 


metrics:	metrics.cpp $(OBJS) $(HEADERS)
	$(CXX) metrics.cpp $(OBJS) $(LDFLAGS) $(CXXFLAGS) -o metrics

clean:
	-rm $(shell find llvmsym -name '*.o') $(TARGET)

extlibs/z3-unstable/build/libz3.a:
	cd extlibs/z3-unstable && \
		autoconf && \
		./configure --with-python=`which python2` && \
		python2 scripts/mk_make.py && \
		$(MAKE) -C build/
	cd extlibs/z3-unstable/build && find . -name "*.o" | xargs ar rs libz3.a 

cscope:
	rm cscope.*
	find llvmsym -name '*.cpp' -o -name '*.h' > cscope.files
	cscope -b -q -k

