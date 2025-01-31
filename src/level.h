#pragma once

#include "vertex.h"
#include "quadblock.h"
#include "checkpoint.h"
#include "lev.h"
#include "bsp.h"
#include "path.h"
#include "material.h"

#include "model.h"
#include "mesh.h"

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
	void GenerateRasterizableData(std::vector<Quadblock>& quadblocks);

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
	MaterialProperty<std::string> m_propTerrain;
	MaterialProperty<uint16_t> m_propQuadFlags;
	MaterialProperty<bool> m_propDoubleSided;
	MaterialProperty<bool> m_propCheckpoints;
	std::vector<Path> m_checkpointPaths;
	Model m_highLODLevelModel;
	Model m_lowLODLevelModel;
	Model m_pointsHighLODLevelModel;
	Model m_pointsLowLODLevelModel;
};