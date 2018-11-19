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
		std::uint32_t cs = ( (s & 0x55555555) + sr) ^ 0x55555555;

		cs ^= ( cs >> 2);
		cs ^= ( cs >> 4);
		cs ^= ( cs >> 8);
		cs ^= ( cs >> 16);

		const std::uint32_t Swap = cs & 0x55555555;

		const std::uint32_t Comp = (cs >> 1) & 0x55555555;

		std::uint32_t t = (s & Swap) ^ Comp;
		s = s ^ sr ^ t ^ (t << 1);

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

		// Pad the index variable with 0b01 bits
		// 0b01(rather than 0b00) due to the later
		// "notequal" calculation being later inverted
		// Bit addition(+) produces an "and"(carry) and
		// "xor"(sum) bit result
		const std::uint32_t sr = (s >> 1) & 0x55555555;

		// Swap_i = Swap_i+1 ^ (s_2i == s_2i+1)
		// Comp_i = Comp_i+1 ^ (s_2i &  s_2i+1)
		// Vector of complement-swap bits
		// bits of ...cscscscscs
		// The addition is a fast trick to put "&" in the odd bit
		// (the carry bit of bit addition is AND)
		// and XOR(not-equal) in the even bits, the final xor flips the
		// not-equal bits into equal
		//
		//    s_2i+1
		//  s_2i |
		//   V   V
		//   0 + 0 = 0b00 ^ 0b01 = 0b01
		//   0 + 1 = 0b01 ^ 0b01 = 0b00
		//   1 + 0 = 0b01 ^ 0b01 = 0b00
		//   1 + 1 = 0b10 ^ 0b01 = 0b11
		//                           VV
		//                ...cscscscscs
		std::uint32_t cs = ((s & 0x55555555) + sr) ^ 0x55555555;

		// Parallel prefix xor to propagate complement and swap info
		// from left to right
		cs ^= ( cs >> 2);
		cs ^= ( cs >> 4);
		cs ^= ( cs >> 8);
		cs ^= ( cs >> 16);

		// Extract swap bits into even bits
		const std::uint32_t Swap = cs & 0x55555555;

		// Extract complement bits into even bits
		const std::uint32_t Comp = (cs >> 1) & 0x55555555;

		//                        V              t             V
		// X_i = [       s_2i+1 ^ (s_2i & Swap_i+1 )] ^ Comp_i+1
		// Y_i = [s_2i ^ s_2i+1 ^ (s_2i & Swap_i+1 )] ^ Comp_i+1
		std::uint32_t t = (s & Swap) ^ Comp;
		s ^= sr ^ t ^ (t << 1);

		// Mask unused bits
		s &= ( ( 1 << 2 * Depth ) - 1 );

		// Parallel bit extract odd and even bits
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

		// Parallel extract odd and even bits
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 1 ) ),
			_mm_set1_epi32(0x22222222)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 1 ) ) );
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 2 ) ),
			_mm_set1_epi32(0x0C0C0C0C)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 2 ) ) );
		t = _mm_and_si128(
			_mm_xor_si128( s, _mm_srli_epi32( s, 4 ) ),
			_mm_set1_epi32(0x00F000F0)
		);
		s = _mm_xor_si128( s, _mm_xor_si128( t, _mm_slli_epi32( t, 4 ) ) );
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

