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
#include "vistree.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <array>
#include <unordered_map>
#include <filesystem>
#include <tuple>
#include <cstdint>

static constexpr size_t REND_NO_SELECTED_QUADBLOCK = std::numeric_limits<size_t>::max();

namespace LevelModels
{
	static constexpr size_t LEVEL = 0;
	static constexpr size_t BSP = 1;
	static constexpr size_t SPAWN = 2;
	static constexpr size_t CHECKPOINT = 3;
	static constexpr size_t SELECTED = 4;
	static constexpr size_t MULTI_SELECTED = 5;
	static constexpr size_t FILTER = 6;
	static constexpr size_t COUNT = 7;
};

class Level
{
public:
	bool Load(const std::filesystem::path& filename);
	bool Save(const std::filesystem::path& path);
	bool IsLoaded() const;
	void Clear(bool clearErrors);
	const std::string& GetName() const;
	std::vector<Quadblock>& GetQuadblocks();
	BSP& GetBSP();
	std::vector<Checkpoint>& GetCheckpoints();
	std::vector<Path>& GetCheckpointPaths();
	const std::filesystem::path& GetParentPath() const;
	std::vector<std::string> GetMaterialNames() const;
	std::vector<size_t> GetMaterialQuadblockIndexes(const std::string& material) const;
	std::tuple<std::vector<Quadblock*>, Vec3> GetRendererSelectedData();
	Model* GetLevelModel();
	Model* GetBspModel();
	Model* GetSpawnModel();
	Model* GetCheckpointModel();
	Model* GetSelectedModel();
	Model* GetMultiSelectedModel();
	Model* GetFilterModel();
	bool LoadPreset(const std::filesystem::path& filename);
	bool SavePreset(const std::filesystem::path& path);
	void ResetFilter();
	void ResetRendererSelection();
	void UpdateRenderCheckpointData();

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

	void OpenHotReloadWindow();
	void RenderUI(Renderer& renderer);

	void InitModels(Renderer& renderer);
	void GenerateRenderLevData();
	void UpdateAnimationRenderData();
	void UpdateFilterRenderData(const Quadblock& qb);
	void GenerateRenderBspData();
	void GenerateRenderStartpointData();
	void GenerateRenderSelectedBlockData(const Quadblock& quadblock, const Vec3& queryPoint);
	bool UpdateAnimTextures(float deltaTime);
	void ViewportClickHandleBlockSelection(int pixelX, int pixelY, bool appendSelection, const Renderer& rend);

	friend class UI;

private:
	bool m_saveScript;
	bool m_showLogWindow;
	bool m_showHotReloadWindow;
	bool m_loaded;
	bool m_simpleVisTree;
	bool m_genVisTree;
	int m_maxQuadPerLeaf;
	float m_maxLeafAxisLength;
	float m_distanceNearClip;
	float m_distanceFarClip;

	std::vector<std::tuple<std::string, std::string>> m_invalidQuadblocks;
	std::string m_logMessage;
	std::string m_name;

	std::filesystem::path m_parentPath;
	std::filesystem::path m_hotReloadLevPath;
	std::filesystem::path m_hotReloadVRMPath;

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
	std::string m_pythonScript = "print('CrashTeamEditor Python console ready!')\nprint('Level:', m_lev.name)";
	std::string m_pythonConsole;
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
	MaterialProperty<bool, MaterialType::CHECKPOINT_PATHABLE> m_propCheckpointPathable;
	MaterialProperty<bool, MaterialType::VISTREE_TRANSPARENT> m_propVisTreeTransparent;

	std::array<Model*, LevelModels::COUNT> m_models;

	Vec3 m_rendererQueryPoint;
	std::vector<size_t> m_rendererSelectedQuadblockIndexes;
	size_t m_lastAnimTextureCount;

	std::vector<PSX::TextureGroup> m_rawTexGroups;
	std::vector<uint8_t> m_rawAnimData;
	bool m_hasRawTextureData;

	std::vector<Vertex> m_originalVertices;
	bool m_hasOriginalVertices;
};
