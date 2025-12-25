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
#include "vistree.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <array>
#include <unordered_map>
#include <filesystem>
#include <cstdint>

static constexpr size_t REND_NO_SELECTED_QUADBLOCK = std::numeric_limits<size_t>::max();

class Level
{
public:
	bool Load(const std::filesystem::path& filename);
	bool Save(const std::filesystem::path& path);
	bool Loaded() const;
	void OpenHotReloadWindow();
	void OpenModelExtractorWindow();
	void Clear(bool clearErrors);
	const std::string& GetName() const;
	const std::vector<Quadblock>& GetQuadblocks() const;
	const std::filesystem::path& GetParentPath() const;
	bool LoadPreset(const std::filesystem::path& filename);
	bool SavePreset(const std::filesystem::path& path);
	void RenderUI();

private:
	void ManageTurbopad(Quadblock& quadblock);
	bool LoadLEV(const std::filesystem::path& levFile);
	bool SaveLEV(const std::filesystem::path& path);
	bool LoadOBJ(const std::filesystem::path& objFile);
	bool StartEmuIPC(const std::string& emulator);
	bool HotReload(const std::string& levPath, const std::string& vrmPath, const std::string& emulator);
	bool SaveGhostData(const std::string& emulator, const std::filesystem::path& path);
	bool SetGhostData(const std::filesystem::path& path, bool tropy);
	bool UpdateVRM();
	bool GenerateCheckpoints();
	bool GenerateBSP();

	void GenerateRenderLevData();
	void GenerateRenderBspData(const BSP& bsp);
	void GenerateRenderCheckpointData(std::vector<Checkpoint>&);
	void GenerateRenderStartpointData(std::array<Spawn, NUM_DRIVERS>&);
	void GenerateRenderSelectedBlockData(const Quadblock& quadblock, const Vec3& queryPoint);
	void GenerateRenderMultipleQuadsData(const std::vector<Quadblock*>& quads);
	void RefreshTextureStores();
	void GeomPoint(const Vertex* verts, int ind, std::vector<float>& data);
	void GeomOctopoint(const Vertex* verts, int ind, std::vector<float>& data);
	void GeomBoundingRect(const BSP* b, int depth, std::vector<float>& data);
	void GeomUVs(const Quadblock& qb, int quadInd, int vertInd, std::vector<float>& data, int textureIndex);
	void ViewportClickHandleBlockSelection(int pixelX, int pixelY, const Renderer& rend);

private:
	bool m_showLogWindow;
	bool m_showHotReloadWindow;
	bool m_showModelExtractorWindow;
	bool m_loaded;
	bool m_genVisTree;
	float m_maxLeafAxisLength;
	float m_distanceFarClip;
	std::vector<std::tuple<std::string, std::string>> m_invalidQuadblocks;
	std::string m_logMessage;
	std::string m_name;

	std::filesystem::path m_parentPath;
	std::filesystem::path m_hotReloadLevPath;
	std::filesystem::path m_hotReloadVRMPath;
	std::filesystem::path m_modelExtractorLevPath;
	std::filesystem::path m_modelExtractorVrmPath;

	std::array<Spawn, NUM_DRIVERS> m_spawn;
	uint32_t m_configFlags;
	std::array<ColorGradient, NUM_GRADIENT> m_skyGradient;
	Color m_clearColor;
	Stars m_stars;
	std::vector<uint8_t> m_tropyGhost;
	std::vector<uint8_t> m_oxideGhost;
	std::vector<Quadblock> m_quadblocks;
	std::vector<Checkpoint> m_checkpoints;
	BSP m_bsp;
	std::vector<Path> m_checkpointPaths;
	std::vector<AnimTexture> m_animTextures;
	BitMatrix m_bspVis;
	std::vector<uint8_t> m_vrm;

	std::unordered_map<std::string, std::vector<size_t>> m_materialToQuadblocks;
	std::unordered_map<std::string, Texture> m_materialToTexture;
	MaterialProperty<std::string, MaterialType::TERRAIN> m_propTerrain;
	MaterialProperty<uint16_t, MaterialType::QUAD_FLAGS> m_propQuadFlags;
	MaterialProperty<bool, MaterialType::DRAW_FLAGS> m_propDoubleSided;
	MaterialProperty<bool, MaterialType::CHECKPOINT> m_propCheckpoints;
	MaterialProperty<QuadblockTrigger, MaterialType::TURBO_PAD> m_propTurboPads;
	MaterialProperty<int, MaterialType::SPEED_IMPACT> m_propSpeedImpact;

	Mesh m_lowLODMesh;
	Mesh m_highLODMesh;
	Mesh m_vertexLowLODMesh;
	Mesh m_vertexHighLODMesh;

	Model m_levelModel;
	Model m_bspModel;
	Model m_spawnsModel;
	Model m_checkModel;
	Model m_selectedBlockModel;
	Model m_multipleSelectedQuads;
	std::vector<Model> m_levelInstancesModels;

	size_t m_rendererSelectedQuadblockIndex;
};