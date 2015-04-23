LLVM_CXX_FLAGS=$(shell llvm-config --cppflags) -UNDEBUG

CXX := clang++

OPT_LVL := -O0
DBG_LVL := -g3

N_JOBS  := $(shell echo $$((2 * `grep -c "^processor" /proc/cpuinfo`)))

LDFLAGS := $(shell llvm-config --ldflags --libs core irreader) -lz3 -lbdd
INCLUDE := -I`pwd`/extlibs/z3-unstable/src/api -I`pwd`/extlibs/z3-unstable/src/api/c++ -I`pwd`/libs
CFLAGS := -Wall -pedantic -g $(OPT_LVL) $(DBG_LVL) -ferror-limit=5
CXXFLAGS := $(CFLAGS) $(LLVM_CXX_FLAGS) -I. --std=c++11 --stdlib=libc++ $(CXXFLAGS) $(INCLUDE)
TARGET := symdivine

PROJECT_DIRS := llvmsym toolkit libs
OBJS := $(shell find $(PROJECT_DIRS) -name '*.cpp' | sed s\/.cpp\$\/.o\/)
OBJS := $(filter-out $(TARGET).cpp,$(OBJS))
DEPENDS   := $(OBJS:.o=.d)

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
	@rm -rf $(TARGET) $(TO_DEL_D) $(TO_DEL_O)

echo:
	@echo $(PROJECT_DIRS)
	@echo $(TO_DEL_D)