cmake_minimum_required( VERSION 3.2.2 )
project( qHilbert )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Optimizations
if( MSVC )
	add_compile_options( /W4 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -Ofast )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Libraries

## GLM
option( GLM_TEST_ENABLE "" OFF )
add_subdirectory(
	dependencies/glm
)

### Targets
add_library(
	qHilbert
	STATIC
	source/qHilbert.cpp
)
target_include_directories(
	qHilbert
	PUBLIC
	include
)
set_target_properties(
	qHilbert
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

### Tests
enable_testing()

# Benchmarks
add_executable(
	Benchmark
	tests/benchmark.cpp
)
target_link_libraries(
	Benchmark
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark
	COMMAND Benchmark
)