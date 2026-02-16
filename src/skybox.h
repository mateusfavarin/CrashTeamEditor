#pragma once

#include "geo.h"
#include "psx_types.h"

#include <filesystem>
#include <string>
#include <cstdint>
#include <vector>

class SkyboxConfig
{
public:
	// OBJ file path
	std::filesystem::path m_objPath;

	// Loaded geometry
	std::vector<Point> m_vertices;
	std::vector<uint16_t> m_indexBuffer; // 4 uint16_t per face: 3 byte offsets + padding (0)

	// Load skybox geometry from OBJ file (vertices with vertex colors, triangle faces)
	bool LoadOBJ(const std::filesystem::path& path);

	// Serialize skybox data to a binary buffer for .lev export
	// Returns: serialized bytes (Skybox header + vertex array + face index arrays)
	// Fills ptrMapOffsets with the file offsets of all pointer fields that need pointer map entries
	// baseOffset is the file offset where this data block will be written
	std::vector<uint8_t> Serialize(size_t baseOffset, std::vector<size_t>& ptrMapOffsets) const;

	// Generate render geometry for the skybox, scaled and centered relative to level bounds
	std::vector<Primitive> ToGeometry(const BoundingBox& levelBounds) const;

	bool IsReady() const;
	bool RenderUI();
	void Clear();

private:
	// Get the number of face segments and distribute indices into up to 8 segments
	// Each segment contains indices (4 uint16_t per face: 3 byte offsets + padding)
	void DistributeFaces(std::vector<std::vector<uint16_t>>& segments) const;
};
