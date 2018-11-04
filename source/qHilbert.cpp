#include <qHilbert.hpp>

#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#elif defined(__ARM_NEON)

#include <arm_neon.h>

#else
#endif


// qHilbert
void qHilbertSerial(
	std::size_t Width, // Must be a power of 2
	std::uint32_t Distance,
	glm::u32vec2& Position
)
{
	std::size_t CurDistance = Distance;
	Position.x = Position.y = 0;
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
				Position.x = static_cast<std::uint32_t>(Level - 1 - Position.x);
				Position.y = static_cast<std::uint32_t>(Level - 1 - Position.y);
			}
			//Swap x and y
			std::swap(Position.x, Position.y);
		}
		// "Move" the XY ahead where needed
		Position.x += static_cast<std::uint32_t>(Level * RegionX);
		Position.y += static_cast<std::uint32_t>(Level * RegionY);
		CurDistance /= 4;
	}
}

void qHilbert(
	std::size_t Width, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	std::size_t Index = Count;
#ifdef _MSC_VER
	std::uint32_t Depth;
	_BitScanReverse64(
		reinterpret_cast<unsigned long*>(&Depth),
		static_cast<std::uint64_t>(Width)
	);
#else
	const std::uint32_t Depth = __builtin_clz(Width) - 1;
#endif

#ifdef __AVX512F__
#pragma message "AVX512F Enabled"
	/// 16 at a time ( AVX 512 )
	for( std::size_t i = Index; i < (Count-i) / 16; ++i )
	{
		__m512i PositionsX = _mm512_setzero_si512();
		__m512i PositionsY = _mm512_setzero_si512();
		// Four distances at a time
		__m512i CurDistances = _mm512_loadu_si512(
			reinterpret_cast<const __m512i*>(&Distances[Index])
		);
		__m512i Levels = _mm512_set1_epi32(1);
		for( std::size_t j = 0; j < Depth; ++j )
		{
			// Constants
			const __m512i LevelBound = _mm512_sub_epi32(Levels, _mm512_set1_epi32(1));
			// Regions
			// RegionsX = 1 & CurDistances / 2
			const __m512i RegionsX = // RegionX
				_mm512_and_si512(
					_mm512_srli_epi32(CurDistances, 1),
					// / 2
					_mm512_set1_epi32(1) // & 0b1
				);
			// RegionsY = 1 & CurDistances ^ RegionsX
			const __m512i RegionsY =
				_mm512_and_si512(
					_mm512_xor_si512(
						// Distance ^ RegionX
						CurDistances,
						RegionsX
					),
					_mm512_set1_epi32(1) // & 0b1
				);

			const __mmask16 RegXOne = // bitmask, RegX[i] == 1
				_mm512_cmpeq_epi32_mask(
					RegionsX,
					_mm512_set1_epi32(1)
				);
			const __mmask16 RegYOne = // bitmask, RegY[i] == 1
				_mm512_cmpeq_epi32_mask(
					RegionsY,
					_mm512_set1_epi32(1)
				);
			const __mmask16 RegYZero = // bitmask, RegY[i] == 0
				_mm512_cmpeq_epi32_mask(
					RegionsY,
					_mm512_setzero_si512()
				);

			// Flip, if RegY[i] == 0 && RegX[i] == 1
			// bitmask, RegY[i] == 0 && RegX[i] == 1
			const __mmask16 FlipMask = _mm512_kand(
				RegXOne,
				RegYZero
			);

			// Calculate Flipped X and Y
	// Level - 1 - PositionX
			// Level - 1 - PositionY
			PositionsX = _mm512_mask_sub_epi32(
				PositionsX,
				FlipMask,
				LevelBound,
				PositionsX
			);
			PositionsY = _mm512_mask_sub_epi32(
				PositionsY,
				FlipMask,
				LevelBound,
				PositionsY
			);

			// Swap X and Y, where RegYZero is true
			const __m512i SwappedX = _mm512_mask_mov_epi32(
				PositionsX,
				RegYZero,
				PositionsY
			);
			const __m512i SwappedY = _mm512_mask_mov_epi32(
				PositionsY,
				RegYZero,
				PositionsX
			);
			PositionsX = SwappedX;
			PositionsY = SwappedY;

			// Increment Positions
	// PosX += RegionsX ? Level : 0
			// PosY += RegionsY ? Level : 0
			PositionsX = _mm512_mask_add_epi32(PositionsX,RegXOne,PositionsX,Levels),
			PositionsY = _mm512_mask_add_epi32(PositionsY,RegYOne,PositionsY,Levels),
			// CurDistance = Dist / 4
			CurDistances = _mm512_srli_epi32(CurDistances, 2);
			// Levels *= 2
			Levels = _mm512_slli_epi32(Levels, 1);
		}
		const __m512i InterleavedLo = _mm512_unpacklo_epi32(
			PositionsX,
			PositionsY
		);
		const __m512i InterleavedHi = _mm512_unpackhi_epi32(
			PositionsX,
			PositionsY
		);
		const __m512i PermuteFirst = _mm512_permutex2var_epi64(
			InterleavedLo,
			_mm512_set_epi64(
				0b1011,// Hi[3]
				0b1010,// Hi[2]
				0b0011,// Lo[3]
				0b0010,// Lo[2]
				0b1001,// Hi[1]
				0b1000,// Hi[0]
				0b0001,// Lo[1]
				0b0000 // Lo[0]
			),
			InterleavedHi
		);
		const __m512i PermuteSecond = _mm512_permutex2var_epi64(
			InterleavedLo,
			_mm512_set_epi64(
				0b1111,// Hi[7]
				0b1110,// Hi[6]
				0b0111,// Lo[7]
				0b0110,// Lo[6]
				0b1101,// Hi[5]
				0b1100,// Hi[4]
				0b0101,// Lo[5]
				0b0100 // Lo[4]
			),
			InterleavedHi
		);
		// store first eight ((x,y),(x,y),(x,y),(x,y),(x,y),(x,y),(x,y),(x,y)
		_mm512_storeu_si512(
			reinterpret_cast<__m512i*>(&Positions[Index]),
			PermuteFirst
		);
		// store second eight ((x,y),(x,y),(x,y),(x,y),(x,y),(x,y),(x,y),(x,y)
		_mm512_storeu_si512(
			reinterpret_cast<__m512i*>(&Positions[Index + 8]),
			PermuteSecond
		);
		Index += 16;
	}
#endif

#ifdef __AVX2__
#pragma message "AVX2 Enabled"
	/// 8 at a time ( AVX 2 )
	for( std::size_t i = Index; i < (Count - i) / 8; ++i )
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
			const __m256i RegionsX =
				_mm256_and_si256(
					_mm256_srli_epi32(CurDistances, 1),
					_mm256_set1_epi32(1)
				);
			// RegionsY = 1 & CurDistances ^ RegionsX
			const __m256i RegionsY =
				_mm256_and_si256(
					_mm256_xor_si256(
						CurDistances,
						RegionsX
					),
					_mm256_set1_epi32(1)
				);

			const __m256i RegXOne = // bitmask, RegX[i] == 1
				_mm256_cmpeq_epi32(
					RegionsX,
					_mm256_set1_epi32(1)
				);
			const __m256i RegYOne = // bitmask, RegY[i] == 1
				_mm256_cmpeq_epi32(
					RegionsY,
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
			// PosX += RegionsX ? Level : 0
			// PosY += RegionsY ? Level : 0
			PositionsX = _mm256_blendv_epi8(
				PositionsX,
				_mm256_add_epi32(PositionsX, Levels),
				RegXOne
			);
			PositionsY = _mm256_blendv_epi8(
				PositionsY,
				_mm256_add_epi32(PositionsY, Levels),
				RegYOne
			);
			// CurDistance = Dist / 4
			CurDistances = _mm256_srli_epi32(CurDistances, 2);
			// Levels *= 2
			Levels = _mm256_slli_epi32(Levels, 1);
		}
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
		// store first four ((x,y),(x,y),(x,y),(x,y))
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Positions[Index]),
			PermuteFirst
		);
		// store second four ((x,y),(x,y),(x,y),(x,y))
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Positions[Index + 4]),
			PermuteSecond
		);
		Index += 8;
	}
