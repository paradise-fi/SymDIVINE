CXX := g++

OPT_LVL := -O0
DBG_LVL := -g3

N_JOBS  := $(shell echo $$((2 * `grep -c "^processor" /proc/cpuinfo`)))

LDFLAGS := -lz3 -lbdd $(shell llvm-config --libs core irreader) $(shell llvm-config --ldflags)
INCLUDE := -I`pwd`/extlibs/z3-unstable/src/api -I`pwd`/extlibs/z3-unstable/src/api/c++ -I`pwd`/libs
CFLAGS := -UNDEBUG-Wall -pedantic -g $(OPT_LVL) $(DBG_LVL)
CXXFLAGS := --std=c++11 $(shell llvm-config --cppflags) $(CFLAGS) -I. $(INCLUDE)
TARGET := symdivine

PROJECT_DIRS := llvmsym toolkit libs
OBJS := $(shell find $(PROJECT_DIRS) -name '*.cpp' | sed s\/.cpp\$\/.o\/)
OBJS := $(filter-out $(TARGET).cpp,$(OBJS))
DEPENDS := $(OBJS:.o=.d)

TO_DEL_O := $(addsuffix /*.o, $(PROJECT_DIRS))
TO_DEL_D := $(addsuffix /*.d, $(PROJECT_DIRS))

# re-expand variables in subsequent rules
.SECONDEXPANSION:

.PHONY: all clean

pall:
	make -j$(N_JOBS) all

all: $(TARGET)

# include compiler-generated dependencies, so obj files get recompiled when
# their header changes
-include $(DEPENDS)

$(TARGET):	$(TARGET).cpp $(OBJS)
	@echo Building main binary: $(TARGET)
	@$(CXX) $(TARGET).cpp $(OBJS) $(CXXFLAGS) -MMD -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	@echo Building $@
	@$(CXX) -c $(CXXFLAGS) -MMD -o $@ $<

clean:
	@echo Cleaning...
	@find . -name "*.o" -type f -delete
	@find . -name "*.d" -type f -delete
	@echo Done.
echo:
	g++ -v
	@echo $(PROJECT_DIRS)