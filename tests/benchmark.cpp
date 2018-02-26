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

#define TRIALCOUNT 10'000

template< typename TimeT = std::chrono::nanoseconds >
struct Measure
{
	template< typename FunctionT, typename ...ArgsT >
	static TimeT Duration(FunctionT&& Func, ArgsT&&... Arguments)
	{
		// Start time
		const auto Start = std::chrono::high_resolution_clock::now();
		
		// Run function, perfect-forward arguments
		std::forward<decltype(Func)>(Func)(std::forward<ArgsT>(Arguments)...);

		// Return executation time.
		return std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - Start
		);
	}
};

/// Taken from Wikipedia
//rotate/flip a quadrant appropriately
void rot(int n, int* x, int* y, int rx, int ry)
{
	if( ry == 0 )
	{
		if( rx == 1 )
		{
			*x = n - 1 - *x;
			*y = n - 1 - *y;
		}
		//Swap x and y
		int t = *x;
		*x = *y;
		*y = t;
	}
}

//convert d to (x,y)
void d2xy(int n, int d, int* x, int* y)
{
	int rx, ry, s, t = d;
	*x = *y = 0;
	for( s = 1; s < n; s *= 2 )
	{
		rx = 1 & (t / 2);
		ry = 1 & (t ^ rx);
		rot(s, x, y, rx, ry);
		*x += s * rx;
		*y += s * ry;
		t /= 4;
	}
}


const std::size_t TestWidth = 64; // Must be power-of-two
std::array<std::uint32_t, TestWidth * TestWidth - 1> Distances;
std::array<Vector2<std::uint32_t>, Distances.size()> TargetPoints;

template< typename Type1, typename Type2 >
bool Vector2Equal(const Vector2<Type1>& A, const Vector2<Type2>& B)
{
	return A.X == B.X && A.Y == B.Y;
}

std::chrono::nanoseconds WikiBench()
{
	/// Wikipedia
	std::chrono::nanoseconds Duration = std::chrono::nanoseconds::zero();
	std::array<Vector2<int>, Distances.size()> PointsInt;
	for( std::size_t i = 0; i < TRIALCOUNT; ++i )
	{
		auto WikiProc =
			[&PointsInt]()
			{
				for( std::size_t i = 0; i < Distances.size(); ++i )
				{
					d2xy(TestWidth, Distances[i], &PointsInt[i].X, &PointsInt[i].Y);
				}
			};
		Duration += Measure<>::Duration(WikiProc);
	}
	std::cout
		<< "d2xy\t"
		<< Duration.count() / static_cast<std::double_t>(TRIALCOUNT) << "ns" << std::endl;
	std::cout
		<< (std::equal(
				TargetPoints.begin(),
				TargetPoints.end(),
				PointsInt.begin(),
				Vector2Equal<std::uint32_t, int>
			)
				? "PASS"
				: "FAIL")
		<< std::endl;
	return Duration;
}

std::chrono::nanoseconds qHilbertBench()
{
	/// qHilbert
	std::chrono::nanoseconds Duration = std::chrono::nanoseconds::zero();
	std::array<Vector2<std::uint32_t>, Distances.size()> Positions;
	for( std::size_t i = 0; i < TRIALCOUNT; ++i )
	{
		Duration += Measure<>::Duration(
			qHilbert,
			TestWidth,
			Distances.data(),
			Positions.data(),
			Distances.size()
		);
	}
	std::cout
		<< "qHilbert\t"
		<< Duration.count() / static_cast<std::double_t>(TRIALCOUNT) << "ns" << std::endl;
	std::cout
		<< (std::equal(
				TargetPoints.begin(),
				TargetPoints.end(),
				Positions.begin(),
				Vector2Equal<std::uint32_t, std::uint32_t>
			)
				? "PASS"
				: "FAIL")
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
			reinterpret_cast<int*>(&TargetPoints[i].X),
			reinterpret_cast<int*>(&TargetPoints[i].Y)
		);
	}

	const std::chrono::nanoseconds WikiTime = WikiBench();
	const std::chrono::nanoseconds qHilbertTime = qHilbertBench();

	std::cout
		<< "Speedup: "
		<< WikiTime.count() / static_cast<std::double_t>(qHilbertTime.count())
		<< std::endl;

	return EXIT_SUCCESS;
}