#endif

#ifdef __SSE4_2__
#pragma message "SSE4.2 Enabled"
	/// 4 at a time ( SSE4.2 )
	for( std::size_t i = Index; i < (Count - i) / 4; ++i )
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
			const __m128i RegYOne = // bitmask, RegY[i] == 1
				_mm_cmpeq_epi32(
					RegionsY,
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
			// PosX += RegionsX ? Level : 0
			// PosY += RegionsY ? Level : 0
			PositionsX = _mm_blendv_epi8(
				PositionsX,
				_mm_add_epi32(PositionsX, Levels),
				RegXOne
			);
			PositionsY = _mm_blendv_epi8(
				PositionsY,
				_mm_add_epi32(PositionsY, Levels),
				RegYOne
			);
			// CurDistance = Dist / 4
			CurDistances = _mm_srli_epi32(CurDistances, 2);
			// Levels *= 2
			Levels = _mm_slli_epi32(Levels, 1);
		}
		// store first two ((xy,xy),(xy,xy))
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Positions[Index]),
			_mm_unpacklo_epi32(PositionsX, PositionsY)
		);
		// store second two ((xy,xy),(xy,xy))
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Positions[Index + 2]),
			_mm_unpackhi_epi32(PositionsX, PositionsY)
		);
		Index += 4;
	}
