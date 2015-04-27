CXX := g++

OPT_LVL := -O0
DBG_LVL := -g3

N_JOBS  := $(shell echo $$((2 * `grep -c "^processor" /proc/cpuinfo`)))

LDFLAGS := -lz3 -lbdd -lboost_regex -lboost_graph $(shell llvm-config --libs core irreader) $(shell llvm-config --ldflags) -ltinfo -ldl -lpthread
INCLUDE := -I`pwd`/extlibs/z3-unstable/src/api -I`pwd`/extlibs/z3-unstable/src/api/c++ -I`pwd`/libs
CFLAGS := -UNDEBUG-Wall -pedantic -g $(OPT_LVL) $(DBG_LVL)
CXXFLAGS := --std=c++11 $(shell llvm-config --cppflags) $(CFLAGS) -I. $(INCLUDE)
TARGET := symdivine
TESTS := tests

PROJECT_DIRS := llvmsym toolkit libs
OBJS := $(shell find $(PROJECT_DIRS) -name '*.cpp' | sed s\/.cpp\$\/.o\/)
OBJS := $(filter-out $(TARGET).cpp,$(OBJS))
DEPENDS := $(OBJS:.o=.d)
DEPENDS += $(TARGET).d

TEST_DIRS := unit_tests
TEST_OBJS := $(shell find $(TEST_DIRS) -name '*.cpp' | sed s\/.cpp\$\/.o\/)
TEST_DEPENDS := $(TEST_OBJS:.o=.d)
TEST_DEPENDS += $(TESTS).d

TO_DEL_O := $(addsuffix /*.o, $(PROJECT_DIRS))
TO_DEL_D := $(addsuffix /*.d, $(PROJECT_DIRS))

# re-expand variables in subsequent rules
.SECONDEXPANSION:

.PHONY: all clean

pall:
	make -j$(N_JOBS) all

all: $(TARGET) $(TESTS)

# include compiler-generated dependencies, so obj files get recompiled when
# their header changes
-include $(DEPENDS)
-include $(TEST_DEPENDS)

#This is the rule for creating the dependency files
%.d: %.cpp
	@echo Creating dependecy for $@
	@$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst %.cpp,%.o,$<)' $< -MF $@

$(TARGET):	$(TARGET).cpp $(OBJS)
	@echo Building main binary: $(TARGET)
	@$(CXX) $(TARGET).cpp $(OBJS) $(CXXFLAGS) -MMD -o $(TARGET) $(LDFLAGS)

$(TESTS): $(TESTS).cpp $(TEST_OBJS)
	@echo Building tests: $(TARGET)
	@$(CXX) $(TESTS).cpp $(TEST_OBJS) $(CXXFLAGS) -MMD -o $(TESTS) $(LDFLAGS)

%.o: %.cpp
	@echo Building $@
	@$(CXX) -c $(CXXFLAGS) -MMD -o $@ $<

clean:
	@echo Cleaning...
	@find . -name "*.o" -type f -delete
	@find . -name "*.d" -type f -delete
	@echo Done.
echo:
	@echo $(DEPENDS)
	@echo $(PROJECT_DIRS)