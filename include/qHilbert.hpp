#pragma once
#include <cstdint>
#include <cstddef>
#include <glm/glm.hpp>

void qHilbertSerial(
	std::size_t Width, // Must be a power of 2
	std::uint32_t Distance,
	glm::u32vec2& Position
);

void qHilbert(
	std::size_t Width, // Must be power of 2
	const std::uint32_t Distances[],
	glm::u32vec2 Positions[],
	std::size_t Count
);