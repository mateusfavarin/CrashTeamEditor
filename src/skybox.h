#pragma once

#include "geo.h"
#include "psx_types.h"

#include <filesystem>
#include <string>
#include <cstdint>
#include <vector>

struct SkyboxConfig
{
	struct SkyboxVertex
	{
		Vec3 pos;
		Color color;
	};

	struct SkyboxFace
	{
		uint16_t a;
		uint16_t b;
		uint16_t c;
	};

	// OBJ file path
	std::filesystem::path objPath;

	// Loaded geometry
	std::vector<SkyboxVertex> vertices;
	std::vector<SkyboxFace> faces;

	// State
	bool enabled = false;

	// Load skybox geometry from OBJ file (vertices with vertex colors, triangle faces)
	bool LoadOBJ(const std::filesystem::path& path);

	// Serialize skybox data to a binary buffer for .lev export
	// Returns: serialized bytes (Skybox header + vertex array + face arrays)
	// Fills ptrMapOffsets with the file offsets of all pointer fields that need pointer map entries
	// baseOffset is the file offset where this data block will be written
	std::vector<uint8_t> Serialize(size_t baseOffset, std::vector<size_t>& ptrMapOffsets) const;

	bool IsReady() const;
	bool RenderUI();
	void Clear();

	// Get the number of face segments and distribute faces into up to 8 segments
	void DistributeFaces(std::vector<std::vector<SkyboxFace>>& segments) const;
};
