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

// AVX2
#include <immintrin.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif


#define TRIALCOUNT 10'000

template< typename TimeT = std::chrono::nanoseconds >
struct Measure
{
	template< typename F, typename ...Args >
	static typename TimeT::rep Execute(F&& func, Args&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
		auto duration = std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - start
		);
		return duration.count();
	}

	template< typename F, typename ...Args >
	static TimeT Duration(F&& func, Args&&... args)
	{
		auto start = std::chrono::high_resolution_clock::now();
		std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
		return std::chrono::duration_cast<TimeT>(
			std::chrono::high_resolution_clock::now() - start
		);
	}
};

template< typename T >
struct Vector2
{
	T X, Y;
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


// qHilbert
inline void qHilbertSerial(
	std::size_t Width, // Must be a power of 2
	const std::uint32_t Distance,
	Vector2<std::uint32_t>& Position
)
{
	std::size_t CurDistance = Distance;
	Position.X = Position.Y = 0;
	for( std::size_t Level = 1; Level < Width; Level *= 2 )
	{
		// find out what quadrant T is in
		const std::uint8_t RegionX = 0b1 & (CurDistance / 2);
		const std::uint8_t RegionY = 0b1 & (CurDistance ^ RegionX);
		// Add a flip to our current XY
		if( RegionY == 0 )
		{
			if( RegionX == 1 )
			{
				Position.X = Level - 1 - Position.X;
				Position.Y = Level - 1 - Position.Y;
			}
			//Swap x and y
			std::swap(Position.X, Position.Y);
		}
		// "Move" the XY ahead
		Position.X += Level * RegionX;
		Position.Y += Level * RegionY;
		CurDistance /= 4;
	}
}

void qHilbert(
	std::size_t Width, // Must be power of 2
	const std::uint32_t Distances[],
	Vector2<std::uint32_t> Positions[],
	std::size_t Count
)
{
	std::size_t Index = 0;
#ifdef _MSC_VER
	std::uint32_t Depth;
	_BitScanReverse(
		reinterpret_cast<unsigned long*>(&Depth),
		Width
	);
#else
	const std::uint32_t Depth = __builtin_clz(Width) - 1;
#endif

	/// 8 at a time ( AVX 2 )
	/*
	for( std::size_t i = 0; i < Count / 8; ++i )
	{
		__m256i PositionsX = _mm256_setzero_si256();
		__m256i PositionsY = _mm256_setzero_si256();
		// Four distances at a time
		__m256i CurDistances = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(&Distances[Index])
		);
		__m256i Levels = _mm256_set1_epi32(1);
		for( std::size_t j = 0; j < Depth; ++j )
		{
			// Constants
			const __m256i LevelBound = _mm256_sub_epi32(Levels, _mm256_set1_epi32(1));
			// Regions
			// RegionsX = 1 & CurDistances / 2
			const __m256i RegionsX = // RegionX
				_mm256_and_si256(
					_mm256_srli_epi32(CurDistances, 1), // / 2
					_mm256_set1_epi32(1) // & 0b1
				);
			// RegionsY = 1 & CurDistances ^ RegionsX
			const __m256i RegionsY =
				_mm256_and_si256(
					_mm256_xor_si256(
						// Distance ^ RegionX
						CurDistances,
						RegionsX
					),
					_mm256_set1_epi32(1) // & 0b1
				);

			const __m256i RegXOne = // bitmask, RegX[i] == 1
				_mm256_cmpeq_epi32(
					RegionsX,
					_mm256_set1_epi32(1)
				);
			const __m256i RegYZero = // bitmask, RegY[i] == 0
				_mm256_cmpeq_epi32(
					RegionsY,
					_mm256_setzero_si256()
				);

			// Flip, if RegY[i] == 0 && RegX[i] == 1
			// bitmask, RegY[i] == 0 && RegX[i] == 1
			const __m256i FlipMask = _mm256_and_si256(
				RegXOne,
				RegYZero
			);

			// Calculate Flipped X and Y
			// Level - 1 - PositionX
			// Level - 1 - PositionY
			const __m256i FlippedX = _mm256_sub_epi32(
				LevelBound,
				PositionsX
			);
			const __m256i FlippedY = _mm256_sub_epi32(
				LevelBound,
				PositionsY
			);

			// Write flipped positions, where needed
			PositionsX = _mm256_blendv_epi8(
				PositionsX,
				FlippedX,
				FlipMask
			);
			PositionsY = _mm256_blendv_epi8(
				PositionsY,
				FlippedY,
				FlipMask
			);

			// Swap X and Y, where RegYZero is true
			const __m256i SwappedX = _mm256_blendv_epi8(
				PositionsX,
				PositionsY,
				RegYZero
			);
			const __m256i SwappedY = _mm256_blendv_epi8(
				PositionsY,
				PositionsX,
				RegYZero
			);
			PositionsX = SwappedX;
			PositionsY = SwappedY;

			// Increment Positions
			// PosX += Level * RegionsX
			// PosY += Level * RegionsY
			PositionsX = _mm256_add_epi32(
				PositionsX,
				_mm256_mullo_epi32(
					Levels,
					RegionsX
				)
			);
			PositionsY = _mm256_add_epi32(
				PositionsY,
				_mm256_mullo_epi32(
					Levels,
					RegionsY
				)
			);
			// CurDistance = Dist / 4
			CurDistances = _mm256_srli_epi32(CurDistances, 2);
			// Levels *= 2
			Levels = _mm256_slli_epi32(Levels, 1);
		}
		// ((X,Y),(0,0)),((X,Y),(0,0))
		// 
		const __m256i InterleavedLo = _mm256_unpacklo_epi32(
			PositionsX,
			PositionsY
		);
		const __m256i InterleavedHi = _mm256_unpackhi_epi32(
			PositionsX,
			PositionsY
		);

		const __m256i PermuteFirst = _mm256_permute2x128_si256(
			InterleavedLo,
			InterleavedHi,
			0b0010'0000
		);
		const __m256i PermuteSecond = _mm256_permute2x128_si256(
			InterleavedLo,
			InterleavedHi,
			0b0011'0001
		);
		// store first four ((xy,xy),(xy,xy))
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Positions[Index]),
			PermuteFirst
		);
		// store secnd four ((xy,xy),(xy,xy))
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Positions[Index + 4]),
			PermuteSecond
		);
		Index += 8;
	}
	*/
	/// 4 at a time ( SSE4.2 )
	for( std::size_t i = 0; i < Count / 4; ++i )
	{
		__m128i PositionsX = _mm_setzero_si128();
		__m128i PositionsY = _mm_setzero_si128();
		// Four distances at a time
		__m128i CurDistances = _mm_loadu_si128(
			reinterpret_cast<const __m128i*>(&Distances[Index])
		);
		__m128i Levels = _mm_set1_epi32(1);
		for( std::size_t j = 0; j < Depth; ++j )
		{
			// Constants
			const __m128i LevelBound = _mm_sub_epi32(Levels, _mm_set1_epi32(1));
			/// Regions
			// RegionsX = 1 & CurDistances / 2
			const __m128i RegionsX =
				_mm_and_si128(
					_mm_srli_epi32(CurDistances, 1),
					_mm_set1_epi32(1)
				);
			// RegionsY = 1 & CurDistances ^ RegionsX
			const __m128i RegionsY =
				_mm_and_si128(
					_mm_xor_si128(
						CurDistances,
						RegionsX
					),
					_mm_set1_epi32(1)
				);

			const __m128i RegXOne = // bitmask, RegX[i] == 1
				_mm_cmpeq_epi32(
					RegionsX,
					_mm_set1_epi32(1)
				);
			const __m128i RegYZero = // bitmask, RegY[i] == 0
				_mm_cmpeq_epi32(
					RegionsY,
					_mm_setzero_si128()
				);

			// Flip, if RegY[i] == 0 && RegX[i] == 1
			// bitmask, RegY[i] == 0 && RegX[i] == 1
			const __m128i FlipMask = _mm_and_si128(
				RegXOne,
				RegYZero
			);

			// Calculate Flipped X and Y
			// Level - 1 - PositionX
			// Level - 1 - PositionY
			const __m128i FlippedX = _mm_sub_epi32(
				LevelBound,
				PositionsX
			);
			const __m128i FlippedY = _mm_sub_epi32(
				LevelBound,
				PositionsY
			);

			// Write flipped positions, where needed
			PositionsX = _mm_blendv_epi8(
				PositionsX,
				FlippedX,
				FlipMask
			);
			PositionsY = _mm_blendv_epi8(
				PositionsY,
				FlippedY,
				FlipMask
			);

			// Swap X and Y, where RegYZero is true
			const __m128i SwappedX = _mm_blendv_epi8(
				PositionsX,
				PositionsY,
				RegYZero
			);
			const __m128i SwappedY = _mm_blendv_epi8(
				PositionsY,
				PositionsX,
				RegYZero
			);
			PositionsX = SwappedX;
			PositionsY = SwappedY;

			// Increment Positions
			// PosX += Level * RegionsX
			// PosY += Level * RegionsY
			PositionsX = _mm_add_epi32(
				PositionsX,
				_mm_mullo_epi32(
					Levels,
					RegionsX
				)
			);
			PositionsY = _mm_add_epi32(
				PositionsY,
				_mm_mullo_epi32(
					Levels,
					RegionsY
				)
			);
			// CurDistance = Dist / 4
			CurDistances = _mm_srli_epi32(CurDistances, 2);
			// Levels *= 2
			Levels = _mm_slli_epi32(Levels, 1);
		}
		// Write four points
		// PositionsX = 4 X positions
		// PositionsY = 4 Y positions
		// Write PosX[0],PosY[0],PosX[1],PosY[1] ( 128i )
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Positions[Index]),
			_mm_unpacklo_epi32(PositionsX, PositionsY)
		);
		// Write PosX[2],PosY[2],PosX[3],PosY[3] ( 128i )
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Positions[Index + 2]),
			_mm_unpackhi_epi32(PositionsX, PositionsY)
		);
		Index += 4;
	}
	///

	// Unaligned residue
	for( ; Index < Count; ++Index )
	{
		qHilbertSerial(
			Width,
			Distances[Index],
			Positions[Index]
		);
	}
}