// Eight at a time
#if defined(__AVX2__) || defined(_MSC_VER)
template<>
inline void qHilbert<SIMDSize::Size8>(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	std::size_t i = 0;
	const std::size_t Depth = Log2(Size);
	for( ; i < Count / 8; ++i )
	{
		__m256i s = _mm256_loadu_si256(
			reinterpret_cast<const __m256i*>(&Distances[i * 8])
		);
		s = _mm256_or_si256( s, _mm256_set1_epi32( 0x55555555 << 2 * Depth ) );

		const __m256i sr = _mm256_and_si256(
			_mm256_srli_epi32( s, 1 ), _mm256_set1_epi32( 0x55555555 )
		);

		__m256i cs = _mm256_xor_si256(
			_mm256_add_epi32(
				_mm256_and_si256( s, _mm256_set1_epi32( 0x55555555 ) ), sr
			),
			_mm256_set1_epi32( 0x55555555 )
		);

		cs = _mm256_xor_si256( cs, _mm256_srli_epi32( cs,  2 ) );
		cs = _mm256_xor_si256( cs, _mm256_srli_epi32( cs,  4 ) );
		cs = _mm256_xor_si256( cs, _mm256_srli_epi32( cs,  8 ) );
		cs = _mm256_xor_si256( cs, _mm256_srli_epi32( cs, 16 ) );

		const __m256i Swap = _mm256_and_si256(
			cs,
			_mm256_set1_epi32( 0x55555555 )
		);
		const __m256i Comp = _mm256_and_si256(
			_mm256_srli_epi32( cs, 1 ), _mm256_set1_epi32( 0x55555555 )
		);

		__m256i t = _mm256_xor_si256( _mm256_and_si256( s, Swap ), Comp );

		s = _mm256_xor_si256(
			s,
			_mm256_xor_si256( sr, _mm256_xor_si256( t, _mm256_slli_epi32( t, 1 ) ) )
		);

		s = _mm256_and_si256( s, _mm256_set1_epi32( ( 1 << 2 * Depth ) - 1 ) );

		// Parallel extract odd and even bits
		t = _mm256_and_si256(
			_mm256_xor_si256( s, _mm256_srli_epi32( s, 1 ) ),
			_mm256_set1_epi32(0x22222222)
		);
		s = _mm256_xor_si256( s, _mm256_xor_si256( t, _mm256_slli_epi32( t, 1 ) ) );
		t = _mm256_and_si256(
			_mm256_xor_si256( s, _mm256_srli_epi32( s, 2 ) ),
			_mm256_set1_epi32(0x0C0C0C0C)
		);
		s = _mm256_xor_si256( s, _mm256_xor_si256( t, _mm256_slli_epi32( t, 2 ) ) );
		t = _mm256_and_si256(
			_mm256_xor_si256( s, _mm256_srli_epi32( s, 4 ) ),
			_mm256_set1_epi32(0x00F000F0)
		);
		s = _mm256_xor_si256( s, _mm256_xor_si256( t, _mm256_slli_epi32( t, 4 ) ) );
		t = _mm256_and_si256(
			_mm256_xor_si256( s, _mm256_srli_epi32( s, 8 ) ),
			_mm256_set1_epi32(0x0000FF00)
		);
		s = _mm256_xor_si256( s, _mm256_xor_si256( t, _mm256_slli_epi32( t, 8 ) ) );

		const __m256i PosX = _mm256_srli_epi32( s, 16 );
		const __m256i PosY = _mm256_and_si256( s, _mm256_set1_epi32(0xFFFF) );

		// There's probably a better way to do this that I forgot about
		//  - Wed Nov 14 22:14:53 PST 2018
		const __m256i InterleaveLo = _mm256_unpacklo_epi32( PosX, PosY );
		const __m256i InterleaveHi = _mm256_unpackhi_epi32( PosX, PosY );

		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Positions[i * 8]),
			_mm256_permute2x128_si256(
				InterleaveLo,
				InterleaveHi,
				0b00'10'00'00
			)
		);
		_mm256_storeu_si256(
			reinterpret_cast<__m256i*>(&Positions[i * 8 + 4]),
			_mm256_permute2x128_si256(
				InterleaveLo,
				InterleaveHi,
				0b00'11'00'01
			)
		);
	}

	qHilbert<SIMDSize::Size8-1>(
		Size,
		Distances + i * 8,
		Positions + i * 8,
		Count % 8
	);
}
#endif

