#pragma once

#include "quadblock.h"
#include "bsp.h"
#include "geo.h"

#include <vector>
#include <cstdint>
#include <mutex>

class BitMatrix
{
public:
	BitMatrix() : m_width(0), m_height(0) {};
	BitMatrix(size_t width, size_t height) : m_width(width), m_height(height), m_data(width * height, 0) {}
	BitMatrix(const BitMatrix& other);
	BitMatrix& operator=(const BitMatrix& other);
	BitMatrix(BitMatrix&& other) noexcept;
	BitMatrix& operator=(BitMatrix&& other) noexcept;

	bool Get(size_t x, size_t y) const;
	size_t GetWidth() const;
	size_t GetHeight() const;
	void Set(bool value, size_t x, size_t y);
	void SetRow(const std::vector<uint8_t>& rowData, size_t y);
	bool IsEmpty() const;
	void Clear();

private:
	size_t m_width;
	size_t m_height;
	std::vector<uint8_t> m_data;
	mutable std::mutex m_mutex;
};

BitMatrix GenerateVisTree(const std::vector<Quadblock>& quadblocks, const BSP* root, bool simpleVisTree, float minDistance, float maxDistance);
