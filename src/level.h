#pragma once

#include "vertex.h"
#include "quadblock.h"
#include "checkpoint.h"
#include "lev.h"
#include "bsp.h"
#include "path.h"

#include <vector>
#include <unordered_map>
#include <filesystem>

class Level
{
public:
	bool Load(const std::filesystem::path& filename);
	bool Save(const std::filesystem::path& path);
	bool Ready();
	void Clear(bool clearErrors);
	void RenderUI();

private:
	bool LoadLEV(const std::filesystem::path& levFile);
	bool SaveLEV(const std::filesystem::path& path);
	bool LoadOBJ(const std::filesystem::path& objFile);

private:
	bool m_showLogWindow;
	std::vector<std::tuple<std::string, std::string>> m_invalidQuadblocks;
	std::string m_logMessage;
	std::string m_name;
	Spawn m_spawn[NUM_DRIVERS];
	uint32_t m_configFlags;
	ColorGradient m_skyGradient[NUM_GRADIENT];
	Color m_clearColor;
	std::vector<Quadblock> m_quadblocks;
	std::vector<Checkpoint> m_checkpoints;
	BSP m_bsp;
	std::unordered_map<std::string, std::vector<size_t>> m_materialToQuadblocks;
	std::unordered_map<std::string, std::string> m_materialTerrainPreview;
	std::unordered_map<std::string, std::string> m_materialTerrainBackup;
	std::unordered_map<std::string, uint16_t> m_materialQuadflagsPreview;
	std::unordered_map<std::string, uint16_t> m_materialQuadflagsBackup;
	std::unordered_map<std::string, bool> m_materialDoubleSidedPreview;
	std::unordered_map<std::string, bool> m_materialDoubleSidedBackup;
	std::unordered_map<std::string, bool> m_materialCheckpointPreview;
	std::unordered_map<std::string, bool> m_materialCheckpointBackup;
	std::vector<Path> m_checkpointPaths;
};