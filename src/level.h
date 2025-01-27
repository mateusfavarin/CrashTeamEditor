#pragma once

#include "vertex.h"
#include "quadblock.h"
#include "checkpoint.h"
#include "lev.h"
#include "bsp.h"
#include "path.h"
#include "material.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <array>
#include <unordered_map>
#include <filesystem>

class Level
{
public:
	bool Load(const std::filesystem::path& filename);
	bool Save(const std::filesystem::path& path);
	bool Loaded();
	bool Ready();
	void Clear(bool clearErrors);
	bool LoadPreset(const std::filesystem::path& filename);
	bool SavePreset(const std::filesystem::path& path);
	void RenderUI();

private:
	void ManageTurbopad(Quadblock& quadblock);
	bool LoadLEV(const std::filesystem::path& levFile);
	bool SaveLEV(const std::filesystem::path& path);
	bool LoadOBJ(const std::filesystem::path& objFile);

private:
	bool m_showLogWindow;
	bool m_loaded;
	std::vector<std::tuple<std::string, std::string>> m_invalidQuadblocks;
	std::string m_logMessage;
	std::string m_name;
	std::array<Spawn, NUM_DRIVERS> m_spawn;
	uint32_t m_configFlags;
	std::array<ColorGradient, NUM_GRADIENT> m_skyGradient;
	Color m_clearColor;
	std::vector<Quadblock> m_quadblocks;
	std::vector<Checkpoint> m_checkpoints;
	BSP m_bsp;
	std::unordered_map<std::string, std::vector<size_t>> m_materialToQuadblocks;
	MaterialProperty<std::string, MaterialType::TERRAIN> m_propTerrain;
	MaterialProperty<uint16_t, MaterialType::QUAD_FLAGS> m_propQuadFlags;
	MaterialProperty<bool, MaterialType::DRAW_FLAGS> m_propDoubleSided;
	MaterialProperty<bool, MaterialType::CHECKPOINT> m_propCheckpoints;
	std::vector<Path> m_checkpointPaths;
};