#pragma once

#include "geo.h"
#include "psx_types.h"
#include "texture.h"

#include <filesystem>
#include <string>
#include <cstdint>
#include <vector>

class Quadblock;

struct MinimapConfig
{
	// World coordinate bounds (fixed-point, FP_ONE_GEO = 64)
	int16_t worldEndX = 0;
	int16_t worldEndY = 0;
	int16_t worldStartX = 0;
	int16_t worldStartY = 0;

	// Icon size is automatically calculated from texture dimensions
	// (stored for serialization, updated when textures are loaded)
	int16_t iconSizeX = 0;
	int16_t iconSizeY = 0;

	// Screen position for driver dots (screen size is 512x252)
	int16_t driverDotStartX = 450;
	int16_t driverDotStartY = 180;

	// Orientation mode - determines minimap orientation relative to the world
	// 0 = Right, 1 = Down, 2 = Left, 3 = Up
	int16_t orientationMode = 0;
	
	// Unknown field - used for drawing, needed for some levels like Crash Cove
	int16_t unk = 0;

	// Texture paths
	std::filesystem::path topTexturePath;
	std::filesystem::path bottomTexturePath;

	// Texture objects
	Texture topTexture;
	Texture bottomTexture;

	// State flags
	bool hasTopTexture = false;
	bool hasBottomTexture = false;
	bool enabled = false;

	// Calculate world bounds from all quadblocks in the level
	void CalculateWorldBoundsFromQuadblocks(const std::vector<Quadblock>& quadblocks);
	
	// Serialize to PSX Map struct
	PSX::Map Serialize() const;
	
	// Deserialize from PSX Map struct
	void Deserialize(const PSX::Map& map);
	
	// Load textures from paths (call after loading from preset)
	void LoadTextures();
	
	// Check if minimap is ready for export
	bool IsReady() const;
	
	// Get textures for VRM packing
	std::vector<Texture*> GetTextures();
	
	// Render the ImGui UI for minimap configuration
	// Returns true if world bounds were modified (for updating visualization)
	bool RenderUI(const std::vector<Quadblock>& quadblocks);
	
	// Clear all minimap data
	void Clear();
};
