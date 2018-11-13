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

#include "TestBed.hpp"

#include <qHilbert.hpp>

#define TRIALCOUNT 10

const std::size_t TestWidth = 32; // Must be power-of-two
std::array<std::uint32_t, TestWidth * TestWidth> Distances;
std::array<glm::u32vec2, Distances.size()> TargetPoints;

void PrintCurve(
	const std::array<glm::u32vec2, Distances.size()>& Positions
)
{
	char Glyphs[TestWidth][TestWidth] = {};
	for( std::size_t i = 1; i < TestWidth * TestWidth - 1; ++i )
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
			) ? "\033[0;32mPASS\033[0m" : "\033[0;31FAIL\033[0m")
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
			) ? "\033[0;32mPASS\033[0m" : "\033[0;31mFAIL\033[0m")
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
		<< (Speedup > 1.0 ? "\033[0;32m":"\033[0;31m")
		<< Speedup
		<< "\033[0m"
		<< std::endl;

	return EXIT_SUCCESS;
}
