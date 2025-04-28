#pragma once

#include "quadblock.h"
#include "bsp.h"
#include "geo.h"

#include <vector>

class BitMatrix
{
public:
	BitMatrix() {};
	BitMatrix(size_t width, size_t height) : m_width(width), m_height(height), m_data(width * height) {}

	bool Get(size_t x, size_t y) const;
	size_t GetWidth() const;
	size_t GetHeight() const;
	void Set(bool value, size_t x, size_t y);

private:
	size_t m_width;
	size_t m_height;
	std::vector<bool> m_data;
};

BitMatrix viztree_method_1(const std::vector<Quadblock>& quadblocks, const std::vector<const BSP*>& leaves);