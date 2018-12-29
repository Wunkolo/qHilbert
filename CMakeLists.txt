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
target_link_libraries(
	qHilbert
	PUBLIC
	glm
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



# Generic
add_executable(
	Benchmark-generic
	tests/benchmark.cpp
)
target_link_libraries(
	Benchmark-generic
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark-generic
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark-generic
	COMMAND Benchmark-generic
)

# Native
add_executable(
	Benchmark-native
	tests/benchmark.cpp
)
target_compile_options(
	Benchmark-native
	PRIVATE
	-mbmi2
)
target_link_libraries(
	Benchmark-native
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark-native
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark-native
	COMMAND Benchmark-native
)

# bmi2
add_executable(
	Benchmark-bmi2
	tests/benchmark.cpp
)
target_compile_options(
	Benchmark-bmi2
	PRIVATE
	-mbmi2
)
target_link_libraries(
	Benchmark-bmi2
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark-bmi2
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark-bmi2
	COMMAND Benchmark-bmi2
)

# SSE4.2
add_executable(
	Benchmark-sse42
	tests/benchmark.cpp
)
target_compile_options(
	Benchmark-sse42
	PRIVATE
	-mbmi2
	-msse4.2
)
target_link_libraries(
	Benchmark-sse42
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark-sse42
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark-sse42
	COMMAND Benchmark-sse42
)

# AVX2
add_executable(
	Benchmark-avx2
	tests/benchmark.cpp
)
target_compile_options(
	Benchmark-avx2
	PRIVATE
	-mbmi2
	-msse4.2
	-mavx2
)
target_link_libraries(
	Benchmark-avx2
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark-avx2
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark-avx2
	COMMAND Benchmark-avx2
)

# AVX512
add_executable(
	Benchmark-avx512
	tests/benchmark.cpp
)
target_compile_options(
	Benchmark-avx512
	PRIVATE
	-mbmi2
	-msse4.2
	-mavx2
	-mavx512f
)
target_link_libraries(
	Benchmark-avx512
	PRIVATE
	qHilbert
)
set_target_properties(
	Benchmark-avx512
	PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS ON
)

add_test(
	NAME Benchmark-avx512
	COMMAND Benchmark-avx512
)