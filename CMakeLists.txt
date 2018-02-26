cmake_minimum_required( VERSION 3.2.2 )
project( qHilbert )

### Standard
set( CMAKE_CXX_STANDARD 14 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

### Verbosity
set( CMAKE_COLOR_MAKEFILE ON )
set( CMAKE_VERBOSE_MAKEFILE ON )

### Optimizations
if( MSVC )
	add_compile_options( /arch:AVX2 )
	add_compile_options( /W3 )
elseif( CMAKE_COMPILER_IS_GNUCXX )
	add_compile_options( -O2 )
	add_compile_options( -Wall )
	add_compile_options( -Wextra )
endif()

### Tests
enable_testing()

# Benchmarks

if( CMAKE_COMPILER_IS_GNUCXX )
	message( STATUS  ${CMAKE_SYSTEM_PROCESSOR})
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
	foreach( Bench ${Benches})
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
	# Default target
	add_executable(
		Benchmark
		tests/benchmark.cpp
	)
	target_include_directories(
		Benchmark
		PRIVATE
		include
	)
	
	target_compile_definitions(
		Benchmark
		PRIVATE
		__AVX512F__
		__AVX2__
		__SSE4_2__
	)
	add_test(
		NAME Benchmark
		COMMAND Benchmark
	)
endif()
