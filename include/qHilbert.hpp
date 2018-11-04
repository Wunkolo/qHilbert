#pragma once
#include <cstdint>
#include <cstddef>

template< typename T >
struct Vector2
{
	T X, Y;
};

static_assert(
	sizeof(Vector2<std::uint32_t>) == sizeof(std::uint32_t) * 2,
	"struct 'Vector2<std::uint32_t>' is not packed"
);

// qHilbert
void qHilbertSerial(
	std::size_t Width, // Must be a power of 2
	std::uint32_t Distance,
	Vector2<std::uint32_t>& Position
);

void qHilbert(
	std::size_t Width, // Must be power of 2
	const std::uint32_t Distances[],
	Vector2<std::uint32_t> Positions[],
	std::size_t Count
);