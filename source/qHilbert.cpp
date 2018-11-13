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

std::size_t Log2( std::size_t Value )
{
#ifdef _MSC_VER
	std::uint32_t Depth = 0;
	_BitScanForward(
		reinterpret_cast<unsigned long*>(&Depth),
		static_cast<std::uint32_t>(Value)
	);
#else
	const std::size_t Depth = __builtin_clz(Value) - 1;
#endif
	return Depth;
}

enum SIMDSize
{
	Serial  = 0,
	Size2   = 1,
	Size4   = 2,
	Size8   = 3,
	Size16  = 4,
	Size32  = 5,
	Size64  = 6,
	Size128 = 7,
	SizeMax
};

template<std::size_t Width>
inline void qHilbert(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	qHilbert<Width-1>(
		Size,
		Distances,
		Positions,
		Count
	);
}

// Serial

#if defined(__BMI2__) || defined(_MSC_VER) // BMI2
template<>
inline void qHilbert<SIMDSize::Serial>(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	const std::size_t Depth = Log2(Size);
	for( std::size_t i = 0; i < Count; ++i )
	{
		std::uint32_t s = Distances[i];
		s |= 0x55555555 << 2 * Depth;

		const std::uint32_t sr = (s >> 1) & 0x55555555;
		std::uint32_t cs = ((s & 0x55555555) + sr) ^ 0x55555555;

		cs ^= ( cs >> 2);
		cs ^= ( cs >> 4);
		cs ^= ( cs >> 8);
		cs ^= ( cs >> 16);

		const std::uint32_t Swap = cs & 0x55555555;
		const std::uint32_t Comp = (cs >> 1) & 0x55555555;

		std::uint32_t t = (s & Swap) ^ Comp;
		s ^= sr ^ t ^ (t << 1);

		Positions[i].x = _pext_u32( s, 0xAAAA );
		Positions[i].y = _pext_u32( s, 0x5555 );
	}
}
#else // Native
template<>
inline void qHilbert<SIMDSize::Serial>(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	const std::size_t Depth = Log2(Size);
	for( std::size_t i = 0; i < Count; ++i )
	{
		// Parallel prefix method, from Hacker's Delight, Pg. 365
		std::uint32_t s = Distances[i];
		s |= 0x55555555 << 2 * Depth;

		const std::uint32_t sr = (s >> 1) & 0x55555555;
		std::uint32_t cs = ((s & 0x55555555) + sr) ^ 0x55555555;

		cs ^= ( cs >> 2);
		cs ^= ( cs >> 4);
		cs ^= ( cs >> 8);
		cs ^= ( cs >> 16);

		const std::uint32_t Swap = cs & 0x55555555;
		const std::uint32_t Comp = (cs >> 1) & 0x55555555;

		std::uint32_t t = (s & Swap) ^ Comp;
		s ^= sr ^ t ^ (t << 1);
		s &= ( ( 1 << 2 * Depth ) - 1 );

		t = (s ^ ( s >> 1)) & 0x22222222; s ^= t ^ ( t << 1 );
		t = (s ^ ( s >> 2)) & 0x0C0C0C0C; s ^= t ^ ( t << 2 );
		t = (s ^ ( s >> 4)) & 0x00F000F0; s ^= t ^ ( t << 4 );
		t = (s ^ ( s >> 8)) & 0x0000FF00; s ^= t ^ ( t << 8 );

		Positions[i].x = s >> 16;
		Positions[i].y = s & 0xFFFF;
	}
}
#endif

// Four at a time
#if defined(__SSE4_2__) || defined(_MSC_VER)
template<>
inline void qHilbert<SIMDSize::Size4>(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	std::size_t i = 0;
	const std::size_t Depth = Log2(Size);
	// 4 at a time ( SSE4.2 )
	for( ; i < Count / 4; ++i )
	{
		__m128i s = _mm_loadu_si128(
			reinterpret_cast<const __m128i*>(&Distances[i * 4])
		);
		s = _mm_or_si128( s, _mm_set1_epi32( 0x55555555 << 2 * Depth ) );

		const __m128i sr = _mm_and_si128(
			_mm_srli_epi32( s, 1 ), _mm_set1_epi32( 0x55555555 )
		);

		__m128i cs = _mm_xor_si128(
			_mm_add_epi32(
				_mm_and_si128( s, _mm_set1_epi32( 0x55555555 ) ), sr
			),
			_mm_set1_epi32( 0x55555555 )
		);

		cs = _mm_xor_si128( cs, _mm_srli_epi32( cs,  2 ) );
		cs = _mm_xor_si128( cs, _mm_srli_epi32( cs,  4 ) );
		cs = _mm_xor_si128( cs, _mm_srli_epi32( cs,  8 ) );
		cs = _mm_xor_si128( cs, _mm_srli_epi32( cs, 16 ) );

		const __m128i Swap = _mm_and_si128( cs, _mm_set1_epi32( 0x55555555 ) );
		const __m128i Comp = _mm_and_si128(
			_mm_srli_epi32( cs, 1 ), _mm_set1_epi32( 0x55555555 )
		);

		__m128i t = _mm_xor_si128( _mm_and_si128( s, Swap ), Comp );

		s = _mm_xor_si128(
			s,
			_mm_xor_si128( sr, _mm_xor_si128( t, _mm_slli_epi32( t, 1 ) ) )
		);

		s = _mm_and_si128( s, _mm_set1_epi32( ( 1 << 2 * Depth ) - 1 ) );

		////
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 1 ) ),
			_mm_set1_epi32(0x22222222)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 1 ) ) );
		////
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 2 ) ),
			_mm_set1_epi32(0x0C0C0C0C)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 2 ) ) );
		////
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 4 ) ),
			_mm_set1_epi32(0x00F000F0)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 4 ) ) );
		////
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 8 ) ),
			_mm_set1_epi32(0x0000FF00)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 8 ) ) );

		const __m128i PosX = _mm_srli_epi32( s, 16 );
		const __m128i PosY = _mm_and_si128( s, _mm_set1_epi32(0xFFFF) );

		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Positions[i * 4]),
			_mm_unpacklo_epi32( PosX, PosY )
		);
		_mm_storeu_si128(
			reinterpret_cast<__m128i*>(&Positions[i * 4 + 2]),
			_mm_unpackhi_epi32( PosX, PosY )
		);
	}

	qHilbert<SIMDSize::Size4-1>(
		Size,
		Distances + i * 4,
		Positions + i * 4,
		Count % 4
	);
}
#endif

void qHilbert(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	qHilbert<SIMDSize::SizeMax>(
		Size,
		Distances,
		Positions,
		Count
	);
}


// Wikipedia implementation
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
