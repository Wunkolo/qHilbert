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

// With a 2D Hilbert curve, every 2 bits of a word maps up
// to an (x,y) transformation. So a 32-bit index value
// can support a maximum "depth" of 16 before truncation
const std::uint32_t TestOrder = 7;

static_assert(
	TestOrder <= 16,
	"Recursion depths beyond 16 not supported for 32-bit indexes"
);

const std::uint32_t TestWidth = 1U << TestOrder;
const std::uint64_t TestArea = TestWidth * static_cast<std::uint64_t>(TestWidth);
std::vector<std::uint32_t> Distances(TestArea);
std::vector<glm::u32vec2> TargetPoints(TestArea);

void PrintCurve(
	const std::vector<glm::u32vec2>& Positions
)
{
	wchar_t Glyphs[TestWidth][TestWidth] = {};
	for( std::size_t i = 0; i < TestArea; ++i )
	{
		const glm::i32vec2& CurPoint = glm::i32vec2(Positions[i]);
		const glm::i32vec2 In  = i != 0 ? 
			glm::i32vec2(Positions[ i - 1 ]) - CurPoint
			:
			glm::i32vec2(0);
		const glm::i32vec2 Out = i != TestWidth * TestWidth ? 
			CurPoint - glm::i32vec2(Positions[ i + 1 ])
			:
			glm::i32vec2(0);
		glm::i8vec2 Delta = glm::sign(In + Out);
		const wchar_t GlyphLUT[3][3] = {
			{L'┓',L'┃',L'┏'},
			{L'━',L' ',L'━'},
			{L'┛',L'┃',L'┗'},
		};
		Glyphs[Positions[i].y][Positions[i].x]
			= GlyphLUT[Delta.y + 1][Delta.x + 1];
	}

	for( std::size_t i = 0; i < TestWidth; ++i )
	{
		std::wprintf(
			L"|%.*ls|\n",
			TestWidth,
			Glyphs[i]
		);
	}
}

std::chrono::nanoseconds WikiBench()
{
	/// Wikipedia
	std::vector<glm::u32vec2> PointsInt(Distances.size());
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
	// PrintCurve(PointsInt);
	std::cout
		<< "d2xy\t\t"
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
	std::vector<glm::u32vec2> Positions(Distances.size());
	const auto qHilbertProc = 
	[&Positions]()
	{
		qHilbert(
			TestOrder,
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
	// PrintCurve(Positions);
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
	std::cout << std::fixed;
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
	std::cout << GetProcessorModel() << std::endl;
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
