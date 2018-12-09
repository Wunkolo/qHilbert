#pragma once
#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>

void qHilbert2D(
	std::size_t Order,
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
);


// Wikipedia implementation
void d2xy(int n, int d, int* x, int* y);
