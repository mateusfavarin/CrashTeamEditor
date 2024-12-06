#pragma once

#include "vertex.h"
#include "quadblock.h"
#include "checkpoint.h"
#include "lev.h"
#include "bsp.h"

#include <vector>
#include <filesystem>

class Level
{
public:
	bool Load(const std::filesystem::path& filename);
	bool Save(const std::filesystem::path& path);
	bool Ready();
	void Clear();
	void RenderUI();

private:
	bool LoadLEV(const std::filesystem::path& levFile);
	bool SaveLEV(const std::filesystem::path& path);
	bool LoadOBJ(const std::filesystem::path& objFile);

private:
	std::string m_name;
	Spawn m_spawn[NUM_DRIVERS];
	uint32_t m_configFlags;
	ColorGradient m_skyGradient[NUM_GRADIENT];
	Color m_clearColor;
	std::vector<Quadblock> m_quadblocks;
	std::vector<Checkpoint> m_checkpoints;
	BSP m_bsp;
};