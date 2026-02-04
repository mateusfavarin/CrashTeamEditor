#pragma once

#include "quadblock.h"
#include "bsp.h"
#include "geo.h"

#include <vector>
#include <cstdint>

struct VisTreeSettings
{
	bool centerOnlySamples;
	bool commutativeRays;
	float nearClipDistance;
	float farClipDistance;
	VisTreeSettings() : centerOnlySamples(false), commutativeRays(false), nearClipDistance(-1.0f), farClipDistance(1000.0f) {}
};

class BitMatrix
{
public:
	BitMatrix() : m_width(0), m_height(0) {};
	BitMatrix(size_t width, size_t height) : m_width(width), m_height(height), m_data(width * height, 0) {}

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
};

BitMatrix GenerateVisTree(const std::vector<Quadblock>& quadblocks, const BSP* root, const VisTreeSettings& settings);