const std::size_t TestWidth = 16; // Must be power-of-two
std::array<std::uint32_t, TestWidth * TestWidth - 1> Distances;
std::array<Vector2<std::uint32_t>, Distances.size()> TargetPoints;
//const std::array<Vector2<std::uint32_t>,Distances.size()> TargetPoints = {{
//	{7,7}, {6,6}, {0,6}, {3,3}, {0,1}, {3,4}
//}};

void WikiTest()
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
				[](const Vector2<std::uint32_t>& A, const Vector2<int>& B) -> bool
				{
					std::printf(
						"(%u,%u)==(%i,%i)\t",
						A.X,
						A.Y,
						B.X,
						B.Y
					);
					return A.X == B.X && A.Y == B.Y;
				}
			)
				? "\nPASS"
				: "\nFAIL")
		<< std::endl;
}

void qHilbertTest()
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
				[](const Vector2<std::uint32_t>& A, const Vector2<std::uint32_t>& B) -> bool
				{
					std::printf(
						"(%u,%u)==(%u,%u)\t",
						A.X,
						A.Y,
						B.X,
						B.Y
					);
					return A.X == B.X && A.Y == B.Y;
				}
			)
				? "\nPASS"
				: "\nFAIL")
		<< std::endl;
}

int main()
{
	std::iota(Distances.begin(), Distances.end(), 0);
	for( std::size_t i = 0; i < Distances.size(); ++i )
	{
		d2xy(
			TestWidth,
			Distances[i],
			reinterpret_cast<int*>(&TargetPoints[i].X),
			reinterpret_cast<int*>(&TargetPoints[i].Y)
		);
	}

	WikiTest();
	qHilbertTest();
	std::cin.ignore();
	return EXIT_SUCCESS;
}
