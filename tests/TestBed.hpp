#pragma once
#include <cstdint>
#include <cstddef>

#include <chrono>

#ifdef _WIN32
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
