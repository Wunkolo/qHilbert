#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <iomanip>

#include <algorithm>
#include <numeric>
#include <array>
#include <vector>
#include <functional>

#include <chrono>

#include <qHilbert.hpp>

#define TRIALCOUNT 10

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



const std::size_t TestWidth = 32; // Must be power-of-two
std::array<std::uint32_t, TestWidth * TestWidth> Distances;
std::array<glm::u32vec2, Distances.size()> TargetPoints;

void PrintCurve(
	const std::array<glm::u32vec2, Distances.size()>& Positions
)
{
	char Glyphs[TestWidth][TestWidth] = {};
	for( std::size_t i = 0; i < TestWidth * TestWidth; ++i )
	{
		const glm::u32vec2& CurPoint = Positions[i];
		const glm::u32vec2& In  = Positions[ i - 1 ] - CurPoint;
		const glm::u32vec2& Out = CurPoint - Positions[ i + 1 ];
		Glyphs[CurPoint.y][CurPoint.x] = "-|"[
			In.y & 0b1
		];
	}

	for( std::size_t i = 0; i < TestWidth; ++i )
	{
		std::printf(
			"|%.*s|\n",
			TestWidth,
			Glyphs[i]
		);
	}
}

std::chrono::nanoseconds WikiBench()
{
	/// Wikipedia
	std::array<glm::u32vec2, Distances.size()> PointsInt;
	const auto WikiProc =
	[&PointsInt]()
	{
		for( std::size_t i = 0; i < Distances.size(); ++i )
		{
			d2xy(
				TestWidth,
				Distances[i],
				reinterpret_cast<int*>(&PointsInt[i].x),
				reinterpret_cast<int*>(&PointsInt[i].y)
			);
		}
	};

	std::chrono::nanoseconds Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < TRIALCOUNT; ++i )
	{
		Duration += Measure<>::Duration(WikiProc);
	}
	PrintCurve(PointsInt);
	std::cout
		<< "d2xy\t"
		<< Duration.count() / static_cast<std::double_t>(TRIALCOUNT)
		<< "ns"
		<< std::endl
		<< (std::equal(
				TargetPoints.begin(),
				TargetPoints.end(),
				PointsInt.begin(),
				PointsInt.end()
			) ? "\e[0;32mPASS\e[0m" : "\e[0;31FAIL\e[0m")
		<< std::endl;
	return Duration;
}

std::chrono::nanoseconds qHilbertBench()
{
	/// qHilbert
	std::array<glm::u32vec2, Distances.size()> Positions;
	const auto qHilbertProc = 
	[&Positions]()
	{
		qHilbert(
			TestWidth,
			Distances.data(),
			Positions.data(),
			Distances.size()
		);
	};

	std::chrono::nanoseconds Duration = std::chrono::nanoseconds::zero();
	for( std::size_t i = 0; i < TRIALCOUNT; ++i )
	{
		Duration += Measure<>::Duration(qHilbertProc);
	}
	PrintCurve(Positions);
	std::cout
		<< "qHilbert\t"
		<< Duration.count() / static_cast<std::double_t>(TRIALCOUNT)
		<< "ns"
		<< std::endl
		<< (std::equal(
				TargetPoints.begin(),
				TargetPoints.end(),
				Positions.begin(),
				Positions.end()
			) ? "\e[0;32mPASS\e[0m" : "\e[0;31mFAIL\e[0m")
		<< std::endl;
	return Duration;
}

int main()
{
	// Distance integers from [0,N^2 - 1]
	std::iota(Distances.begin(), Distances.end(), 0);

	// Generate ground-truth variables
	for( std::size_t i = 0; i < Distances.size(); ++i )
	{
		d2xy(
			TestWidth,
			Distances[i],
			reinterpret_cast<int*>(&TargetPoints[i].x),
			reinterpret_cast<int*>(&TargetPoints[i].y)
		);
	}

	const std::chrono::nanoseconds WikiTime = WikiBench();
	const std::chrono::nanoseconds qHilbertTime = qHilbertBench();

	const std::double_t Speedup = WikiTime.count()
		/ static_cast<std::double_t>(qHilbertTime.count());

	std::cout
		<< "Speedup: "
		<< (Speedup > 1.0 ? "\e[0;32m":"\e[0;31m")
		<< Speedup
		<< "\e[0m"
		<< std::endl;

	return EXIT_SUCCESS;
}
