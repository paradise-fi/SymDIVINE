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
OBJS := $(filter-out $(TARGET).o,$(OBJS))
DEPENDS := $(OBJS:.o=.d)
DEPENDS += $(TARGET).d

TEST_DIRS := unit_tests
TEST_OBJS := $(shell find $(TEST_DIRS) -name '*.cpp' | sed s\/.cpp\$\/.o\/)
TEST_DEPENDS := $(TEST_OBJS:.o=.d)
TEST_DEPENDS += $(TESTS).d

PARSERS_DIR := parsers
PARSERS_OBJS := $(shell find $(PARSERS_DIR) -name '*.l' -o -name '*.y' | sed 's/\(.*\.\)l/\1o/' | sed 's/\(.*\.\)y/\1o/')

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

$(TARGET):	$(TARGET).cpp $(OBJS) $(PARSERS_OBJS)
	@echo Building main binary: $(TARGET)
	@$(CXX) $(TARGET).cpp $(OBJS) $(PARSERS_OBJS) $(CXXFLAGS) -MMD -o $(TARGET) $(LDFLAGS)

$(TESTS): $(TESTS).cpp $(TEST_OBJS) $(PARSERS_OBJS) $(OBJS)
	@echo Building tests: $(TARGET)
	@$(CXX) $(TESTS).cpp $(TEST_OBJS) $(PARSERS_OBJS) $(OBJS) $(CXXFLAGS) -MMD -o $(TESTS) $(LDFLAGS)

$(PARSERS_DIR)/ltl_parser.h: $(PARSERS_DIR)/ltl_parser.cpp

$(PARSERS_DIR)/ltl_parser.cpp: $(PARSERS_DIR)/ltl_parser.y $(PARSERS_DIR)/ltl_tokens.h
	@echo Generating ltl_parser
	@bison -d -o $(PARSERS_DIR)/ltl_parser.cpp $(PARSERS_DIR)/ltl_parser.y

$(PARSERS_DIR)/ltl_tokens.h: $(PARSERS_DIR)/ltl_tokens.cpp

$(PARSERS_DIR)/ltl_tokens.cpp: $(PARSERS_DIR)/ltl_tokens.l
	@echo Generating ltl_tokens
	@flex -o $(PARSERS_DIR)/ltl_tokens.cpp --header-file=$(PARSERS_DIR)/ltl_tokens.h $(PARSERS_DIR)/ltl_tokens.l


%.o: %.cpp
	@echo Building $@
	@$(CXX) -c $(CXXFLAGS) -MMD -o $@ $<

clean:
	@echo Cleaning...
	@find . -name "*.o" -type f -delete
	@find . -name "*.d" -type f -delete
	@find $(PARSERS_DIR) -name "*.cpp" -type f -delete
	@find $(PARSERS_DIR) -name "*.h" -type f -delete
	@find $(PARSERS_DIR) -name "*.hpp" -type f -delete
	@echo Done.
echo:
	@echo $(DEPENDS)
	@echo $(PROJECT_DIRS)
	@echo $(PARSERS_OBJS)