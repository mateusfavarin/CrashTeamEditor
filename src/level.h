#pragma once

#include "vertex.h"
#include "quadblock.h"
#include "checkpoint.h"
#include "lev.h"
#include "bsp.h"
#include "path.h"
#include "material.h"
#include "texture.h"
#include "renderer.h"
#include "animtexture.h"
#include "model.h"
#include "mesh.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <array>
#include <unordered_map>
#include <filesystem>
#include <cstdint>

class Level
{
public:
	bool Load(const std::filesystem::path& filename);
	bool Save(const std::filesystem::path& path);
	bool Loaded();
	bool Ready();
	void OpenHotReloadWindow();
	void Clear(bool clearErrors);
	const std::vector<Quadblock>& GetQuadblocks() const;
	bool LoadPreset(const std::filesystem::path& filename);
	bool SavePreset(const std::filesystem::path& path);
	void RenderUI();

	void GenerateRenderLevData(std::vector<Quadblock>& quadblocks);
	void GenerateRenderBspData(const BSP& bsp);
	void GenerateRenderCheckpointData(std::vector<Checkpoint>&);
	void GenerateRenderStartpointData(std::array<Spawn, NUM_DRIVERS>&);
	void GenerateRenderSelectedBlockData(const Quadblock& quadblock, const Vec3& queryPoint);
	void GeomPoint(const Vertex* verts, int ind, std::vector<float>& data);
	void GeomOctopoint(const Vertex* verts, int ind, std::vector<float>& data);
	void GeomBoundingRect(const BSP* b, int depth, std::vector<float>& data);
	void ViewportClickHandleBlockSelection(int pixelX, int pixelY, const Renderer& rend);

private:
	void ManageTurbopad(Quadblock& quadblock);
	bool LoadLEV(const std::filesystem::path& levFile);
	bool SaveLEV(const std::filesystem::path& path);
	bool LoadOBJ(const std::filesystem::path& objFile);
	bool StartEmuIPC(const std::string& emulator);
	bool HotReload(const std::string& levPath, const std::string& vrmPath, const std::string& emulator);
	bool SaveGhostData(const std::string& emulator, const std::filesystem::path& path);
	bool SetGhostData(const std::filesystem::path& path, bool tropy);

private:
	bool m_showLogWindow;
	bool m_showHotReloadWindow;
	bool m_loaded;
	std::vector<std::tuple<std::string, std::string>> m_invalidQuadblocks;
	std::string m_logMessage;
	std::string m_name;
	std::filesystem::path m_savedLevPath;
	std::filesystem::path m_savedVRMPath;
	std::array<Spawn, NUM_DRIVERS> m_spawn;
	uint32_t m_configFlags;
	std::array<ColorGradient, NUM_GRADIENT> m_skyGradient;
	Color m_clearColor;
	std::vector<uint8_t> m_tropyGhost;
	std::vector<uint8_t> m_oxideGhost;
	std::vector<Quadblock> m_quadblocks;
	std::vector<Checkpoint> m_checkpoints;
	BSP m_bsp;
	std::unordered_map<std::string, std::vector<size_t>> m_materialToQuadblocks;
	std::unordered_map<std::string, Texture> m_materialToTexture;
	MaterialProperty<std::string, MaterialType::TERRAIN> m_propTerrain;
	MaterialProperty<uint16_t, MaterialType::QUAD_FLAGS> m_propQuadFlags;
	MaterialProperty<bool, MaterialType::DRAW_FLAGS> m_propDoubleSided;
	MaterialProperty<bool, MaterialType::CHECKPOINT> m_propCheckpoints;
	std::vector<Path> m_checkpointPaths;
	std::vector<AnimTexture> m_animTextures;

	Mesh m_lowLODMesh;
	Mesh m_highLODMesh;
	Mesh m_vertexLowLODMesh;
	Mesh m_vertexHighLODMesh;

	Model m_highLODLevelModel;
	Model m_lowLODLevelModel;
	Model m_pointsHighLODLevelModel;
	Model m_pointsLowLODLevelModel;
	Model m_bspModel;
	Model m_spawnsModel;
	Model m_checkModel;
	Model m_selectedBlockModel;
};