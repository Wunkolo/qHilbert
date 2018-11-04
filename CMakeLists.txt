cmake_minimum_required( VERSION 3.2.2 )
project( qHilbert )

### Standard
set( CMAKE_CXX_STANDARD 14 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

### Optimizations
if( MSVC )
	add_compile_options( /W4 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -O2 )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Libraries

## GLM
option( GLM_TEST_ENABLE "" OFF )
add_subdirectory(
	dependencies/glm
)

### Tests
enable_testing()

# Benchmarks

if( CMAKE_COMPILER_IS_GNUCXX )
	if( ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86" )
		set(
			Benches
			avx512f
			avx2
			sse4.2
		)
	else( ${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm" )
		set(
			Benches
		)
	endif()
	
	# Individual extensions
	foreach( Bench ${Benches})
		add_executable(
			Benchmark-${Bench}
			tests/benchmark.cpp
		)
		target_include_directories(
			Benchmark-${Bench}
			PRIVATE
			include
		)
		target_compile_options(
			Benchmark-${Bench}
			PRIVATE
			-m${Bench}
		)
		add_test(
			NAME Benchmark-${Bench}
			COMMAND Benchmark-${Bench}
		)
	endforeach( Bench )
	
	# All Extensions
	add_executable(
		Benchmark-all
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-all
		PRIVATE
		include
	)
	foreach( Bench ${Benches} )
		target_compile_definitions(
			Benchmark-all
			PRIVATE
			BENCH${Bench}
		)
		target_compile_options(
			Benchmark-all
			PRIVATE
			-m${Bench}
		)
	endforeach( Bench )
	
	add_test(
		NAME Benchmark-all
		COMMAND Benchmark-all
	)
	
	# Native
	add_executable(
		Benchmark-native
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-native
		PRIVATE
		include
	)
	target_compile_options(
		Benchmark-native
		PRIVATE
		-march=native
	)
	add_test(
		NAME Benchmark-native
		COMMAND Benchmark-native
	)
	
	# Base
	add_executable(
		Benchmark-base
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-base
		PRIVATE
		include
	)
	add_test(
		NAME Benchmark-base
		COMMAND Benchmark-base
	)
else()
	# Base
	add_executable(
		Benchmark-base
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-base
		PRIVATE
		include
	)
	add_test(
		NAME Benchmark-base
		COMMAND Benchmark-base
	)

	# SSE4.2
	add_executable(
		Benchmark-sse4.2
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-sse4.2
		PRIVATE
		include
	)
	add_test(
		NAME Benchmark-sse4.2
		COMMAND Benchmark-sse4.2
	)
	target_compile_definitions(
		Benchmark-sse4.2
		PRIVATE
		__SSE4_2__
	)

	# AVX2
	add_executable(
		Benchmark-avx2
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-avx2
		PRIVATE
		include
	)
	add_test(
		NAME Benchmark-avx2
		COMMAND Benchmark-avx2
	)
	target_compile_definitions(
		Benchmark-avx2
		PRIVATE
		__AVX2__
		__SSE4_2__
	)

	# AVX512F
	add_executable(
		Benchmark-avx512f
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark-avx512f
		PRIVATE
		include
	)
	add_test(
		NAME Benchmark-avx512f
		COMMAND Benchmark-avx512f
	)
	target_compile_definitions(
		Benchmark-avx512f
		PRIVATE
		__AVX512F__
		__AVX2__
		__SSE4_2__
	)
endif()