#endif
	/// ARM ( NEON )

#ifdef __ARM_NEON
#pragma message "NEON Enabled"
	/// 4 at a time ( NEON )
	for( std::size_t i = Index; i < (Count - i) / 4; ++i )
	{
		uint32x4_t PositionsX = {0};
		uint32x4_t PositionsY = {0};
		uint32x4_t CurDistances = vld1q_u32(
			reinterpret_cast<const std::uint32_t*>(&Distances[Index])
		);
		uint32x4_t Levels = vdupq_n_u32(1);
		for( std::size_t j = 0; j < Depth; ++j )
		{
			const uint32x4_t LevelBound = vsubq_u32( Levels, vmovq_n_u32(1) );
			const uint32x4_t RegionsX = vandq_u32(
				vshrq_n_u32( CurDistances, 1 ),
				vdupq_n_u32(1)
			);
			const uint32x4_t RegionsY = vandq_u32(
				veorq_u32(CurDistances,RegionsX),
				vdupq_n_u32(1)
			);

			const uint32x4_t RegXOne = 
				vceqq_u32(
					RegionsX,
					vdupq_n_u32(1)
				);
			const uint32x4_t RegYOne = 
				vceqq_u32(
					RegionsY,
					vdupq_n_u32(1)
				);
			const uint32x4_t RegYZero = 
				vceqq_u32(
					RegionsY,
					vdupq_n_u32(0)
				);

			// Flip, if RegX[i] == 1 and RegY[i] == 0
			const uint32x4_t FlipMask = vandq_u32( RegXOne, RegYZero );
			const uint32x4_t FlippedX = vsubq_u32( LevelBound, PositionsX );
			const uint32x4_t FlippedY = vsubq_u32( LevelBound, PositionsY );
			PositionsX = vbslq_u32( FlipMask, FlippedX, PositionsX );
			PositionsY = vbslq_u32( FlipMask, FlippedY, PositionsY );

			// Swap if RegY[i] == 0
			const uint32x4_t SwappedX = vbslq_u32(
				RegYZero,
				PositionsY,
				PositionsX
			);
			const uint32x4_t SwappedY = vbslq_u32(
				RegYZero,
				PositionsX,
				PositionsY
			);
			PositionsX = SwappedX;
			PositionsY = SwappedY;

			// Integrate Positions
			PositionsX = vbslq_u32(
				RegXOne,
				vaddq_u32(PositionsX,Levels),
				PositionsX
			);
			PositionsY = vbslq_u32(
				RegYOne,
				vaddq_u32(PositionsY,Levels),
				PositionsY
			);

			// CurDistance /= 4
			CurDistances = vshrq_n_u32( CurDistances, 2 );
			// Levels *= 2
			Levels = vshlq_n_u32( Levels, 1);
		}
		// Interleaved
		const uint32x4x2_t Interleaved = vzipq_u32( PositionsX, PositionsY );
		// store interleaved (x,y) pairs
		vst1q_u32(
			reinterpret_cast<uint32_t*>(&Positions[Index]),
			Interleaved.val[0]
		);
		vst1q_u32(
			reinterpret_cast<uint32_t*>(&Positions[Index + 2]),
			Interleaved.val[1]
		);
		Index += 4;
	}
#endif
	// Unaligned
	for( std::size_t i = Index; i < Count; ++i )
	{
		qHilbertSerial(
			Width,
			Distances[i],
			Positions[i]
		);
		Index += 1;
	}
}
