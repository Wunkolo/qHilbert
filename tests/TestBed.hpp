#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

#include <chrono>

#ifdef _WIN32
#include <intrin.h>
#define NOMINMAX
#include <Windows.h>
// Statically enables "ENABLE_VIRTUAL_TERMINAL_PROCESSING" for the terminal
// at runtime to allow for unix-style escape sequences. 
static const bool _WndV100Enabled = []() -> bool
	{
		const auto Handle = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD ConsoleMode;
		GetConsoleMode(
			Handle,
			&ConsoleMode
		);
		SetConsoleMode(
			Handle,
			ConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
		);
		GetConsoleMode(
			Handle,
			&ConsoleMode
		);
		return ConsoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	}();
std::string GetProcessorModel()
{
	std::string Model;
	Model.reserve(48);

	for( std::size_t i = 0x80000002; i < 0x80000005; ++i )
	{
		std::array<std::uint32_t,4> CPUData;
		__cpuid(
			reinterpret_cast<int*>(&CPUData[0]),
			i
		);
		for( const std::uint32_t& Word : CPUData )
		{
			Model.append(
				reinterpret_cast<const char*>(&Word),
				4
			);
		}
	}

	return Model;
}
#else
#include <cpuid.h>
std::string GetProcessorModel()
{
	std::string Model;
	Model.reserve(48);

	for( std::size_t i = 0x80000002; i < 0x80000005; ++i )
	{
		std::array<std::uint32_t,4> CPUData;
		__get_cpuid(
			i,
			&CPUData[0], &CPUData[1],
			&CPUData[2], &CPUData[3]
		);
		for( const std::uint32_t& Word : CPUData )
		{
			Model.append(
				reinterpret_cast<const char*>(&Word),
				4
			);
		}
	}

	return Model;
}

#endif

template< typename TimeT = std::chrono::nanoseconds >
struct Measure
{
	template< typename FunctionT, typename ...ArgsT >
	static TimeT Duration(FunctionT&& Func, ArgsT&&... Arguments)
	{
		// Start time
		const auto Start = std::chrono::high_resolution_clock::now();
		
		// Run function, perfect-forward arguments
		std::forward<decltype(Func)>(Func)(
			std::forward<ArgsT>(Arguments)...
		);

		// Return executation time.
		return std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - Start
		);
	}
};

