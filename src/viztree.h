#pragma once

#include "quadblock.h"
#include "bsp.h"
#include "geo.h"

#include <vector>

class BitMatrix
{
public:
	BitMatrix(size_t width, size_t height) : width(width), height(height), data(width* height) {}

	char& get(size_t x, size_t y);
	bool read(size_t x, size_t y) const;
	BitMatrix transposed() const;
	BitMatrix operator|(const BitMatrix& other) const;
	size_t getWidth() const;
	size_t getHeight() const;

private:
	size_t width, height;
	//std::vector<bool> has a special optimized implementation that packs the bits, however,
	//the "get" function (that returns a ref) wouldn't work as-is. Fix me later probably.
	std::vector<char> data;
};

BitMatrix viztree_method_1(const std::vector<Quadblock>& quadblocks);
BitMatrix quadVizToBspViz(const BitMatrix& quadViz, const std::vector<const BSP*>& leaves);