// Sixteen at a time
#if defined(__AVX512F__) || defined(_MSC_VER)
template<>
inline void qHilbert<SIMDSize::Size16>(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
)
{
	// Compile time constants used for vpternlogd
	constexpr std::uint8_t OPA = 0b11110000;
	constexpr std::uint8_t OPB = 0b11001100;
	constexpr std::uint8_t OPC = 0b10101010;

	std::size_t i = 0;
	const std::size_t Depth = Log2(Size);
	for( ; i < Count / 16; ++i )
	{
		__m512i s = _mm512_loadu_si512(
			reinterpret_cast<const __m512i*>(&Distances[i * 16])
		);
		s = _mm512_or_si512( s, _mm512_set1_epi32( 0x55555555 << 2 * Depth ) );

		const __m512i sr = _mm512_and_si512(
			_mm512_srli_epi32( s, 1 ), _mm512_set1_epi32( 0x55555555 )
		);

		__m512i cs = _mm512_xor_si512(
			_mm512_add_epi32(
				_mm512_and_si512( s, _mm512_set1_epi32( 0x55555555 ) ), sr
			),
			_mm512_set1_epi32( 0x55555555 )
		);

		cs = _mm512_xor_si512( cs, _mm512_srli_epi32( cs,  2 ) );
		cs = _mm512_xor_si512( cs, _mm512_srli_epi32( cs,  4 ) );
		cs = _mm512_xor_si512( cs, _mm512_srli_epi32( cs,  8 ) );
		cs = _mm512_xor_si512( cs, _mm512_srli_epi32( cs, 16 ) );

		const __m512i Swap = _mm512_and_si512(
			cs,
			_mm512_set1_epi32( 0x55555555 )
		);
		const __m512i Comp = _mm512_and_si512(
			_mm512_srli_epi32( cs, 1 ), _mm512_set1_epi32( 0x55555555 )
		);

		__m512i t = _mm512_ternarylogic_epi32(
			s,
			Swap,
			Comp,
			(OPA & OPB) ^ OPC
		);

		s = _mm512_ternarylogic_epi32(
			s,
			_mm512_ternarylogic_epi32(
				sr,
				t,
				_mm512_slli_epi32(t, 1),
				OPA ^ OPB ^ OPC
			),
			_mm512_set1_epi32( ( 1 << 2 * Depth ) - 1 ),
			(OPA ^ OPB) & OPC
		);

		// Parallel extract odd and even bits
		t = _mm512_ternarylogic_epi32(
			s,
			_mm512_srli_epi32(s, 1),
			_mm512_set1_epi32(0x22222222),
			(OPA ^ OPB) & OPC
		);
		s = _mm512_ternarylogic_epi32(
			s,
			t,
			_mm512_slli_epi32(t, 1),
			OPA ^ OPB ^ OPC
		);
		t = _mm512_ternarylogic_epi32(
			s,
			_mm512_srli_epi32(s, 2),
			_mm512_set1_epi32(0x0C0C0C0C),
			(OPA ^ OPB) & OPC
		);
		s = _mm512_ternarylogic_epi32(
			s,
			t,
			_mm512_slli_epi32(t, 2),
			OPA ^ OPB ^ OPC
		);
		t = _mm512_ternarylogic_epi32(
			s,
			_mm512_srli_epi32(s, 4),
			_mm512_set1_epi32(0x00F000F0),
			(OPA ^ OPB) & OPC
		);
		s = _mm512_ternarylogic_epi32(
			s,
			t,
			_mm512_slli_epi32(t, 4),
			OPA ^ OPB ^ OPC
		);
		t = _mm512_ternarylogic_epi32(
			s,
			_mm512_srli_epi32(s, 8),
			_mm512_set1_epi32(0x0000FF00),
			(OPA ^ OPB) & OPC
		);
		s = _mm512_ternarylogic_epi32(
			s,
			t,
			_mm512_slli_epi32(t, 8),
			OPA ^ OPB ^ OPC
		);

		const __m512i PosX = _mm512_srli_epi32( s, 16 );
		const __m512i PosY = _mm512_and_si512( s, _mm512_set1_epi32(0xFFFF) );

		// There's probably a better way to do this that I forgot about
		//  - Wed Nov 14 22:14:53 PST 2018
		const __m512i InterleaveLo = _mm512_unpacklo_epi32( PosX, PosY );
		const __m512i InterleaveHi = _mm512_unpackhi_epi32( PosX, PosY );

		_mm512_storeu_si512(
			reinterpret_cast<__m512i*>(&Positions[i * 16]),
			_mm512_permutex2var_epi64(
				InterleaveLo,
				_mm512_set_epi64(8|3,8|2,3,2,8|1,8|0,1,0),
				InterleaveHi
			)
		);
		_mm512_storeu_si512(
			reinterpret_cast<__m512i*>(&Positions[i * 16 + 8]),
			_mm512_permutex2var_epi64(
				InterleaveLo,
				_mm512_set_epi64(8|7,8|6,7,6,8|5,8|4,5,4),
				InterleaveHi
			)
		);
	}

	qHilbert<SIMDSize::Size16-1>(
		Size,
		Distances + i * 16,
		Positions + i * 16,
		Count % 16
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
