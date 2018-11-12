#pragma once
#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>

void qHilbert(
	std::size_t Size, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
);


// Wikipedia implementation
void d2xy(int n, int d, int* x, int* y);
