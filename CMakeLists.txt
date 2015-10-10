cmake_minimum_required(VERSION 2.8)
project(SymDIVINE CXX C)

set(CMAKE_VERBOSE_MAKEFILE on)

# Setup packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Import packages
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(Threads REQUIRED)
find_package(LLVM REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM libs: " ${LLVM_LIBRARIES})

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

file(GLOB BIN_SOURCES
	src/llvmsym/*.cpp
	src/llvmsym/formula/*.cpp
	src/llvmsym/llvmwrap/*.cpp
	src/llvmsym/programutils/*.cpp
	src/llvmsym/stanalysis/*.cpp
	src/toolkit/*.cpp
	)

file(GLOB LIB_SOURCES
	libs/docopt/*.cpp
	)

exec_program("llvm-config" ARGS --cppflags OUTPUT_VARIABLE LLVM_FLAGS)
exec_program("llvm-config" ARGS --libs all OUTPUT_VARIABLE LLVM_LIBS)
exec_program("llvm-config" ARGS --ldflags OUTPUT_VARIABLE LLVM_LD_FLAGS)


bison_target(LTL_PARSER src/parsers/ltl_parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
flex_target(LTL_SCANNER src/parsers/ltl_tokens.l ${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp COMPILE_FLAGS --header-file=${CMAKE_CURRENT_BINARY_DIR}/ltl_tokens.h)

# include directories
add_flex_bison_dependency(LTL_SCANNER LTL_PARSER)
include_directories(src extlibs/z3/src/api /extlibs/z3/src/api/c++ libs)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# link directories
link_directories(${LLVM_LIBRARY_DIRS})

add_executable(symdivine src/symdivine.cpp ${BIN_SOURCES} ${LIB_SOURCES}
	${BISON_LTL_PARSER_OUTPUTS} ${FLEX_LTL_SCANNER_OUTPUTS})

target_link_libraries(symdivine ${FLEX_LIBRARIES} ${BISON_LIBRARIES})
target_link_libraries(symdivine ${LLVM_LIBRARIES})
target_link_libraries(symdivine ${CMAKE_THREAD_LIBS_INIT})


set_property(TARGET symdivine PROPERTY CXX_STANDARD 11)
set_property(TARGET symdivine PROPERTY CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lz3 -lboost_regex -lboost_graph ${LLVM_LD_FLAGS} -ltinfo -ldl")
