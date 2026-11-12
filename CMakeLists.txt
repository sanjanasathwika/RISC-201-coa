cmake_minimum_required(VERSION 3.16)
project(risc201_processor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library
add_library(risc201_core
  src/isa.cpp
  src/alu.cpp
  src/memory.cpp
  src/assembler.cpp
  src/cpu.cpp
  src/control.cpp
  src/stack.cpp
  src/branch_predictor.cpp
)
target_include_directories(risc201_core PUBLIC include)

# Main executable
add_executable(risc201 src/main.cpp)
target_link_libraries(risc201 PRIVATE risc201_core)

# Tests
enable_testing()
add_executable(risc201_tests tests/test_main.cpp)
target_link_libraries(risc201_tests PRIVATE risc201_core)
add_test(NAME risc201_tests COMMAND risc201_tests)

# Native Architecture Demonstration Targets (Compiled C++ Targets, Zero PowerShell)
add_custom_target(demonstrate_all
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/risc201.exe" demo
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS risc201 risc201_tests
  COMMENT ">>> RUNNING COMPLETE C++ PROCESSOR ARCHITECTURE DEMONSTRATION <<<"
  USES_TERMINAL
)

add_custom_target(alu_demo
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/risc201.exe" alu-demo
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS risc201
  COMMENT ">>> RUNNING ALU HARDWARE DEMONSTRATION <<<"
  USES_TERMINAL
)

add_custom_target(control_demo
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/risc201.exe" compare programs/factorial.s
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS risc201
  COMMENT ">>> RUNNING CONTROL UNIT BENCHMARK <<<"
  USES_TERMINAL
)

add_custom_target(memory_demo
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/risc201.exe" mem-demo
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS risc201
  COMMENT ">>> RUNNING MEMORY & CACHE DEMONSTRATION <<<"
  USES_TERMINAL
)

add_custom_target(pipeline_demo
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/risc201.exe" compare programs/hazard_test.s
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS risc201
  COMMENT ">>> RUNNING PIPELINE & HAZARDS DEMONSTRATION <<<"
  USES_TERMINAL
)

add_custom_target(run_tests
  COMMAND "${CMAKE_CURRENT_BINARY_DIR}/risc201_tests.exe"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS risc201_tests
  COMMENT ">>> EXECUTING ALL 78 TESTCASES <<<"
  USES_TERMINAL
)

