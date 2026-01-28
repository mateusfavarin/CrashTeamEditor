#include "level.h"
#include "psx_types.h"
#include "io.h"
#include "utils.h"
#include "geo.h"
#include "process.h"
#include "gui_render_settings.h"
#include "renderer.h"
#include "vistree.h"
#include "minimap.h"

#include <fstream>
#include <unordered_set>
#include <map>
#include <algorithm>

bool Level::Load(const std::filesystem::path& filename)
{
	Clear(true);
	if (!filename.has_filename() || !filename.has_extension()) { return false; }
	std::filesystem::path ext = filename.extension();
	if (ext == ".lev") { return LoadLEV(filename); }
	if (ext == ".obj") { return LoadOBJ(filename); }
	return false;
}

bool Level::Save(const std::filesystem::path& path)
{
	return SaveLEV(path);
}

bool Level::IsLoaded() const
{
	return m_loaded;
}

void Level::OpenHotReloadWindow()
{
	m_showHotReloadWindow = true;
}

void Level::Clear(bool clearErrors)
{
	m_loaded = false;
	m_showHotReloadWindow = false;
	for (size_t i = 0; i < NUM_DRIVERS; i++) { m_spawn[i] = Spawn(); }
	for (size_t i = 0; i < NUM_GRADIENT; i++) { m_skyGradient[i] = ColorGradient(); }
	if (clearErrors)
	{
		m_showLogWindow = false;
		m_logMessage.clear();
		m_invalidQuadblocks.clear();
	}
	m_configFlags = LevConfigFlags::NONE;
	m_clearColor = Color();
	m_stars = {};
	m_stars.zDepth = static_cast<uint16_t>(OT_SIZE) - 2;
	m_name.clear();
	m_hotReloadLevPath.clear();
	m_hotReloadVRMPath.clear();
	m_quadblocks.clear();
	m_checkpoints.clear();
	m_bsp.Clear();
	m_materialToQuadblocks.clear();
	m_materialToTexture.clear();
	m_checkpointPaths.clear();
	m_tropyGhost.clear();
	m_oxideGhost.clear();
	m_animTextures.clear();
	m_rendererQueryPoint = Vec3();
	m_rendererSelectedQuadblockIndexes.clear();
	m_genVisTree = false;
	m_bspVis.Clear();
	m_maxLeafAxisLength = 64.0f;
	m_distanceFarClip = 1000.0f;
	m_pythonConsole.clear();
	m_saveScript = false;
	m_vrm.clear();
	m_textureStorePaths.clear();
	m_lastAnimTextureCount = 0;
	m_minimapConfig.Clear();
	DeleteMaterials(this);

	m_levelModel.Clear();
	m_bspModel.Clear();
	m_spawnsModel.Clear();
	m_checkModel.Clear();
	m_minimapBoundsModel.Clear();
	m_selectedBlockModel.Clear();
	m_multipleSelectedQuads.Clear();
	m_filterEdgeModel.Clear();

	m_lowLODMesh.Clear();
	m_highLODMesh.Clear();
	m_vertexLowLODMesh.Clear();
	m_vertexHighLODMesh.Clear();
	m_filterEdgeLowLODMesh.Clear();
	m_filterEdgeHighLODMesh.Clear();
}

const std::string& Level::GetName() const
{
	return m_name;
}

std::vector<Quadblock>& Level::GetQuadblocks()
{
	return m_quadblocks;
}

BSP& Level::GetBSP()
{
	return m_bsp;
}

std::vector<Checkpoint>& Level::GetCheckpoints()
{
	return m_checkpoints;
}

std::vector<Path>& Level::GetCheckpointPaths()
{
	return m_checkpointPaths;
}

const std::filesystem::path& Level::GetParentPath() const
{
	return m_parentPath;
}

MinimapConfig& Level::GetMinimapConfig()
{
	return m_minimapConfig;
}

std::vector<std::string> Level::GetMaterialNames() const
{
	std::vector<std::string> names;
	names.reserve(m_materialToQuadblocks.size());
	for (const auto& [key, value] : m_materialToQuadblocks) { names.push_back(key); }
	return names;
}

std::vector<size_t> Level::GetMaterialQuadblockIndexes(const std::string& material) const
{
	if (!m_materialToQuadblocks.contains(material)) { return std::vector<size_t>(); }
	return m_materialToQuadblocks.at(material);
}

std::tuple<std::vector<Quadblock*>, Vec3> Level::GetRendererSelectedData()
{
	std::vector<Quadblock*> quadblocks;
	quadblocks.reserve(m_rendererSelectedQuadblockIndexes.size());
	for (size_t index : m_rendererSelectedQuadblockIndexes)
	{
		if (index < m_quadblocks.size()) { quadblocks.push_back(&m_quadblocks[index]); }
	}
	return std::make_tuple(std::move(quadblocks), m_rendererQueryPoint);
}

bool Level::GenerateBSP()
{
	std::vector<size_t> quadIndexes;
	for (size_t i = 0; i < m_quadblocks.size(); i++) { quadIndexes.push_back(i); }
	m_bsp.Clear();
	m_bsp.SetQuadblockIndexes(quadIndexes);
	m_bsp.Generate(m_quadblocks, MAX_QUADBLOCKS_LEAF, m_maxLeafAxisLength);
	if (m_bsp.IsValid())
	{
		GenerateRenderBspData(m_bsp);
		std::vector<const BSP*> bspLeaves = m_bsp.GetLeaves();
		if (m_genVisTree) { m_bspVis = GenerateVisTree(m_quadblocks, bspLeaves, m_distanceFarClip * m_distanceFarClip); }
		return true;
	}
	m_bsp.Clear();
	return false;
}

bool Level::GenerateCheckpoints()
{
	if (m_checkpointPaths.empty()) { return false; }

	for (const Path& path : m_checkpointPaths) { if (!path.IsReady()) { return false; } }

	size_t checkpointIndex = 0;
	std::vector<size_t> linkNodeIndexes;
	std::vector<std::vector<Checkpoint>> pathCheckpoints;
	for (Path& path : m_checkpointPaths)
	{
		pathCheckpoints.push_back(path.GeneratePath(checkpointIndex, m_quadblocks));
		checkpointIndex += pathCheckpoints.back().size();
		linkNodeIndexes.push_back(path.GetStart());
		linkNodeIndexes.push_back(path.GetEnd());
	}
	m_checkpoints.clear();
	for (const std::vector<Checkpoint>& checkpoints : pathCheckpoints)
	{
		for (const Checkpoint& checkpoint : checkpoints)
		{
			m_checkpoints.push_back(checkpoint);
		}
	}

	int lastPathIndex = static_cast<int>(m_checkpointPaths.size()) - 1;
	const Checkpoint* currStartCheckpoint = &m_checkpoints[0];
	float distFinish = 0.0f;
	for (int i = lastPathIndex; i >= 0; i--)
	{
		m_checkpointPaths[i].UpdateDist(distFinish, currStartCheckpoint->GetPos(), m_checkpoints);
		currStartCheckpoint = &m_checkpoints[m_checkpointPaths[i].GetStart()];
		distFinish = currStartCheckpoint->GetDistFinish();
	}

	for (size_t i = 0; i < linkNodeIndexes.size(); i++)
	{
		Checkpoint& node = m_checkpoints[linkNodeIndexes[i]];
		if (i % 2 == 0)
		{
			size_t linkDown = (i == 0) ? linkNodeIndexes.size() - 1 : i - 1;
			node.UpdateDown(static_cast<int>(linkNodeIndexes[linkDown]));
		}
		else
		{
			size_t linkUp = (i + 1) % linkNodeIndexes.size();
			node.UpdateUp(static_cast<int>(linkNodeIndexes[linkUp]));
		}
	}

	// Cap the number of checkpoints to 255
	const size_t MAX_CHECKPOINTS = 255;
	if (m_checkpoints.size() > MAX_CHECKPOINTS)
	{
		std::unordered_set<size_t> protectedIndices(linkNodeIndexes.begin(), linkNodeIndexes.end());
		for (size_t i = 0; i < m_checkpoints.size(); ++i)
		{
			const Checkpoint& cp = m_checkpoints[i];
			if (cp.GetRight() != NONE_CHECKPOINT_INDEX || cp.GetLeft() != NONE_CHECKPOINT_INDEX)
			{
				protectedIndices.insert(i);
			}
		}

		// Build dist->index map for candidates
		std::multimap<float, size_t> distToNextMap;
		std::unordered_map<size_t, float> currentDistances;

		for (size_t i = 0; i < m_checkpoints.size(); ++i)
		{
			if (protectedIndices.find(i) != protectedIndices.end()) continue;
			const Checkpoint& cp = m_checkpoints[i];
			int downIndex = cp.GetDown();
			if (downIndex != NONE_CHECKPOINT_INDEX)
			{
				float distToNext = (m_checkpoints[downIndex].GetPos() - cp.GetPos()).Length();
				currentDistances[i] = distToNext;
				distToNextMap.insert({distToNext, i});
			}
		}

		size_t total = m_checkpoints.size();
		size_t numToRemove = total - MAX_CHECKPOINTS;
		size_t numRemovable = distToNextMap.size();
		if (numToRemove > numRemovable)
		{
			numToRemove = numRemovable;
		}

		// Heuristic: Pick smallest-dist-to-next checkpoint to remove
		std::unordered_set<size_t> indexesToRemove;
		auto it = distToNextMap.begin();
		for (size_t i = 0; i < numToRemove && it != distToNextMap.end(); )
		{
			size_t candidateIndex = it->second;
			indexesToRemove.insert(candidateIndex);

			// Update distance for previous checkpoint
			const Checkpoint& removedCP = m_checkpoints[candidateIndex];
			int upIndex = removedCP.GetUp();
			int downIndex = removedCP.GetDown();

			if (upIndex != NONE_CHECKPOINT_INDEX &&
				downIndex != NONE_CHECKPOINT_INDEX &&
				protectedIndices.find(upIndex) == protectedIndices.end())
			{
				// Update the distance for the checkpoint before this one
				float oldDist = currentDistances[upIndex];
				float removedDist = currentDistances[candidateIndex];
				float newDist = oldDist + removedDist;

				auto range = distToNextMap.equal_range(oldDist);
				for (auto mapIt = range.first; mapIt != range.second; ++mapIt)
				{
					if (mapIt->second == upIndex)
					{
						distToNextMap.erase(mapIt);
						break;
					}
				}

				currentDistances[upIndex] = newDist;
				distToNextMap.insert({newDist, upIndex});
			}

			++it;
			++i;
		}

		// Build mapping oldIndex -> newIndex
		std::vector<int> oldToNew(total, -1);
		std::vector<Checkpoint> newCheckpoints;
		newCheckpoints.reserve(MAX_CHECKPOINTS);

		for (size_t old = 0; old < total; ++old)
		{
			if (indexesToRemove.find(old) == indexesToRemove.end())
			{
				int newIdx = static_cast<int>(newCheckpoints.size());
				oldToNew[old] = newIdx;
				// copy original checkpoint
				newCheckpoints.push_back(m_checkpoints[old]);
			}
		}

		// Update links
		const int N = static_cast<int>(newCheckpoints.size());

		for (int i = 0; i < N; ++i)
		{
			newCheckpoints[i].SetIndex(i);

			int newUp = (i + 1) % N;
			int newDown = (i == 0) ? (N - 1) : (i - 1);
			newCheckpoints[i].UpdateUp(newUp);
			newCheckpoints[i].UpdateDown(newDown);
		}

		// Update quadblock checkpoint references
		for (Quadblock& qb : m_quadblocks)
		{
			int oldCheckpoint = qb.GetCheckpoint();
			if (oldCheckpoint >= 0 && oldCheckpoint < static_cast<int>(oldToNew.size()))
			{
				int newCheckpoint = oldToNew[oldCheckpoint];
				if (newCheckpoint == -1)
				{
					// This checkpoint was removed, find nearest valid checkpoint
					float minDist = std::numeric_limits<float>::max();
					int nearestCheckpoint = 0;
					Vec3 qbCenter = qb.GetBoundingBox().Midpoint();

					for (int i = 0; i < N; ++i)
					{
						float dist = (newCheckpoints[i].GetPos() - qbCenter).Length();
						if (dist < minDist)
						{
							minDist = dist;
							nearestCheckpoint = i;
						}
					}
					qb.SetCheckpoint(nearestCheckpoint);
				}
				else
				{
					qb.SetCheckpoint(newCheckpoint);
				}
			}
		}

		m_checkpoints = std::move(newCheckpoints);
	}

	GenerateRenderCheckpointData(m_checkpoints);
	return true;
}


enum class PresetHeader : unsigned
{
	SPAWN, LEVEL, PATH, MATERIAL, TURBO_PAD, ANIM_TEXTURES, SCRIPT, MINIMAP
};

bool Level::LoadPreset(const std::filesystem::path& filename)
{
	m_showLogWindow = true;
	nlohmann::json json = nlohmann::json::parse(std::ifstream(filename));
	if (!json.contains("header"))
	{
		m_logMessage += "\nFailed loaded preset: " + filename.string();
		return false;
	}

	const PresetHeader header = json["header"];
	if (header == PresetHeader::SPAWN)
	{
		if (json.contains("spawn")) { m_spawn = json["spawn"]; }
	}
	else if (header == PresetHeader::LEVEL)
	{
		if (json.contains("configFlags")) { m_configFlags = json["configFlags"]; }
		if (json.contains("skyGradient")) { m_skyGradient = json["skyGradient"]; }
		if (json.contains("clearColor")) { m_clearColor = json["clearColor"]; }
		if (json.contains("stars")) { json["stars"].get_to(m_stars); }
	}
	else if (header == PresetHeader::PATH)
	{
		if (json.contains("pathCount"))
		{
			const size_t pathCount = json["pathCount"];
			m_checkpointPaths.resize(pathCount);
			for (size_t i = 0; i < pathCount; i++)
			{
				if (!json.contains("path" + std::to_string(i))) { continue; }

				nlohmann::json& pathJson = json["path" + std::to_string(i)];
				if (!pathJson.contains("index")) { continue; }

				size_t index = pathJson["index"];
				Path& path = m_checkpointPaths[index];
				path.FromJson(pathJson, m_quadblocks);
			}
			GenerateCheckpoints();
		}
	}
	else if (header == PresetHeader::MATERIAL)
	{
		if (json.contains("materials"))
		{
			std::vector<std::string> materials = json["materials"];
			for (const std::string& material : materials)
			{
				if (m_materialToQuadblocks.contains(material))
				{
					if (json.contains(material + "_terrain"))
					{
						m_propTerrain.SetPreview(material, json[material + "_terrain"]);
						m_propTerrain.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
					}
					if (json.contains(material + "_quadflags"))
					{
						m_propQuadFlags.SetPreview(material, json[material + "_quadflags"]);
						m_propQuadFlags.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
					}
					if (json.contains(material + "_drawflags"))
					{
						m_propDoubleSided.SetPreview(material, json[material + "_drawflags"]);
						m_propDoubleSided.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
					}
					if (json.contains(material + "_checkpoint"))
					{
						m_propCheckpoints.SetPreview(material, json[material + "_checkpoint"]);
						m_propCheckpoints.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
					}
					if (json.contains(material + "_trigger"))
					{
						QuadblockTrigger trigger = json[material + "_trigger"];
						m_propTurboPads.GetBackup(material) = trigger;
						m_propTurboPads.GetPreview(material) = trigger;
					}
					if (json.contains(material + "_speedImpact"))
					{
						m_propSpeedImpact.SetPreview(material, json[material + "_speedImpact"]);
						m_propSpeedImpact.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
					}
					if (json.contains(material + "_checkpointPathable"))
					{
						m_propCheckpointPathable.SetPreview(material, json[material + "_checkpointPathable"]);
						m_propCheckpointPathable.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
					}
				}
			}
		}
	}
	else if (header == PresetHeader::ANIM_TEXTURES)
	{
		if (json.contains("animCount"))
		{
			const size_t animCount = json["animCount"];
			for (size_t i = 0; i < animCount; i++)
			{
				if (!json.contains("anim" + std::to_string(i))) { continue; }
				AnimTexture animTexture;
				animTexture.FromJson(json["anim" + std::to_string(i)], m_quadblocks, m_parentPath);
				if (animTexture.IsPopulated()) { m_animTextures.push_back(animTexture); }
			}
		}
	}
	else if (header == PresetHeader::TURBO_PAD)
	{
		if (json.contains("turbopads"))
		{
			std::unordered_set<std::string> turboPads = json["turbopads"];
			for (Quadblock& quadblock : m_quadblocks)
			{
				const std::string& quadName = quadblock.GetName();
				if (turboPads.contains(quadName))
				{
					if (!json.contains(quadName + "_trigger")) { continue; }
					quadblock.SetTrigger(json[quadName + "_trigger"]);
					ManageTurbopad(quadblock);
					if (m_bsp.IsValid())
					{
						m_bsp.Clear();
						GenerateRenderBspData(m_bsp);
					}
				}
			}
		}
	}
	else if (header == PresetHeader::SCRIPT)
	{
		m_pythonScript = json["script"];
	}
	else if (header == PresetHeader::MINIMAP)
	{
		if (json.contains("minimap"))
		{
			m_minimapConfig = json["minimap"];
		}
	}
	else
	{
		m_logMessage += "\nFailed loaded preset: " + filename.string();
		return false;
	}
	m_logMessage += "\nSuccessfully loaded preset: " + filename.string();
	return true;
}

bool Level::SavePreset(const std::filesystem::path& path)
{
	std::filesystem::path dirPath = path / (m_name + "_presets");
	if (!std::filesystem::exists(dirPath)) { std::filesystem::create_directory(dirPath); }

	auto SaveJSON = [](const std::filesystem::path& path, const nlohmann::json& json)
		{
			std::ofstream pathFile(path);
			pathFile << std::setw(4) << json << std::endl;
		};

	nlohmann::json spawnJson = {};
	spawnJson["header"] = PresetHeader::SPAWN;
	spawnJson["spawn"] = m_spawn;
	SaveJSON(dirPath / "spawn.json", spawnJson);

	nlohmann::json levelJson = {};
	levelJson["header"] = PresetHeader::LEVEL;
	levelJson["configFlags"] = m_configFlags;
	levelJson["skyGradient"] = m_skyGradient;
	levelJson["clearColor"] = m_clearColor;
	levelJson["stars"] = m_stars;
	SaveJSON(dirPath / "level.json", levelJson);

	nlohmann::json pathJson = {};
	pathJson["header"] = PresetHeader::PATH;
	pathJson["pathCount"] = m_checkpointPaths.size();
	for (size_t i = 0; i < m_checkpointPaths.size(); i++)
	{
		pathJson["path" + std::to_string(i)] = nlohmann::json();
		m_checkpointPaths[i].ToJson(pathJson["path" + std::to_string(i)], m_quadblocks);
	}
	SaveJSON(dirPath / "path.json", pathJson);

	if (!m_materialToQuadblocks.empty())
	{
		nlohmann::json materialJson = {};
		materialJson["header"] = PresetHeader::MATERIAL;
		std::vector<std::string> materials; materials.reserve(m_materialToQuadblocks.size());
		for (const auto& [key, value] : m_materialToQuadblocks)
		{
			materials.push_back(key);
			materialJson[key + "_terrain"] = m_propTerrain.GetBackup(key);
			materialJson[key + "_quadflags"] = m_propQuadFlags.GetBackup(key);
			materialJson[key + "_drawflags"] = m_propDoubleSided.GetBackup(key);
			materialJson[key + "_checkpoint"] = m_propCheckpoints.GetBackup(key);
			materialJson[key + "_checkpointPathable"] = m_propCheckpointPathable.GetBackup(key);
			materialJson[key + "_trigger"] = m_propTurboPads.GetBackup(key);
			materialJson[key + "_speedImpact"] = m_propSpeedImpact.GetBackup(key);
		}
		materialJson["materials"] = materials;
		SaveJSON(dirPath / "material.json", materialJson);
	}

	if (!m_animTextures.empty())
	{
		nlohmann::json animJson = {};
		animJson["header"] = PresetHeader::ANIM_TEXTURES;
		animJson["animCount"] = m_animTextures.size();
		size_t i = 0;
		for (const AnimTexture& animTexture : m_animTextures)
		{
			animTexture.ToJson(animJson["anim" + std::to_string(i++)], m_quadblocks);
		}
		SaveJSON(dirPath / "animtex.json", animJson);
	}

	std::unordered_set<std::string> turboPads;
	nlohmann::json turboPadJson = {};
	for (const Quadblock& quadblock : m_quadblocks)
	{
		if (quadblock.GetTurboPadIndex() == TURBO_PAD_INDEX_NONE) { continue; }
		const std::string& quadName = quadblock.GetName();
		turboPads.insert(quadName);
		turboPadJson[quadName + "_trigger"] = quadblock.GetTrigger();
	}
	if (!turboPads.empty())
	{
		turboPadJson["header"] = PresetHeader::TURBO_PAD;
		turboPadJson["turbopads"] = turboPads;
		SaveJSON(dirPath / "turbopad.json", turboPadJson);
	}

	if (m_saveScript)
	{
		nlohmann::json scriptJson = {};
		scriptJson["header"] = PresetHeader::SCRIPT;
		scriptJson["script"] = m_pythonScript;
		SaveJSON(dirPath / "script.json", scriptJson);
	}

	if (m_minimapConfig.enabled)
	{
		nlohmann::json minimapJson = {};
		minimapJson["header"] = PresetHeader::MINIMAP;
		minimapJson["minimap"] = m_minimapConfig;
		SaveJSON(dirPath / "minimap.json", minimapJson);
	}
	return true;
}

void Level::ResetFilter()
{
	for (Quadblock& qb : m_quadblocks)
	{
		qb.SetFilter(false);
		qb.SetFilterColor(GuiRenderSettings::defaultFilterColor);
	}
}

void Level::ResetRendererSelection()
{
	m_rendererQueryPoint = Vec3();
	m_rendererSelectedQuadblockIndexes.clear();
	m_selectedBlockModel.SetMesh();
}

void Level::UpdateRendererCheckpoints()
{
	GenerateRenderCheckpointData(m_checkpoints);
}

void Level::BuildRenderModels(std::vector<Model>& models)
{
	models.clear();
	if (!m_loaded) { return; }

	if (GuiRenderSettings::showLevel)
	{
		models.push_back(m_levelModel);
		if (GuiRenderSettings::showLowLOD)
		{
			if (GuiRenderSettings::showLevVerts) { m_levelModel.SetMesh(&m_vertexLowLODMesh); }
			else { m_levelModel.SetMesh(&m_lowLODMesh); }
		}
		else
		{
			if (GuiRenderSettings::showLevVerts) { m_levelModel.SetMesh(&m_vertexHighLODMesh); }
			else { m_levelModel.SetMesh(&m_highLODMesh); }
		}
	}
	if (GuiRenderSettings::showBspRectTree) { models.push_back(m_bspModel); }
	if (GuiRenderSettings::showCheckpoints) { models.push_back(m_checkModel); }
	if (GuiRenderSettings::showStartpoints) { models.push_back(m_spawnsModel); }
	if (GuiRenderSettings::showMinimapBounds) { models.push_back(m_minimapBoundsModel); }
	if (GuiRenderSettings::filterActive)
	{
		if (GuiRenderSettings::showLowLOD) { m_filterEdgeModel.SetMesh(&m_filterEdgeLowLODMesh); }
		else { m_filterEdgeModel.SetMesh(&m_filterEdgeHighLODMesh); }
		models.push_back(m_filterEdgeModel);
	}
	models.push_back(m_selectedBlockModel);
	if (GuiRenderSettings::showVisTree) { models.push_back(m_multipleSelectedQuads); }
}

void Level::ManageTurbopad(Quadblock& quadblock)
{
	bool stp = true;
	size_t turboPadIndex = TURBO_PAD_INDEX_NONE;
	switch (quadblock.GetTrigger())
	{
	case QuadblockTrigger::TURBO_PAD:
		stp = false;
	case QuadblockTrigger::SUPER_TURBO_PAD:
	{
		Quadblock turboPad = quadblock;
		const Vec3 up(0.0f, 1.0f, 0.0f);
		turboPad.Translate(TURBO_PAD_QUADBLOCK_TRANSLATION, up);
		turboPad.SetCheckpoint(-1);
		turboPad.SetCheckpointStatus(false);
		turboPad.SetName(quadblock.GetName() + (stp ? "_stp" : "_tp"));
		turboPad.SetFlag(QuadFlags::TRIGGER_SCRIPT | QuadFlags::INVISIBLE_TRIGGER | QuadFlags::WALL);
		turboPad.SetTerrain(stp ? TerrainType::SUPER_TURBO_PAD : TerrainType::TURBO_PAD);
		turboPad.SetTurboPadIndex(TURBO_PAD_INDEX_NONE);
		turboPad.SetHide(true);
		turboPad.SetAnimated(false);

		size_t index = m_quadblocks.size();
		turboPadIndex = quadblock.GetTurboPadIndex();
		quadblock.SetTurboPadIndex(index);
		m_quadblocks.push_back(turboPad);
		if (turboPadIndex == TURBO_PAD_INDEX_NONE) { break; }
	}
	case QuadblockTrigger::NONE:
	{
		bool clearTurboPadIndex = false;
		if (turboPadIndex == TURBO_PAD_INDEX_NONE)
		{
			clearTurboPadIndex = true;
			turboPadIndex = quadblock.GetTurboPadIndex();
		}
		if (turboPadIndex == TURBO_PAD_INDEX_NONE) { break; }

		for (Quadblock& quad : m_quadblocks)
		{
			size_t index = quad.GetTurboPadIndex();
			if (index > turboPadIndex) { quad.SetTurboPadIndex(index - 1); }
		}

		if (clearTurboPadIndex) { quadblock.SetTurboPadIndex(TURBO_PAD_INDEX_NONE); }
		m_quadblocks.erase(m_quadblocks.begin() + turboPadIndex);
		break;
	}
	}
}

bool Level::LoadLEV(const std::filesystem::path& levFile)
{
	std::ifstream file(levFile, std::ios::binary);

	uint32_t offPointerMap;
	Read(file, offPointerMap);

	std::streampos offLev = file.tellg();
	PSX::LevHeader header = {};
	Read(file, header);

	m_configFlags = header.config;
	m_clearColor = ConvertColor(header.clear);
	m_stars = ConvertStars(header.stars);
	for (size_t i = 0; i < m_spawn.size(); i++)
	{
		m_spawn[i].pos = ConvertPSXVec3(header.driverSpawn[i].pos, FP_ONE_GEO);
		m_spawn[i].rot = ConvertPSXAngle(header.driverSpawn[i].rot);
	}
	for (size_t i = 0; i < NUM_GRADIENT; i++)
	{
		m_skyGradient[i].posFrom = ConvertFP(header.skyGradient[i].posFrom, 1u);
		m_skyGradient[i].posTo = ConvertFP(header.skyGradient[i].posTo, 1u);
		m_skyGradient[i].colorFrom = ConvertColor(header.skyGradient[i].colorFrom);
		m_skyGradient[i].colorTo = ConvertColor(header.skyGradient[i].colorTo);
	}

	PSX::MeshInfo meshInfo = {};
	file.seekg(offLev + std::streampos(header.offMeshInfo));
	Read(file, meshInfo);

	std::vector<PSX::Vertex> vertices;
	vertices.reserve(meshInfo.numVertices);
	file.seekg(offLev + std::streampos(meshInfo.offVertices));
	for (uint32_t i = 0; i < meshInfo.numVertices; i++)
	{
		PSX::Vertex vertex = {};
		Read(file, vertex);
		vertices.push_back(vertex);
	}

	file.seekg(offLev + std::streampos(meshInfo.offQuadblocks));
	for (uint32_t i = 0; i < meshInfo.numQuadblocks; i++)
	{
		PSX::Quadblock quadblock = {};
		Read(file, quadblock);
		m_quadblocks.emplace_back(quadblock, vertices, [this](const Quadblock& qb) { UpdateFilterRenderData(qb); });
		m_materialToQuadblocks["default"].push_back(i);
	}

	file.seekg(offLev + std::streampos(header.offCheckpointNodes));
	for (uint32_t i = 0; i < header.numCheckpointNodes; i++)
	{
		PSX::Checkpoint checkpoint = {};
		Read(file, checkpoint);
		m_checkpoints.emplace_back(checkpoint, static_cast<int>(i));
	}
	GenerateRenderCheckpointData(m_checkpoints);

	m_loaded = true;
	file.close();
	GenerateRenderLevData();
	return true;
}

bool Level::SaveLEV(const std::filesystem::path& path)
{
	/*
	*	Serialization order:
	*		- offMap
	*		- LevHeader
	*		- MeshInfo
	*		- Textures
	*		- Animated Textures
	*		- Array of quadblocks
	*		- Array of VisibleSets
	*		- Array of PVS
	*		- Array of vertices
	*		- Array of BSP
	*		- Array of checkpoints
	*		- N. Tropy Ghost
	*		- N. Oxide Ghost
	*		- LevelExtraHeader
	*		- NavHeaders
	*		- VisMem
	*		- PointerMap
	*/
	m_hotReloadLevPath = path / (m_name + ".lev");
	std::ofstream file(m_hotReloadLevPath, std::ios::binary);

	if (m_bsp.IsEmpty()) { GenerateBSP(); }

	std::vector<const BSP*> bspNodes = m_bsp.GetTree();
	std::vector<const BSP*> orderedBSPNodes(bspNodes.size());
	for (const BSP* bsp : bspNodes) { orderedBSPNodes[bsp->GetId()] = bsp; }

	PSX::LevHeader header = {};
	const size_t offHeader = 0;
	size_t currOffset = sizeof(header);

	PSX::MeshInfo meshInfo = {};
	const size_t offMeshInfo = currOffset;
	currOffset += sizeof(meshInfo);

	const size_t offTexture = currOffset;
	size_t offAnimData = 0;

	PSX::TextureLayout defaultTex = {};
	defaultTex.clut.self = 32 | (20 << 6);
	defaultTex.texPage.self = (512 >> 6) | ((0 >> 8) << 4) | (0 << 5) | (0 << 7);
	defaultTex.u0 = 0;		defaultTex.v0 = 0;
	defaultTex.u1 = 15;		defaultTex.v1 = 0;
	defaultTex.u2 = 0;		defaultTex.v2 = 15;
	defaultTex.u3 = 15;		defaultTex.v3 = 15;

	PSX::TextureGroup defaultTexGroup = {};
	defaultTexGroup.far = defaultTex;
	defaultTexGroup.middle = defaultTex;
	defaultTexGroup.near = defaultTex;
	defaultTexGroup.mosaic = defaultTex;

	std::vector<uint8_t> animData;
	std::vector<size_t> animPtrMapOffsets;
	std::vector<PSX::TextureGroup> texGroups;
	std::unordered_map<PSX::TextureLayout, size_t> savedLayouts;
	if (UpdateVRM())
	{
		for (auto& [material, texture] : m_materialToTexture)
		{
			std::vector<size_t>& quadIndexes = m_materialToQuadblocks[material];
			for (size_t index : quadIndexes)
			{
				Quadblock& currQuad = m_quadblocks[index];
				if (currQuad.GetAnimated()) { continue; }
				for (size_t i = 0; i < NUM_FACES_QUADBLOCK + 1; i++)
				{
					size_t textureID = 0;
					const QuadUV& uvs = currQuad.GetQuadUV(i);
					PSX::TextureLayout layout = texture.Serialize(uvs);
					if (savedLayouts.contains(layout)) { textureID = savedLayouts[layout]; }
					else
					{
						textureID = texGroups.size();
						savedLayouts[layout] = textureID;

						PSX::TextureGroup texGroup = {};
						texGroup.far = layout;
						texGroup.middle = layout;
						texGroup.near = layout;
						texGroup.mosaic = layout;
						texGroups.push_back(texGroup);
					}
					currQuad.SetTextureID(textureID, i);
				}
			}
		}

		if (!m_animTextures.empty())
		{
			std::vector<std::array<size_t, NUM_FACES_QUADBLOCK>> animOffsetPerQuadblock;
			for (AnimTexture& animTex : m_animTextures)
			{
				const std::vector<AnimTextureFrame>& animFrames = animTex.GetFrames();
				const std::vector<Texture>& animTextures = animTex.GetTextures();
				std::vector<std::vector<size_t>> texgroupIndexesPerFrame(NUM_FACES_QUADBLOCK);
				bool firstFrame = true;
				for (const AnimTextureFrame& frame : animFrames)
				{
					Texture& texture = const_cast<Texture&>(animTextures[frame.textureIndex]);
					for (size_t i = 0; i < NUM_FACES_QUADBLOCK + 1; i++)
					{
						if (i == NUM_FACES_QUADBLOCK && !firstFrame) { continue; }
						size_t textureID = 0;
						const QuadUV& uvs = frame.uvs[i];
						PSX::TextureLayout layout = texture.Serialize(uvs);
						if (savedLayouts.contains(layout)) { textureID = savedLayouts[layout]; }
						else
						{
							textureID = texGroups.size();
							savedLayouts[layout] = textureID;

							PSX::TextureGroup texGroup = {};
							texGroup.far = layout;
							texGroup.middle = layout;
							texGroup.near = layout;
							texGroup.mosaic = layout;
							texGroups.push_back(texGroup);
						}
						if (firstFrame && i == NUM_FACES_QUADBLOCK)
						{
							const std::vector<size_t>& quadblockIndexes = animTex.GetQuadblockIndexes();
							for (size_t index : quadblockIndexes)
							{
								m_quadblocks[index].SetTextureID(textureID, i);
							}
						}
						else { texgroupIndexesPerFrame[i].push_back(textureID); }
					}
					firstFrame = false;
				}
				std::array<size_t, NUM_FACES_QUADBLOCK> offsetPerQuadblock = {};
				for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
				{
					bool foundEquivalent = false;
					for (size_t j = 0; j < i; j++)
					{
						if (texgroupIndexesPerFrame[i] == texgroupIndexesPerFrame[j])
						{
							offsetPerQuadblock[i] = offsetPerQuadblock[j];
							foundEquivalent = true;
							break;
						}
					}
					if (foundEquivalent) { continue; }
					std::vector<uint8_t> buffer = animTex.Serialize(texgroupIndexesPerFrame[i][0], offTexture);
					size_t animTexOffset = animData.size();
					offsetPerQuadblock[i] = animTexOffset;
					animPtrMapOffsets.push_back(animTexOffset);
					for (uint8_t byte : buffer) { animData.push_back(byte); }
					for (size_t j = 0; j < animFrames.size(); j++)
					{
						uint32_t offset = static_cast<uint32_t>((texgroupIndexesPerFrame[i][j] * sizeof(PSX::TextureGroup)) + offTexture);
						size_t offAnimTexArr = animData.size();
						animPtrMapOffsets.push_back(offAnimTexArr);
						for (size_t k = 0; k < sizeof(uint32_t); k++) { animData.push_back(0); }
						memcpy(&animData[offAnimTexArr], &offset, sizeof(uint32_t));
					}
				}
				animOffsetPerQuadblock.push_back(offsetPerQuadblock);
			}

			offAnimData = currOffset + (sizeof(PSX::TextureGroup) * texGroups.size());

			animPtrMapOffsets.push_back(animData.size());
			size_t offEndAnimData = animData.size();
			for (size_t i = 0; i < sizeof(uint32_t); i++) { animData.push_back(0); }
			memcpy(&animData[offEndAnimData], &offAnimData, sizeof(uint32_t));

			for (size_t i = 0; i < m_animTextures.size(); i++)
			{
				const std::vector<size_t>& quadblockIndexes = m_animTextures[i].GetQuadblockIndexes();
				for (size_t index : quadblockIndexes)
				{
					Quadblock& quadblock = m_quadblocks[index];
					for (size_t j = 0; j < NUM_FACES_QUADBLOCK; j++)
					{
						quadblock.SetAnimTextureOffset(animOffsetPerQuadblock[i][j], offAnimData, j);
					}
				}
			}
		}
		else
		{
			offAnimData = currOffset + (sizeof(PSX::TextureGroup) * texGroups.size());
			for (size_t i = 0; i < sizeof(uint32_t); i++) { animData.push_back(0); }
			memcpy(&animData[0], &offAnimData, sizeof(uint32_t));
			animPtrMapOffsets.push_back(0);
		}

		m_hotReloadVRMPath = path / (m_name + ".vrm");
		std::ofstream vrmFile(m_hotReloadVRMPath, std::ios::binary);
		Write(vrmFile, m_vrm.data(), m_vrm.size());
		vrmFile.close();
	}
	else
	{
		texGroups.push_back(defaultTexGroup);
		offAnimData = currOffset + (sizeof(PSX::TextureGroup) * texGroups.size());
		for (size_t i = 0; i < sizeof(uint32_t); i++) { animData.push_back(0); }
		memcpy(&animData[0], &offAnimData, sizeof(uint32_t));
		animPtrMapOffsets.push_back(0);
	}

	currOffset += (sizeof(PSX::TextureGroup) * texGroups.size()) + animData.size();

	const size_t offQuadblocks = currOffset;
	std::vector<std::vector<uint8_t>> serializedBSPs;
	std::vector<std::vector<uint8_t>> serializedQuads;
	std::vector<const Quadblock*> orderedQuads;
	std::unordered_map<Vertex, size_t> vertexMap;
	std::vector<Vertex> orderedVertices;
	size_t bspSize = 0;
	for (const BSP* bsp : orderedBSPNodes)
	{
		serializedBSPs.push_back(bsp->Serialize(currOffset));
		bspSize += serializedBSPs.back().size();
		if (bsp->IsBranch()) { continue; }
		const std::vector<size_t>& quadIndexes = bsp->GetQuadblockIndexes();
		for (const size_t index : quadIndexes)
		{
			const Quadblock& quadblock = m_quadblocks[index];
			std::vector<Vertex> quadVertices = quadblock.GetVertices();
			std::vector<size_t> verticesIndexes;
			for (const Vertex& vertex : quadVertices)
			{
				if (!vertexMap.contains(vertex))
				{
					size_t vertexIndex = orderedVertices.size();
					orderedVertices.push_back(vertex);
					vertexMap[vertex] = vertexIndex;
				}
				verticesIndexes.push_back(vertexMap[vertex]);
			}
			size_t quadIndex = serializedQuads.size();
			serializedQuads.push_back(quadblock.Serialize(quadIndex, offTexture, verticesIndexes));
			orderedQuads.push_back(&quadblock);
			currOffset += serializedQuads.back().size();
		}
	}

	constexpr size_t BITS_PER_SLOT = sizeof(uint32_t) * 8;
	std::vector<std::tuple<std::vector<uint32_t>, size_t>> visibleNodes;
	std::vector<std::tuple<std::vector<uint32_t>, size_t>> visibleQuads;
	std::vector<std::tuple<std::vector<uint32_t>, size_t>> visibleInstances;
	size_t visNodeSize = static_cast<size_t>(std::ceil(static_cast<float>(bspNodes.size()) / static_cast<float>(BITS_PER_SLOT)));
	size_t visQuadSize = static_cast<size_t>(std::ceil(static_cast<float>(m_quadblocks.size()) / static_cast<float>(BITS_PER_SLOT)));
	std::vector<uint32_t> visibleNodeAll(visNodeSize, 0xFFFFFFFF);
	for (const BSP* bsp : orderedBSPNodes)
	{
		if (bsp->GetFlags() & BSPFlags::INVISIBLE) { visibleNodeAll[bsp->GetId() / BITS_PER_SLOT] &= ~(1 << (bsp->GetId() % BITS_PER_SLOT)); }
	}

	std::vector<uint32_t> visibleQuadsAll(visQuadSize, 0xFFFFFFFF);
	size_t quadIndex = 0;
	const bool validVisTree = m_genVisTree && !m_bspVis.IsEmpty();
	const std::vector<const BSP*> bspLeaves = m_bsp.GetLeaves();
	std::unordered_map<size_t, const BSP*> idToLeaf;
	std::unordered_map<const BSP*, size_t> leafToMatrix;
	for (const BSP* leaf : bspLeaves) { idToLeaf[leaf->GetId()] = leaf; }
	for (size_t i = 0; i < bspLeaves.size(); i++) { leafToMatrix[bspLeaves[i]] = i; }
	for (const Quadblock* quad : orderedQuads)
	{
		if (quad->GetFlags() & (QuadFlags::INVISIBLE | QuadFlags::INVISIBLE_TRIGGER))
		{
			visibleQuadsAll[quadIndex / BITS_PER_SLOT] &= ~(1 << (quadIndex % BITS_PER_SLOT));
		}
		if (validVisTree)
		{
			std::vector<uint32_t> visNodes(visNodeSize, 0x0);
			const BSP* bspLeaf = idToLeaf[quad->GetBSPID()];
			const size_t matrixId = leafToMatrix[bspLeaf];
			for (size_t i = 0; i < bspLeaves.size(); i++)
			{
				if (m_bspVis.Get(matrixId, i))
				{
					const BSP* curr = bspLeaves[i];
					while (curr != nullptr)
					{
						visNodes[curr->GetId() / BITS_PER_SLOT] |= (1 << (31 - (curr->GetId() % BITS_PER_SLOT)));
						curr = curr->GetParent();
					}
				}
			}
			visibleNodes.push_back({visNodes, currOffset});
			currOffset += visNodes.size() * sizeof(uint32_t);
		}
		quadIndex++;
	}

	if (!validVisTree)
	{
		visibleNodes.push_back({visibleNodeAll, currOffset});
		currOffset += visibleNodeAll.size() * sizeof(uint32_t);
	}

	visibleQuads.push_back({visibleQuadsAll, currOffset});
	currOffset += visibleQuadsAll.size() * sizeof(uint32_t);

	std::vector<uint32_t> visibleInstancesDummy;
	visibleInstancesDummy.push_back(0);
	visibleInstances.push_back({visibleInstancesDummy, currOffset});
	currOffset += visibleInstancesDummy.size() * sizeof(uint32_t);

	std::unordered_map<PSX::VisibleSet, size_t> visibleSetMap;
	std::vector<PSX::VisibleSet> visibleSets;
	const size_t offVisibleSet = currOffset;

	for (size_t quadCount = 0; quadCount < orderedQuads.size(); quadCount++)
	{
		PSX::VisibleSet set = {};
		if (validVisTree) { set.offVisibleBSPNodes = static_cast<uint32_t>(std::get<size_t>(visibleNodes[quadCount])); }
		else { set.offVisibleBSPNodes = static_cast<uint32_t>(std::get<size_t>(visibleNodes[0])); }
		set.offVisibleQuadblocks = static_cast<uint32_t>(std::get<size_t>(visibleQuads[0]));
		set.offVisibleInstances = static_cast<uint32_t>(std::get<size_t>(visibleInstances[0]));
		set.offVisibleExtra = 0;

		size_t visibleSetIndex = 0;
		if (visibleSetMap.contains(set)) { visibleSetIndex = visibleSetMap.at(set); }
		else
		{
			visibleSetIndex = visibleSets.size();
			visibleSets.push_back(set);
			visibleSetMap[set] = visibleSetIndex;
		}

		PSX::Quadblock* serializedQuad = reinterpret_cast<PSX::Quadblock*>(serializedQuads[quadCount].data());
		serializedQuad->offVisibleSet = static_cast<uint32_t>(offVisibleSet + sizeof(PSX::VisibleSet) * visibleSetIndex);
	}

	currOffset += visibleSets.size() * sizeof(PSX::VisibleSet);

	const size_t offVertices = currOffset;
	std::vector<std::vector<uint8_t>> serializedVertices;
	for (const Vertex& vertex : orderedVertices)
	{
		serializedVertices.push_back(vertex.Serialize());
		currOffset += serializedVertices.back().size();
	}

	const size_t offBSP = currOffset;
	currOffset += bspSize;

	meshInfo.numQuadblocks = static_cast<uint32_t>(serializedQuads.size());
	meshInfo.numVertices = static_cast<uint32_t>(serializedVertices.size());
	meshInfo.offQuadblocks = static_cast<uint32_t>(offQuadblocks);
	meshInfo.offVertices = static_cast<uint32_t>(offVertices);
	meshInfo.unk2 = 0;
	meshInfo.offBSPNodes = static_cast<uint32_t>(offBSP);
	meshInfo.numBSPNodes = static_cast<uint32_t>(serializedBSPs.size());

	const size_t offCheckpoints = currOffset;
	std::vector<std::vector<uint8_t>> serializedCheckpoints;
	for (const Checkpoint& checkpoint : m_checkpoints)
	{
		serializedCheckpoints.push_back(checkpoint.Serialize());
		currOffset += serializedCheckpoints.back().size();
	}

	const size_t offTropyGhost = m_tropyGhost.empty() ? 0 : currOffset;
	currOffset += m_tropyGhost.size();

	const size_t offOxideGhost = m_oxideGhost.empty() ? 0 : currOffset;
	currOffset += m_oxideGhost.size();

	// Note: extraHeader.offsets[MINIMAP] will be updated later after minimap data is serialized
	PSX::LevelExtraHeader extraHeader = {};
	extraHeader.offsets[PSX::LevelExtra::MINIMAP] = 0;
	extraHeader.offsets[PSX::LevelExtra::SPAWN] = 0;
	extraHeader.offsets[PSX::LevelExtra::CAMERA_END_OF_RACE] = 0;
	extraHeader.offsets[PSX::LevelExtra::CAMERA_DEMO] = 0;
	extraHeader.offsets[PSX::LevelExtra::N_TROPY_GHOST] = static_cast<uint32_t>(offTropyGhost);
	extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST] = static_cast<uint32_t>(offOxideGhost);
	extraHeader.offsets[PSX::LevelExtra::CREDITS] = 0;

	// Determine count based on what's enabled
	if (offOxideGhost > 0) { extraHeader.count = PSX::LevelExtra::COUNT; }
	else if (offTropyGhost > 0) { extraHeader.count = PSX::LevelExtra::N_OXIDE_GHOST; }
	else if (m_minimapConfig.IsReady()) { extraHeader.count = PSX::LevelExtra::COUNT; }
	else { extraHeader.count = 0; }

	const size_t offExtraHeader = currOffset;
	currOffset += sizeof(extraHeader);

	constexpr size_t BOT_PATH_COUNT = 3;
	std::vector<PSX::NavHeader> navHeaders(BOT_PATH_COUNT);

	const size_t offNavHeaders = currOffset;
	currOffset += navHeaders.size() * sizeof(PSX::NavHeader);

	std::vector<uint32_t> visMemNodesP1(visNodeSize);
	const size_t offVisMemNodesP1 = currOffset;
	currOffset += visMemNodesP1.size() * sizeof(uint32_t);

	std::vector<uint32_t> visMemQuadsP1(visQuadSize);
	const size_t offVisMemQuadsP1 = currOffset;
	currOffset += visMemQuadsP1.size() * sizeof(uint32_t);

	std::vector<uint32_t> visMemBSPP1(bspNodes.size() * 2);
	const size_t offVisMemBSPP1 = currOffset;
	currOffset += visMemBSPP1.size() * sizeof(uint32_t);

	PSX::VisualMem visMem = {};
	visMem.offNodes[0] = static_cast<uint32_t>(offVisMemNodesP1);
	visMem.offQuads[0] = static_cast<uint32_t>(offVisMemQuadsP1);
	visMem.offBSP[0] = static_cast<uint32_t>(offVisMemBSPP1);
	const size_t offVisMem = currOffset;
	currOffset += sizeof(visMem);

	// Minimap data serialization
	size_t offMinimapStruct = 0;
	size_t offSpawnType1 = 0;
	size_t offLevTexLookup = 0;
	size_t offMinimapIcons = 0;
	std::vector<uint8_t> minimapData;
	std::vector<size_t> minimapPtrMapOffsets;

	if (m_minimapConfig.IsReady())
	{
		// Map struct
		offMinimapStruct = currOffset;
		PSX::Map mapStruct = m_minimapConfig.Serialize();
		size_t mapStructOffset = minimapData.size();
		minimapData.resize(minimapData.size() + sizeof(PSX::Map));
		memcpy(&minimapData[mapStructOffset], &mapStruct, sizeof(PSX::Map));
		currOffset += sizeof(PSX::Map);

		// SpawnType1 header (count + pointer to map struct)
		offSpawnType1 = currOffset;
		PSX::SpawnType1 spawnType1Header = {};
		spawnType1Header.count = PSX::LevelExtra::COUNT; // Must have all entries for minimap
		size_t st1HeaderOffset = minimapData.size();
		minimapData.resize(minimapData.size() + sizeof(PSX::SpawnType1));
		memcpy(&minimapData[st1HeaderOffset], &spawnType1Header, sizeof(PSX::SpawnType1));
		currOffset += sizeof(PSX::SpawnType1);

		// SpawnType1 pointers array - pointer at index 0 points to Map struct
		size_t st1PointersOffset = minimapData.size();
		minimapData.resize(minimapData.size() + sizeof(uint32_t) * PSX::LevelExtra::COUNT);
		uint32_t* st1Pointers = reinterpret_cast<uint32_t*>(&minimapData[st1PointersOffset]);
		st1Pointers[PSX::LevelExtra::MINIMAP] = static_cast<uint32_t>(offMinimapStruct);
		for (size_t i = 1; i < PSX::LevelExtra::COUNT; i++) { st1Pointers[i] = 0; }
		minimapPtrMapOffsets.push_back(currOffset); // Pointer to map struct needs patching
		currOffset += sizeof(uint32_t) * PSX::LevelExtra::COUNT;

		// Icon structs (top and bottom minimap textures)
		offMinimapIcons = currOffset;

		// Create default UV for full texture
		QuadUV defaultUV = {{Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(1.0f, 1.0f)}};

		// Top icon
		PSX::Icon topIcon = {};
		strncpy_s(topIcon.name, sizeof(topIcon.name), "minimap-top", _TRUNCATE);
		topIcon.globalIconArrayIndex = PSX::ICON_INDEX_MAP_TOP;
		topIcon.texLayout = m_minimapConfig.topTexture.Serialize(defaultUV);
		size_t topIconOffset = minimapData.size();
		minimapData.resize(minimapData.size() + sizeof(PSX::Icon));
		memcpy(&minimapData[topIconOffset], &topIcon, sizeof(PSX::Icon));
		currOffset += sizeof(PSX::Icon);

		// Bottom icon
		PSX::Icon bottomIcon = {};
		strncpy_s(bottomIcon.name, sizeof(bottomIcon.name), "minimap-bot", _TRUNCATE);
		bottomIcon.globalIconArrayIndex = PSX::ICON_INDEX_MAP_BOTTOM;
		bottomIcon.texLayout = m_minimapConfig.bottomTexture.Serialize(defaultUV);
		size_t bottomIconOffset = minimapData.size();
		minimapData.resize(minimapData.size() + sizeof(PSX::Icon));
		memcpy(&minimapData[bottomIconOffset], &bottomIcon, sizeof(PSX::Icon));
		currOffset += sizeof(PSX::Icon);

		// LevTexLookup struct
		offLevTexLookup = currOffset;
		PSX::LevTexLookup levTexLookup = {};
		levTexLookup.numIcon = 2;
		levTexLookup.offFirstIcon = static_cast<uint32_t>(offMinimapIcons);
		levTexLookup.numIconGroup = 0;
		levTexLookup.offFirstIconGroupPtr = 0;
		size_t levTexLookupOffset = minimapData.size();
		minimapData.resize(minimapData.size() + sizeof(PSX::LevTexLookup));
		memcpy(&minimapData[levTexLookupOffset], &levTexLookup, sizeof(PSX::LevTexLookup));
		minimapPtrMapOffsets.push_back(currOffset + offsetof(PSX::LevTexLookup, offFirstIcon)); // Pointer to first icon
		currOffset += sizeof(PSX::LevTexLookup);

		// Update extraHeader to point to the minimap struct
		extraHeader.offsets[PSX::LevelExtra::MINIMAP] = static_cast<uint32_t>(offMinimapStruct);
	}

	const size_t offPointerMap = currOffset;

	header.offMeshInfo = static_cast<uint32_t>(offMeshInfo);
	header.offAnimTex = static_cast<uint32_t>(offAnimData);
	for (size_t i = 0; i < NUM_DRIVERS; i++)
	{
		header.driverSpawn[i].pos = ConvertVec3(m_spawn[i].pos, FP_ONE_GEO);
		header.driverSpawn[i].rot = ConvertAngle(m_spawn[i].rot);
	}
	header.config = m_configFlags;
	for (size_t i = 0; i < NUM_GRADIENT; i++)
	{
		header.skyGradient[i].posFrom = ConvertFloat(m_skyGradient[i].posFrom, 1u);
		header.skyGradient[i].posTo = ConvertFloat(m_skyGradient[i].posTo, 1u);
		header.skyGradient[i].colorFrom = ConvertColor(m_skyGradient[i].colorFrom);
		header.skyGradient[i].colorTo = ConvertColor(m_skyGradient[i].colorTo);
	}
	header.stars = ConvertStars(m_stars);
	header.offExtra = static_cast<uint32_t>(offExtraHeader);
	header.numCheckpointNodes = static_cast<uint32_t>(m_checkpoints.size());
	header.offCheckpointNodes = static_cast<uint32_t>(offCheckpoints);
	header.offVisMem = static_cast<uint32_t>(offVisMem);
	header.offLevNavTable = static_cast<uint32_t>(offNavHeaders);

	// Set minimap pointers in header if enabled
	if (m_minimapConfig.IsReady())
	{
		header.offIconsLookup = static_cast<uint32_t>(offLevTexLookup);
		header.offIcons = static_cast<uint32_t>(offMinimapIcons);
	}

#define CALCULATE_OFFSET(s, m, b) static_cast<uint32_t>(offsetof(s, m) + b)

	std::vector<uint32_t> pointerMap =
	{
		CALCULATE_OFFSET(PSX::LevHeader, offMeshInfo, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offExtra, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offCheckpointNodes, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offVisMem, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offAnimTex, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offLevNavTable, offHeader),
		CALCULATE_OFFSET(PSX::MeshInfo, offQuadblocks, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offVertices, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offBSPNodes, offMeshInfo),
		CALCULATE_OFFSET(PSX::VisualMem, offNodes[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offQuads[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offBSP[0], offVisMem),
	};

	// Add minimap header pointers to pointer map
	if (m_minimapConfig.IsReady())
	{
		pointerMap.push_back(CALCULATE_OFFSET(PSX::LevHeader, offIconsLookup, offHeader));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::LevHeader, offIcons, offHeader));
	}

	if (offTropyGhost != 0) { pointerMap.push_back(CALCULATE_OFFSET(PSX::LevelExtraHeader, offsets[PSX::LevelExtra::N_TROPY_GHOST], offExtraHeader)); }
	if (offOxideGhost != 0) { pointerMap.push_back(CALCULATE_OFFSET(PSX::LevelExtraHeader, offsets[PSX::LevelExtra::N_OXIDE_GHOST], offExtraHeader)); }
	if (m_minimapConfig.IsReady()) { pointerMap.push_back(CALCULATE_OFFSET(PSX::LevelExtraHeader, offsets[PSX::LevelExtra::MINIMAP], offExtraHeader)); }

	for (size_t i = 0; i < animPtrMapOffsets.size(); i++)
	{
		pointerMap.push_back(static_cast<uint32_t>(animPtrMapOffsets[i] + offAnimData));
	}

	size_t offCurrQuad = offQuadblocks;
	for (size_t i = 0; i < serializedQuads.size(); i++)
	{
		pointerMap.push_back(CALCULATE_OFFSET(PSX::Quadblock, offMidTextures[0], offCurrQuad));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::Quadblock, offMidTextures[1], offCurrQuad));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::Quadblock, offMidTextures[2], offCurrQuad));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::Quadblock, offMidTextures[3], offCurrQuad));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::Quadblock, offLowTexture, offCurrQuad));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::Quadblock, offVisibleSet, offCurrQuad));
		offCurrQuad += serializedQuads[i].size();
	}

	size_t offCurrNode = offBSP;
	for (size_t i = 0; i < serializedBSPs.size(); i++)
	{
		if (orderedBSPNodes[i]->IsBranch()) { offCurrNode += serializedBSPs[i].size(); continue; }
		size_t visMemListIndex = 2 * i + 1;
		visMemBSPP1[visMemListIndex] = static_cast<uint32_t>(offCurrNode);
		pointerMap.push_back(static_cast<uint32_t>(offVisMemBSPP1 + visMemListIndex * sizeof(uint32_t)));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::BSPLeaf, offQuads, offCurrNode));
		offCurrNode += serializedBSPs[i].size();
	}

	size_t offCurrVisibleSet = offVisibleSet;
	for (const PSX::VisibleSet& visibleSet : visibleSets)
	{
		pointerMap.push_back(CALCULATE_OFFSET(PSX::VisibleSet, offVisibleBSPNodes, offCurrVisibleSet));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::VisibleSet, offVisibleQuadblocks, offCurrVisibleSet));
		pointerMap.push_back(CALCULATE_OFFSET(PSX::VisibleSet, offVisibleInstances, offCurrVisibleSet));
		offCurrVisibleSet += sizeof(PSX::VisibleSet);
	}

	// Add minimap internal pointers to pointer map
	for (size_t offset : minimapPtrMapOffsets)
	{
		pointerMap.push_back(static_cast<uint32_t>(offset));
	}

	const size_t pointerMapBytes = pointerMap.size() * sizeof(uint32_t);

	Write(file, &offPointerMap, sizeof(uint32_t));
	Write(file, &header, sizeof(header));
	Write(file, &meshInfo, sizeof(meshInfo));
	Write(file, texGroups.data(), texGroups.size() * sizeof(PSX::TextureGroup));
	if (!animData.empty()) { Write(file, animData.data(), animData.size()); }
	for (const std::vector<uint8_t>& serializedQuad : serializedQuads) { Write(file, serializedQuad.data(), serializedQuad.size()); }
	for (const auto& tuple : visibleNodes)
	{
		const std::vector<uint32_t>& visibleNode = std::get<0>(tuple);
		Write(file, visibleNode.data(), visibleNode.size() * sizeof(uint32_t));
	}
	for (const auto& tuple : visibleQuads)
	{
		const std::vector<uint32_t>& visibleQuad = std::get<0>(tuple);
		Write(file, visibleQuad.data(), visibleQuad.size() * sizeof(uint32_t));
	}
	for (const auto& tuple : visibleInstances)
	{
		const std::vector<uint32_t>& visibleInst = std::get<0>(tuple);
		Write(file, visibleInst.data(), visibleInst.size() * sizeof(uint32_t));
	}
	Write(file, visibleSets.data(), visibleSets.size() * sizeof(PSX::VisibleSet));
	for (const std::vector<uint8_t>& serializedVertex : serializedVertices) { Write(file, serializedVertex.data(), serializedVertex.size()); }
	for (const std::vector<uint8_t>& serializedBSP : serializedBSPs) { Write(file, serializedBSP.data(), serializedBSP.size()); }
	for (const std::vector<uint8_t>& serializedCheckpoint : serializedCheckpoints) { Write(file, serializedCheckpoint.data(), serializedCheckpoint.size()); }
	if (!m_tropyGhost.empty()) { Write(file, m_tropyGhost.data(), m_tropyGhost.size()); }
	if (!m_oxideGhost.empty()) { Write(file, m_oxideGhost.data(), m_oxideGhost.size()); }
	Write(file, &extraHeader, sizeof(extraHeader));
	Write(file, navHeaders.data(), navHeaders.size() * sizeof(PSX::NavHeader));
	Write(file, visMemNodesP1.data(), visMemNodesP1.size() * sizeof(uint32_t));
	Write(file, visMemQuadsP1.data(), visMemQuadsP1.size() * sizeof(uint32_t));
	Write(file, visMemBSPP1.data(), visMemBSPP1.size() * sizeof(uint32_t));
	Write(file, &visMem, sizeof(visMem));
	// Write minimap data if present
	if (!minimapData.empty()) { Write(file, minimapData.data(), minimapData.size()); }
	Write(file, &pointerMapBytes, sizeof(uint32_t));
	Write(file, pointerMap.data(), pointerMapBytes);
	file.close();
	return true;
}

bool Level::LoadOBJ(const std::filesystem::path& objFile)
{
	std::string line;
	std::ifstream file(objFile);
	m_name = objFile.filename().replace_extension().string();
	m_parentPath = objFile.parent_path();

	bool ret = true;
	std::unordered_map<std::string, std::vector<Tri>> triMap;
	std::unordered_map<std::string, std::vector<Quad>> quadMap;
	std::unordered_map<std::string, std::vector<Vec3>> normalMap;
	std::unordered_map<std::string, std::string> materialMap;
	std::unordered_map<std::string, bool> meshMap;
	std::unordered_set<std::string> materials;
	std::vector<Point> vertices;
	std::vector<Vec3> normals;
	std::vector<Vec2> uvs;
	std::string currQuadblockName;
	bool currQuadblockGoodUV = true;
	size_t quadblockCount = 0;
	while (std::getline(file, line))
	{
		std::vector<std::string> tokens = Split(line);
		if (tokens.empty()) { continue; }
		const std::string& command = tokens[0];
		if (command == "v")
		{
			if (tokens.size() < 4) { continue; }
			vertices.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
			if (tokens.size() < 7) { continue; }
			vertices.back().color = Color(std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]));
		}
		else if (command == "vn")
		{
			if (tokens.size() < 4) { continue; }
			normals.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
		}
		else if (command == "vt")
		{
			if (tokens.size() < 3) { continue; }
			Vec2 uv = {std::stof(tokens[1]), std::stof(tokens[2])};
			if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
			{
				m_invalidQuadblocks.emplace_back(currQuadblockName, "WARNING: UV outside of expect range [0.0f, 1.0f].");
			}
			auto Wrap = [](float x)
				{
					if (x >= 0.0f && x <= 1.0f) { return x; }
					float r = fmodf(x, 1.0f);
					if (r < 0.0f) { r += 1.0f; }
					if (r == 0.0f && x > 0.0f) { r = 1.0f; }
					return r;
				};
			uv.x = Wrap(uv.x);
			uv.y = 1.0f - Wrap(uv.y);
			uvs.emplace_back(uv);
		}
		else if (command == "o")
		{
			if (tokens.size() < 2 || meshMap.contains(tokens[1]))
			{
				ret = false;
				m_invalidQuadblocks.emplace_back(tokens[1], "Duplicated mesh name.");
				continue;
			}
			currQuadblockName = tokens[1];
			currQuadblockGoodUV = true;
			meshMap[currQuadblockName] = false;
			quadblockCount++;
		}
		else if (command == "usemtl")
		{
			if (tokens.size() < 2) { continue; }
			if (currQuadblockName.empty() || materialMap.contains(currQuadblockName)) { continue; } /* TODO: return false, generate error message */
			materialMap[currQuadblockName] = tokens[1];
		}
		else if (command == "f")
		{
			if (currQuadblockName.empty()) { return false; }
			if (tokens.size() < 4) { continue; }

			if (meshMap.contains(currQuadblockName) && meshMap.at(currQuadblockName))
			{
				ret = false;
				m_invalidQuadblocks.emplace_back(currQuadblockName, "Triblock and Quadblock merged in the same mesh.");
				continue;
			}

			bool isQuadblock = tokens.size() == 5;

			std::vector<std::string> token0 = Split(tokens[1], '/');
			std::vector<std::string> token1 = Split(tokens[2], '/');
			std::vector<std::string> token2 = Split(tokens[3], '/');

			const size_t EXPECTED_INFORMATION_PER_TOKEN = 3; /* pos, opt uvs, normals */
			if (token0.size() < EXPECTED_INFORMATION_PER_TOKEN ||
				token1.size() < EXPECTED_INFORMATION_PER_TOKEN ||
				token2.size() < EXPECTED_INFORMATION_PER_TOKEN)
			{
				ret = false;
				m_invalidQuadblocks.emplace_back(currQuadblockName, "Missing vertex normals.");
				continue;
			}

			int i0 = std::stoi(token0[0]) - 1;
			int i1 = std::stoi(token1[0]) - 1;
			int i2 = std::stoi(token2[0]) - 1;
			int ni0 = std::stoi(token0[2]) - 1;
			int ni1 = std::stoi(token1[2]) - 1;
			int ni2 = std::stoi(token2[2]) - 1;
			normalMap[currQuadblockName].push_back(normals[ni0]);
			normalMap[currQuadblockName].push_back(normals[ni1]);
			normalMap[currQuadblockName].push_back(normals[ni2]);

			vertices[i0].normal = normals[ni0];
			vertices[i1].normal = normals[ni1];
			vertices[i2].normal = normals[ni2];

			if (currQuadblockGoodUV)
			{
				int uv0 = 0;
				int uv1 = 0;
				int uv2 = 0;
				try
				{
					uv0 = std::stoi(token0[1]) - 1;
					uv1 = std::stoi(token1[1]) - 1;
					uv2 = std::stoi(token2[1]) - 1;
				}
				catch (...) { currQuadblockGoodUV = false; }

				if (currQuadblockGoodUV)
				{
					vertices[i0].uv = uvs[uv0];
					vertices[i1].uv = uvs[uv1];
					vertices[i2].uv = uvs[uv2];
				}
			}

			if (!currQuadblockGoodUV)
			{
				m_invalidQuadblocks.emplace_back(currQuadblockName, "Missing UVs.");
			}

			bool blockFetched = false;
			if (isQuadblock)
			{
				std::vector<std::string> token3 = Split(tokens[4], '/');
				int i3 = std::stoi(token3[0]) - 1;
				int ni3 = std::stoi(token3[2]) - 1;
				normalMap[currQuadblockName].push_back(normals[ni3]);
				vertices[i3].normal = normals[ni3];
				if (currQuadblockGoodUV)
				{
					int uv3 = std::stoi(token3[1]) - 1;
					vertices[i3].uv = uvs[uv3];
				}

				if (!quadMap.contains(currQuadblockName)) { quadMap[currQuadblockName] = std::vector<Quad>(); }
				quadMap[currQuadblockName].emplace_back(vertices[i0], vertices[i1], vertices[i2], vertices[i3]);
				blockFetched = quadMap[currQuadblockName].size() == 4;
			}
			else
			{
				if (!triMap.contains(currQuadblockName)) { triMap[currQuadblockName] = std::vector<Tri>(); }
				triMap[currQuadblockName].emplace_back(vertices[i0], vertices[i1], vertices[i2]);
				blockFetched = triMap[currQuadblockName].size() == 4;
			}

			if (blockFetched)
			{
				Vec3 averageNormal = Vec3();
				for (const Vec3& normal : normalMap[currQuadblockName])
				{
					averageNormal = averageNormal + normal;
				}
				averageNormal = averageNormal / averageNormal.Length();
				std::string material;
				if (materialMap.contains(currQuadblockName))
				{
					material = materialMap[currQuadblockName];
					m_materialToQuadblocks[material].push_back(m_quadblocks.size());
					if (!materials.contains(material))
					{
						materials.insert(material);
						m_materialToTexture[material] = Texture();
						m_propTerrain.SetDefaultValue(material, TerrainType::DEFAULT);
						m_propQuadFlags.SetDefaultValue(material, QuadFlags::DEFAULT);
						m_propDoubleSided.SetDefaultValue(material, false);
						m_propCheckpoints.SetDefaultValue(material, false);
						m_propTurboPads.SetDefaultValue(material, QuadblockTrigger::NONE);
						m_propCheckpointPathable.SetDefaultValue(material, true);
						m_propTerrain.RegisterMaterial(this);
						m_propQuadFlags.RegisterMaterial(this);
						m_propDoubleSided.RegisterMaterial(this);
						m_propCheckpoints.RegisterMaterial(this);
						m_propTurboPads.RegisterMaterial(this);
						m_propSpeedImpact.RegisterMaterial(this);
						m_propCheckpointPathable.RegisterMaterial(this);
					}
				}
				bool sameUVs = true;
				if (isQuadblock)
				{
					Quad& q0 = quadMap[currQuadblockName][0];
					Quad& q1 = quadMap[currQuadblockName][1];
					Quad& q2 = quadMap[currQuadblockName][2];
					Quad& q3 = quadMap[currQuadblockName][3];
					const Vec2& targetUV = q0.p[0].uv;
					for (size_t i = 0; i < 4; i++)
					{
						const Quad& q = quadMap[currQuadblockName][i];
						for (size_t j = 0; j < 4; j++)
						{
							if (q.p[j].uv != targetUV) { sameUVs = false; break; }
						}
						if (!sameUVs) { break; }
					}
					try
					{
						m_quadblocks.emplace_back(currQuadblockName, q0, q1, q2, q3, averageNormal, material, currQuadblockGoodUV, [this](const Quadblock& qb) { UpdateFilterRenderData(qb); });
						meshMap[currQuadblockName] = true;
					}
					catch (const QuadException& e)
					{
						ret = false;
						m_invalidQuadblocks.emplace_back(currQuadblockName, e.what());
					}
				}
				else
				{
					Tri& t0 = triMap[currQuadblockName][0];
					Tri& t1 = triMap[currQuadblockName][1];
					Tri& t2 = triMap[currQuadblockName][2];
					Tri& t3 = triMap[currQuadblockName][3];
					const Vec2& targetUV = t0.p[0].uv;
					for (size_t i = 0; i < 4; i++)
					{
						const Tri& t = triMap[currQuadblockName][i];
						for (size_t j = 0; j < 3; j++)
						{
							if (t.p[j].uv != targetUV) { sameUVs = false; break; }
						}
						if (!sameUVs) { break; }
					}
					try
					{
						m_quadblocks.emplace_back(currQuadblockName, t0, t1, t2, t3, averageNormal, material, currQuadblockGoodUV, [this](const Quadblock& qb) { UpdateFilterRenderData(qb); });
						meshMap[currQuadblockName] = true;
					}
					catch (const QuadException& e)
					{
						ret = false;
						m_invalidQuadblocks.emplace_back(currQuadblockName, e.what());
					}
				}
				if (sameUVs)
				{
					m_invalidQuadblocks.emplace_back(currQuadblockName, "Degenerated UV data.");
				}
			}
		}
	}
	file.close();

	m_showLogWindow = !m_invalidQuadblocks.empty();

	if (!materials.empty())
	{
		std::filesystem::path mtlPath = m_parentPath / (objFile.stem().string() + ".mtl");
		if (std::filesystem::exists(mtlPath))
		{
			std::ifstream mtl(mtlPath);
			std::string currMaterial;
			while (std::getline(mtl, line))
			{
				std::vector<std::string> tokens = Split(line);
				if (tokens.empty()) { continue; }

				const std::string& command = tokens[0];
				if (command == "newmtl") { currMaterial = tokens[1]; }
				else if (command == "map_Kd")
				{
					std::string imagePath = tokens[1];
					for (size_t i = 2; i < tokens.size(); i++) { imagePath += " " + tokens[i]; }
					std::filesystem::path materialPath = imagePath;
					if (!std::filesystem::exists(materialPath)) { materialPath = m_parentPath / materialPath.filename(); }
					if (std::filesystem::exists(materialPath))
					{
						m_materialToTexture[currMaterial] = Texture(materialPath);
					}
				}
			}
		}
	}

	if (ret)
	{
		for (const auto& [material, texture] : m_materialToTexture)
		{
			const std::filesystem::path& texPath = texture.GetPath();
			const std::vector<size_t>& quadblockIndexes = m_materialToQuadblocks[material];
			for (const size_t index : quadblockIndexes) { m_quadblocks[index].SetTexPath(texPath); }
		}
	}

	if (ret)
	{
		for (const auto& [material, texture] : m_materialToTexture)
		{
			const std::filesystem::path& texPath = texture.GetPath();
			const std::vector<size_t>& quadblockIndexes = m_materialToQuadblocks[material];
			for (const size_t index : quadblockIndexes) { m_quadblocks[index].SetTexPath(texPath); }
		}
	}

	if (quadblockCount != m_quadblocks.size())
	{
		m_showLogWindow = true;
		m_logMessage = "Error: number of meshes does not equal number of quadblocks.\n\nNumber of meshes found: " + std::to_string(quadblockCount) + "\nNumber of quadblocks: " + std::to_string(m_quadblocks.size());;
		m_logMessage += "\n\nThe following meshes are not a quadblock:\n\n";
		constexpr size_t QUADS_PER_LINE = 10;
		size_t invalidQuadblocks = 0;
		for (auto& [name, status] : meshMap)
		{
			if (status) { continue; }
			m_logMessage += name + ", ";
			if (((invalidQuadblocks + 1) % QUADS_PER_LINE) == 0) { m_logMessage += "\n"; }
			invalidQuadblocks++;
		}
		ret = false;
	}
	m_loaded = ret;

	if (m_loaded)
	{
		std::filesystem::path presetFolder = m_parentPath / (m_name + "_presets");
		if (std::filesystem::is_directory(presetFolder))
		{
			for (const auto& entry : std::filesystem::directory_iterator(presetFolder))
			{
				const std::filesystem::path json = entry.path();
				if (json.has_extension() && json.extension() == ".json") { LoadPreset(json); }
			}
		}
	}
	GenerateRenderLevData();
	GenerateBSP();
	return ret;
}

bool Level::StartEmuIPC(const std::string& emulator)
{
	constexpr size_t PSX_RAM_SIZE = 0x800000;
	int pid = Process::GetPID(emulator);
	if (pid == Process::INVALID_PID || !Process::OpenMemoryMap(emulator + "_" + std::to_string(pid), PSX_RAM_SIZE)) { return false; }
	return true;
}

bool Level::HotReload(const std::string& levPath, const std::string& vrmPath, const std::string& emulator)
{
	bool vrmOnly = false;
	if (levPath.empty())
	{
		if (vrmPath.empty()) { return false; }
		vrmOnly = true;
	}

	if (!StartEmuIPC(emulator)) { return false; }

	constexpr size_t GAMEMODE_ADDR = 0x80096b20;
	constexpr uint32_t GAME_PAUSED = 0xF;
	if (Process::At<uint32_t>(GAMEMODE_ADDR) & GAME_PAUSED) { return false; }

	constexpr size_t VRAM_ADDR = 0x80200000;
	constexpr size_t RAM_ADDR = 0x80300000;
	constexpr size_t SIGNAL_ADDR = 0x8000C000;
	constexpr size_t SIGNAL_ADDR_VRAM_ONLY = 0x8000C004;
	constexpr int HOT_RELOAD_START = 1;
	constexpr int HOT_RELOAD_READY = 3;
	constexpr int HOT_RELOAD_EXEC = 4;

	if (!vrmOnly)
	{
		Process::At<int32_t>(SIGNAL_ADDR) = HOT_RELOAD_START;
		while (Process::At<volatile int32_t>(SIGNAL_ADDR) != HOT_RELOAD_READY) {}
	}
	if (!vrmPath.empty())
	{
		std::vector<uint8_t> vrm;
		ReadBinaryFile(vrm, vrmPath);
		for (size_t i = 0; i < vrm.size(); i++) { Process::At<uint8_t>(VRAM_ADDR + i) = vrm[i]; }
	}

	if (!levPath.empty())
	{
		std::vector<uint8_t> lev;
		ReadBinaryFile(lev, levPath);
		for (size_t i = 0; i < lev.size(); i++) { Process::At<uint8_t>(RAM_ADDR + i) = lev[i]; }
	}

	if (vrmOnly) { Process::At<int32_t>(SIGNAL_ADDR_VRAM_ONLY) = 1; }
	else { Process::At<int32_t>(SIGNAL_ADDR) = HOT_RELOAD_EXEC; }

	return true;
}

bool Level::SaveGhostData(const std::string& emulator, const std::filesystem::path& path)
{
	constexpr size_t SIGNAL_ADDR = 0x8000C008;
	if (!StartEmuIPC(emulator) || Process::At<int32_t>(SIGNAL_ADDR) == 0) { return false; }

	std::vector<uint8_t> data;
	constexpr size_t GHOST_SIZE_ADDR = 0x80270038;
	constexpr size_t GHOST_DATA_ADDR = 0x8027003C;

	size_t fileSize = static_cast<size_t>(Process::At<uint32_t>(GHOST_SIZE_ADDR));
	if (fileSize != GHOST_DATA_FILESIZE) { return false; }

	data.resize(fileSize);
	for (size_t i = 0; i < data.size(); i++) { data[i] = Process::At<uint8_t>(GHOST_DATA_ADDR + i); }
	Process::At<int32_t>(SIGNAL_ADDR) = 0;

	std::ofstream file(path, std::ios::binary);
	Write(file, data.data(), data.size() * sizeof(uint8_t));
	file.close();
	return true;
}

bool Level::SetGhostData(const std::filesystem::path& path, bool tropy)
{
	std::vector<uint8_t> data;
	ReadBinaryFile(data, path);
	if (data.size() != GHOST_DATA_FILESIZE) { return false; }

	if (tropy) { m_tropyGhost.resize(GHOST_DATA_FILESIZE); }
	else { m_oxideGhost.resize(GHOST_DATA_FILESIZE); }
	memcpy(tropy ? m_tropyGhost.data() : m_oxideGhost.data(), data.data(), data.size());
	return true;
}

bool Level::UpdateVRM()
{
	std::vector<Texture*> textures;
	std::vector<std::tuple<Texture*, Texture*>> copyTextureAttributes;
	for (auto& [material, texture] : m_materialToTexture)
	{
		bool foundEqual = false;
		for (Texture* addedTexture : textures)
		{
			if (texture == *addedTexture)
			{
				copyTextureAttributes.push_back({addedTexture, &texture});
				foundEqual = true;
				break;
			}
		}
		if (foundEqual) { continue; }
		textures.push_back(&texture);
	}

	for (const AnimTexture& animTex : m_animTextures)
	{
		const std::vector<AnimTextureFrame>& animFrames = animTex.GetFrames();
		const std::vector<Texture>& animTextures = animTex.GetTextures();
		for (const AnimTextureFrame& frame : animFrames)
		{
			bool foundEqual = false;
			Texture* texture = const_cast<Texture*>(&animTextures[frame.textureIndex]);
			for (Texture* addedTexture : textures)
			{
				if (*texture == *addedTexture)
				{
					copyTextureAttributes.push_back({addedTexture, texture});
					foundEqual = true;
					break;
				}
			}
			if (foundEqual) { continue; }
			textures.push_back(texture);
		}
	}

	// Add minimap textures if enabled
	if (m_minimapConfig.IsReady())
	{
		std::vector<Texture*> minimapTextures = m_minimapConfig.GetTextures();
		for (Texture* tex : minimapTextures)
		{
			bool foundEqual = false;
			for (Texture* addedTexture : textures)
			{
				if (*tex == *addedTexture)
				{
					copyTextureAttributes.push_back({addedTexture, tex});
					foundEqual = true;
					break;
				}
			}
			if (foundEqual) { continue; }
			textures.push_back(tex);
		}
	}

	m_vrm = PackVRM(textures);
	if (m_vrm.empty()) { return false; }

	for (auto& [from, to] : copyTextureAttributes)
	{
		to->CopyVRAMAttributes(*from);
	}

	return true;
}

bool Level::UpdateAnimTextures(float deltaTime)
{
	bool changed = false;
	if (m_animTextures.size() != m_lastAnimTextureCount)
	{
		m_lastAnimTextureCount = m_animTextures.size();
		changed = true;
	}

	for (AnimTexture& animTex : m_animTextures)
	{
		if (animTex.AdvanceRender(deltaTime)) { changed = true; }
	}

	return changed;
}

void Level::GeomPoint(const Vertex* verts, int ind, std::vector<float>& data)
{
	//for present info, data needs to be pushed in the same order as it appears in VBufDataType
	data.push_back(verts[ind].m_pos.x);
	data.push_back(verts[ind].m_pos.y);
	data.push_back(verts[ind].m_pos.z);
	Color col = verts[ind].GetColor(true);
	data.push_back(col.Red());
	data.push_back(col.Green());
	data.push_back(col.Blue());
	data.push_back(verts[ind].m_normal.x);
	data.push_back(verts[ind].m_normal.y);
	data.push_back(verts[ind].m_normal.z);
}

void Level::GeomUVs(const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& uvs, int quadInd, int vertInd, std::vector<float>& data, int textureIndex)
{
	/* 062 is triblock
	p0 -- p1 -- p2
	|  q0 |  q1 |
	p3 -- p4 -- p5
	|  q2 |  q3 |
	p6 -- p7 -- p8
	*/
	const QuadUV& quv = uvs[quadInd];
	constexpr int NUM_VERTICES_QUAD = 4;
	constexpr int uvVertInd[NUM_FACES_QUADBLOCK + 1][NUM_VERTICES_QUAD] =
	{
		{0, 1, 3, 4},
		{1, 2, 4, 5},
		{3, 4, 6, 7},
		{4, 5, 7, 8},
		{0, 2, 6, 8},
	};

	int vertIndInUvs = 0;
	for (int i = 0; i < NUM_VERTICES_QUAD; i++)
	{
		if (vertInd == uvVertInd[quadInd][i]) { vertIndInUvs = i; break; }
	}
	const Vec2& uv = quv[vertIndInUvs];
	data.push_back(uv.x);
	data.push_back(uv.y);
	data.push_back(std::bit_cast<float>(textureIndex)); //todo, do this in geometry shader since ID is the same for all 3 verts for this tri
}

void Level::GeomOctopoint(const Vertex* verts, int ind, std::vector<float>& data)
{
	constexpr float radius = 0.5f;
	constexpr float sqrtThree = 1.44224957031f;

	Vertex v = Vertex(verts[ind]);
	v.m_pos.x += radius; v.m_normal = Vec3(1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x -= radius;
	v.m_pos.y += radius; GeomPoint(&v, 0, data); v.m_pos.y -= radius;
	v.m_pos.z += radius; GeomPoint(&v, 0, data); v.m_pos.z -= radius;

	v.m_pos.x -= radius; v.m_normal = Vec3(-1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x += radius;
	v.m_pos.y += radius; GeomPoint(&v, 0, data); v.m_pos.y -= radius;
	v.m_pos.z += radius; GeomPoint(&v, 0, data); v.m_pos.z -= radius;

	v.m_pos.x += radius; v.m_normal = Vec3(1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x -= radius;
	v.m_pos.y -= radius; GeomPoint(&v, 0, data); v.m_pos.y += radius;
	v.m_pos.z += radius; GeomPoint(&v, 0, data); v.m_pos.z -= radius;

	v.m_pos.x += radius; v.m_normal = Vec3(1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x -= radius;
	v.m_pos.y += radius; GeomPoint(&v, 0, data); v.m_pos.y -= radius;
	v.m_pos.z -= radius; GeomPoint(&v, 0, data); v.m_pos.z += radius;

	v.m_pos.x -= radius; v.m_normal = Vec3(-1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x += radius;
	v.m_pos.y -= radius; GeomPoint(&v, 0, data); v.m_pos.y += radius;
	v.m_pos.z += radius; GeomPoint(&v, 0, data); v.m_pos.z -= radius;

	v.m_pos.x += radius; v.m_normal = Vec3(1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x -= radius;
	v.m_pos.y -= radius; GeomPoint(&v, 0, data); v.m_pos.y += radius;
	v.m_pos.z -= radius; GeomPoint(&v, 0, data); v.m_pos.z += radius;

	v.m_pos.x -= radius; v.m_normal = Vec3(-1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x += radius;
	v.m_pos.y += radius; GeomPoint(&v, 0, data); v.m_pos.y -= radius;
	v.m_pos.z -= radius; GeomPoint(&v, 0, data); v.m_pos.z += radius;

	v.m_pos.x -= radius; v.m_normal = Vec3(-1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree); GeomPoint(&v, 0, data); v.m_pos.x += radius;
	v.m_pos.y -= radius; GeomPoint(&v, 0, data); v.m_pos.y += radius;
	v.m_pos.z -= radius; GeomPoint(&v, 0, data); v.m_pos.z += radius;
}

void Level::GeomBoundingRect(const BSP* bsp, int depth, std::vector<float>& data)
{
	constexpr float sqrtThree = 1.44224957031f;

	if (GuiRenderSettings::bspTreeMaxDepth < depth)
	{
		GuiRenderSettings::bspTreeMaxDepth = depth;
	}
	const BoundingBox& bb = bsp->GetBoundingBox();
	Color c = Color(depth * 30.0, 1.0, 1.0);
	Vertex verts[] = {
		Vertex(Point(bb.min.x, bb.min.y, bb.min.z, c.r, c.g, c.b)), //---
		Vertex(Point(bb.min.x, bb.min.y, bb.max.z, c.r, c.g, c.b)), //--+
		Vertex(Point(bb.min.x, bb.max.y, bb.min.z, c.r, c.g, c.b)), //-+-
		Vertex(Point(bb.max.x, bb.min.y, bb.min.z, c.r, c.g, c.b)), //+--
		Vertex(Point(bb.max.x, bb.max.y, bb.min.z, c.r, c.g, c.b)), //++-
		Vertex(Point(bb.min.x, bb.max.y, bb.max.z, c.r, c.g, c.b)), //-++
		Vertex(Point(bb.max.x, bb.min.y, bb.max.z, c.r, c.g, c.b)), //+-+
		Vertex(Point(bb.max.x, bb.max.y, bb.max.z, c.r, c.g, c.b)), //+++
	};
	//these normals are octohedral, should technechally be duplicated and vertex normals should probably be for faces.
	verts[0].m_normal = Vec3(-1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree);
	verts[1].m_normal = Vec3(-1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree);
	verts[2].m_normal = Vec3(-1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree);
	verts[3].m_normal = Vec3(1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree);
	verts[4].m_normal = Vec3(1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree);
	verts[5].m_normal = Vec3(-1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree);
	verts[6].m_normal = Vec3(1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree);
	verts[7].m_normal = Vec3(1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree);

	if (GuiRenderSettings::bspTreeTopDepth <= depth && GuiRenderSettings::bspTreeBottomDepth >= depth)
	{
		constexpr int NUM_EDGES = 12;
		constexpr int EDGE_VERTS = 2;
		const int edgeIndexes[NUM_EDGES][EDGE_VERTS] =
		{
			{0, 1}, {2, 5}, {3, 6}, {4, 7},
			{0, 2}, {1, 5}, {3, 4}, {6, 7},
			{0, 3}, {1, 6}, {2, 4}, {5, 7},
		};

		for (int i = 0; i < NUM_EDGES; i++)
		{
			const int a = edgeIndexes[i][0];
			const int b = edgeIndexes[i][1];
			GeomPoint(verts, a, data);
			GeomPoint(verts, b, data);
			GeomPoint(verts, b, data);
		}
	}

	if (bsp->GetLeftChildren() != nullptr)
	{
		GeomBoundingRect(bsp->GetLeftChildren(), depth + 1, data);
	}
	if (bsp->GetRightChildren() != nullptr)
	{
		GeomBoundingRect(bsp->GetRightChildren(), depth + 1, data);
	}
}

int Level::GetTextureIndex(const std::filesystem::path& texPath)
{
	auto findResult = m_textureStorePaths.find(texPath);
	if (findResult != m_textureStorePaths.end()) { return findResult->second; }
	int texIndex = static_cast<int>(m_textureStorePaths.size());
	m_textureStorePaths[texPath] = texIndex;
	m_highLODMesh.UpdateTextureStore(texPath);
	m_lowLODMesh.UpdateTextureStore(texPath);
	return texIndex;
}

void Level::GenerateRenderLevData()
{
	/* 062 is triblock
		p0 -- p1 -- p2
		|  q0 |  q1 |
		p3 -- p4 -- p5
		|  q2 |  q3 |
		p6 -- p7 -- p8
	*/
	std::vector<float> highLODData, lowLODData, vertexHighLODData, vertexLowLODData;
	std::vector<float> filterHighLODData, filterLowLODData;
	size_t highLodVertexCount = 0;
	size_t lowLodVertexCount = 0;
	size_t vertexHighLodVertexCount = 0;
	size_t vertexLowLodVertexCount = 0;
	size_t filterHighLodVertexCount = 0;
	size_t filterLowLodVertexCount = 0;

	struct AnimRenderData
	{
		const AnimTextureFrame* frame;
		int textureIndex;
	};

	std::vector<AnimRenderData> animRenderData(m_quadblocks.size());
	std::vector<bool> hasAnimRenderData(m_quadblocks.size(), false);
	for (const AnimTexture& animTex : m_animTextures)
	{
		if (!animTex.IsPopulated()) { continue; }
		const std::vector<Texture>& textures = animTex.GetTextures();
		const AnimTextureFrame& frame = animTex.GetRenderFrame();
		int animTexIndex = GetTextureIndex(textures[frame.textureIndex].GetPath());
		for (size_t qbIndex : animTex.GetQuadblockIndexes())
		{
			animRenderData[qbIndex] = {&frame, animTexIndex};
			hasAnimRenderData[qbIndex] = true;
		}
	}

	for (size_t matIndex = 0; matIndex < m_materialToQuadblocks.size(); matIndex++)
	{
		auto begin = m_materialToQuadblocks.begin();
		std::advance(begin, matIndex);
		const std::string& mat = begin->first;
		const std::vector<size_t>& quadblockIndexes = begin->second;
		const int baseTexIndex = GetTextureIndex(m_materialToTexture[mat].GetPath());
		for (size_t qbIndexIndex = 0; qbIndexIndex < quadblockIndexes.size(); qbIndexIndex++)
		{
			size_t qbIndex = quadblockIndexes[qbIndexIndex];
			Quadblock& qb = m_quadblocks[qbIndex];
			const Vertex* verts = qb.GetUnswizzledVertices();
			const AnimRenderData* animData = hasAnimRenderData[qbIndex] ? &animRenderData[qbIndex] : nullptr;
			const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& uvs = animData ? animData->frame->uvs : qb.GetUVs();
			const int texIndex = animData ? animData->textureIndex : baseTexIndex;
			const size_t highLodBaseIndex = highLodVertexCount;
			const size_t lowLodBaseIndex = lowLodVertexCount;
			const size_t vertexHighLodBaseIndex = vertexHighLodVertexCount;
			const size_t vertexLowLodBaseIndex = vertexLowLodVertexCount;
			const size_t filterHighLodBaseIndex = filterHighLodVertexCount;
			const size_t filterLowLodBaseIndex = filterLowLodVertexCount;
			qb.SetRenderIndices(highLodBaseIndex, lowLodBaseIndex, highLodBaseIndex, lowLodBaseIndex, vertexHighLodBaseIndex, vertexLowLodBaseIndex);
			qb.SetRenderFilterIndices(filterHighLodBaseIndex, filterLowLodBaseIndex);
			const Color& filterColor = qb.GetFilter() ? qb.GetFilterColor() : Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u));
			Vertex filterVerts[NUM_VERTICES_QUADBLOCK];
			for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
			{
				const Vec3& pos = verts[i].m_pos;
				Vertex v = Vertex(Point(pos.x, pos.y, pos.z, filterColor.r, filterColor.g, filterColor.b));
				v.m_normal = verts[i].m_normal;
				filterVerts[i] = v;
			}

			//clockwise point ordering
			if (qb.IsQuadblock())
			{
				constexpr size_t HIGH_LOD_VERT_COUNT = 9;
				const std::array<int, HIGH_LOD_VERT_COUNT> highLODIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8};
				for (int index : highLODIndexes)
				{
					GeomOctopoint(verts, index, vertexHighLODData);
					vertexHighLodVertexCount += 24;
				}

				constexpr size_t LOW_LOD_VERT_COUNT = 4;
				const std::array<int, LOW_LOD_VERT_COUNT> lowLODIndexes = {0, 2, 6, 8};
				for (int index : lowLODIndexes)
				{
					GeomOctopoint(verts, index, vertexLowLODData);
					vertexLowLodVertexCount += 24;
				}

				for (int triIndex = 0; triIndex < 8; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, highLODData);
					GeomUVs(uvs, triIndex / 2, firstPointIndex, highLODData, texIndex); //triIndex / 2 (integer division) because 2 tris per quad

					int secondPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, highLODData);
					GeomUVs(uvs, triIndex / 2, secondPointIndex, highLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, highLODData);
					GeomUVs(uvs, triIndex / 2, thirdPointIndex, highLODData, texIndex);

					highLodVertexCount += 3;
				}

				for (int triIndex = 0; triIndex < 2; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, lowLODData);
					GeomUVs(uvs, 4, firstPointIndex, lowLODData, texIndex);

					int secondPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, lowLODData);
					GeomUVs(uvs, 4, secondPointIndex, lowLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, lowLODData);
					GeomUVs(uvs, 4, thirdPointIndex, lowLODData, texIndex);

					lowLodVertexCount += 3;
				}

				for (int triIndex = 0; triIndex < 8; triIndex++)
				{
					GeomPoint(filterVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][0], filterHighLODData);
					GeomPoint(filterVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][1], filterHighLODData);
					GeomPoint(filterVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][2], filterHighLODData);
					filterHighLodVertexCount += 3;
				}

				for (int triIndex = 0; triIndex < 2; triIndex++)
				{
					GeomPoint(filterVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][0], filterLowLODData);
					GeomPoint(filterVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][1], filterLowLODData);
					GeomPoint(filterVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][2], filterLowLODData);
					filterLowLodVertexCount += 3;
				}
			}
			else
			{
				constexpr size_t HIGH_LOD_VERT_COUNT = 6;
				const std::array<int, HIGH_LOD_VERT_COUNT> highLODIndexes = {0, 1, 2, 3, 4, 6};
				for (int index : highLODIndexes)
				{
					GeomOctopoint(verts, index, vertexHighLODData);
					vertexHighLodVertexCount += 24;
				}

				constexpr size_t LOW_LOD_VERT_COUNT = 3;
				const std::array<int, LOW_LOD_VERT_COUNT> lowLODIndexes = {0, 2, 6};
				for (int index : lowLODIndexes)
				{
					GeomOctopoint(verts, index, vertexLowLODData);
					vertexLowLodVertexCount += 24;
				}

				for (int triIndex = 0; triIndex < 4; triIndex++)
				{
					int quadBlockIndex = 0;
					switch (triIndex)
					{
					case 0:
					case 1:
						quadBlockIndex = 0;
						break;
					case 2:
						quadBlockIndex = 1;
						break;
					case 3:
						quadBlockIndex = 2;
						break;
					}

					int firstPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, highLODData);
					GeomUVs(uvs, quadBlockIndex, firstPointIndex, highLODData, texIndex);

					int secondPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, highLODData);
					GeomUVs(uvs, quadBlockIndex, secondPointIndex, highLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, highLODData);
					GeomUVs(uvs, quadBlockIndex, thirdPointIndex, highLODData, texIndex);

					highLodVertexCount += 3;
				}

				for (int triIndex = 0; triIndex < 1; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, lowLODData);
					GeomUVs(uvs, 4, firstPointIndex, lowLODData, texIndex);

					int secondPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, lowLODData);
					GeomUVs(uvs, 4, secondPointIndex, lowLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, lowLODData);
					GeomUVs(uvs, 4, thirdPointIndex, lowLODData, texIndex);

					lowLodVertexCount += 3;
				}

				for (int triIndex = 0; triIndex < 4; triIndex++)
				{
					GeomPoint(filterVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][0], filterHighLODData);
					GeomPoint(filterVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][1], filterHighLODData);
					GeomPoint(filterVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][2], filterHighLODData);
					filterHighLodVertexCount += 3;
				}

				for (int triIndex = 0; triIndex < 1; triIndex++)
				{
					GeomPoint(filterVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][0], filterLowLODData);
					GeomPoint(filterVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][1], filterLowLODData);
					GeomPoint(filterVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][2], filterLowLODData);
					filterLowLodVertexCount += 3;
				}
			}
		}
	}

	m_highLODMesh.UpdateMesh(highLODData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals | Mesh::VBufDataType::STUV | Mesh::VBufDataType::TexIndex), Mesh::ShaderSettings::None);
	m_lowLODMesh.UpdateMesh(lowLODData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals | Mesh::VBufDataType::STUV | Mesh::VBufDataType::TexIndex), Mesh::ShaderSettings::None);

	m_vertexHighLODMesh.UpdateMesh(vertexHighLODData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	m_vertexLowLODMesh.UpdateMesh(vertexLowLODData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);

	const unsigned filterMeshFlags = (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals);
	const unsigned filterMeshShaderSettings = (Mesh::ShaderSettings::DrawWireframe | Mesh::ShaderSettings::DrawBackfaces | Mesh::ShaderSettings::ForceDrawOnTop |
		Mesh::ShaderSettings::DrawLinesAA | Mesh::ShaderSettings::DontOverrideShaderSettings | Mesh::ShaderSettings::ThickLines | Mesh::ShaderSettings::DiscardZeroColor);
	m_filterEdgeHighLODMesh.UpdateMesh(filterHighLODData, filterMeshFlags, filterMeshShaderSettings);
	m_filterEdgeLowLODMesh.UpdateMesh(filterLowLODData, filterMeshFlags, filterMeshShaderSettings);
}

void Level::UpdateAnimationRenderData()
{
	constexpr int NUM_VERTICES_QUAD = 4;
	constexpr int uvVertInd[NUM_FACES_QUADBLOCK + 1][NUM_VERTICES_QUAD] =
	{
		{0, 1, 3, 4},
		{1, 2, 4, 5},
		{3, 4, 6, 7},
		{4, 5, 7, 8},
		{0, 2, 6, 8},
	};

	auto GetUVForVertex = [&](const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& uvs, int quadInd, int vertInd) -> Vec2
		{
			const QuadUV& quv = uvs[quadInd];
			int vertIndInUvs = 0;
			for (int i = 0; i < NUM_VERTICES_QUAD; i++)
			{
				if (vertInd == uvVertInd[quadInd][i]) { vertIndInUvs = i; break; }
			}
			return quv[vertIndInUvs];
		};

	for (const AnimTexture& animTex : m_animTextures)
	{
		if (!animTex.IsPopulated()) { continue; }
		const std::vector<Texture>& textures = animTex.GetTextures();
		const AnimTextureFrame& frame = animTex.GetRenderFrame();
		int texIndex = GetTextureIndex(textures[frame.textureIndex].GetPath());
		for (size_t qbIndex : animTex.GetQuadblockIndexes())
		{
			Quadblock& qb = m_quadblocks[qbIndex];
			const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& uvs = frame.uvs;
			const size_t highLodBaseIndex = qb.GetRenderHighLodUVIndex();
			const size_t lowLodBaseIndex = qb.GetRenderLowLodUVIndex();
			if (highLodBaseIndex == RENDER_INDEX_NONE || lowLodBaseIndex == RENDER_INDEX_NONE) { continue; }

			if (qb.IsQuadblock())
			{
				for (int triIndex = 0; triIndex < 8; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][0];
					m_highLODMesh.UpdateUV(GetUVForVertex(uvs, triIndex / 2, firstPointIndex), texIndex, highLodBaseIndex + triIndex * 3);

					int secondPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][1];
					m_highLODMesh.UpdateUV(GetUVForVertex(uvs, triIndex / 2, secondPointIndex), texIndex, highLodBaseIndex + triIndex * 3 + 1);

					int thirdPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][2];
					m_highLODMesh.UpdateUV(GetUVForVertex(uvs, triIndex / 2, thirdPointIndex), texIndex, highLodBaseIndex + triIndex * 3 + 2);
				}

				for (int triIndex = 0; triIndex < 2; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][0];
					m_lowLODMesh.UpdateUV(GetUVForVertex(uvs, 4, firstPointIndex), texIndex, lowLodBaseIndex + triIndex * 3);

					int secondPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][1];
					m_lowLODMesh.UpdateUV(GetUVForVertex(uvs, 4, secondPointIndex), texIndex, lowLodBaseIndex + triIndex * 3 + 1);

					int thirdPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][2];
					m_lowLODMesh.UpdateUV(GetUVForVertex(uvs, 4, thirdPointIndex), texIndex, lowLodBaseIndex + triIndex * 3 + 2);
				}
			}
			else
			{
				for (int triIndex = 0; triIndex < 4; triIndex++)
				{
					int quadBlockIndex = 0;
					switch (triIndex)
					{
					case 0:
					case 1:
						quadBlockIndex = 0;
						break;
					case 2:
						quadBlockIndex = 1;
						break;
					case 3:
						quadBlockIndex = 2;
						break;
					}

					int firstPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][0];
					m_highLODMesh.UpdateUV(GetUVForVertex(uvs, quadBlockIndex, firstPointIndex), texIndex, highLodBaseIndex + triIndex * 3);

					int secondPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][1];
					m_highLODMesh.UpdateUV(GetUVForVertex(uvs, quadBlockIndex, secondPointIndex), texIndex, highLodBaseIndex + triIndex * 3 + 1);

					int thirdPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][2];
					m_highLODMesh.UpdateUV(GetUVForVertex(uvs, quadBlockIndex, thirdPointIndex), texIndex, highLodBaseIndex + triIndex * 3 + 2);
				}

				for (int triIndex = 0; triIndex < 1; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][0];
					m_lowLODMesh.UpdateUV(GetUVForVertex(uvs, 4, firstPointIndex), texIndex, lowLodBaseIndex + triIndex * 3);

					int secondPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][1];
					m_lowLODMesh.UpdateUV(GetUVForVertex(uvs, 4, secondPointIndex), texIndex, lowLodBaseIndex + triIndex * 3 + 1);

					int thirdPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][2];
					m_lowLODMesh.UpdateUV(GetUVForVertex(uvs, 4, thirdPointIndex), texIndex, lowLodBaseIndex + triIndex * 3 + 2);
				}
			}
		}
	}
}

void Level::UpdateFilterRenderData(const Quadblock& qb)
{
	const size_t highLodBaseIndex = qb.GetRenderFilterHighLodEdgeIndex();
	const size_t lowLodBaseIndex = qb.GetRenderFilterLowLodEdgeIndex();
	if (highLodBaseIndex == RENDER_INDEX_NONE || lowLodBaseIndex == RENDER_INDEX_NONE) { return; }

	const Vertex* verts = qb.GetUnswizzledVertices();
	const Color& filterColor = qb.GetFilter() ? qb.GetFilterColor() : Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u));
	Vertex filterVerts[NUM_VERTICES_QUADBLOCK];
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
	{
		const Vec3& pos = verts[i].m_pos;
		Vertex v = Vertex(Point(pos.x, pos.y, pos.z, filterColor.r, filterColor.g, filterColor.b));
		v.m_normal = verts[i].m_normal;
		filterVerts[i] = v;
	}

	if (qb.IsQuadblock())
	{
		for (int triIndex = 0; triIndex < 8; triIndex++)
		{
			m_filterEdgeHighLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]], highLodBaseIndex + triIndex * 3);
			m_filterEdgeHighLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]], highLodBaseIndex + triIndex * 3 + 1);
			m_filterEdgeHighLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]], highLodBaseIndex + triIndex * 3 + 2);
		}

		for (int triIndex = 0; triIndex < 2; triIndex++)
		{
			m_filterEdgeLowLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]], lowLodBaseIndex + triIndex * 3);
			m_filterEdgeLowLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]], lowLodBaseIndex + triIndex * 3 + 1);
			m_filterEdgeLowLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]], lowLodBaseIndex + triIndex * 3 + 2);
		}
	}
	else
	{
		for (int triIndex = 0; triIndex < 4; triIndex++)
		{
			m_filterEdgeHighLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]], highLodBaseIndex + triIndex * 3);
			m_filterEdgeHighLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]], highLodBaseIndex + triIndex * 3 + 1);
			m_filterEdgeHighLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]], highLodBaseIndex + triIndex * 3 + 2);
		}

		for (int triIndex = 0; triIndex < 1; triIndex++)
		{
			m_filterEdgeLowLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]], lowLodBaseIndex + triIndex * 3);
			m_filterEdgeLowLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]], lowLodBaseIndex + triIndex * 3 + 1);
			m_filterEdgeLowLODMesh.UpdatePoint(filterVerts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]], lowLodBaseIndex + triIndex * 3 + 2);
		}
	}
}

void Level::GenerateRenderBspData(const BSP& bsp)
{
	static Mesh bspMesh;
	std::vector<float> bspData;

	GuiRenderSettings::bspTreeMaxDepth = 0;
	GeomBoundingRect(&bsp, 0, bspData);

	bspMesh.UpdateMesh(bspData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals),
		(Mesh::ShaderSettings::DrawWireframe | Mesh::ShaderSettings::DontOverrideShaderSettings));
	m_bspModel.SetMesh(&bspMesh);
}

void Level::GenerateRenderCheckpointData(std::vector<Checkpoint>& checkpoints)
{
	if (checkpoints.empty()) { return; }

	static Mesh checkMesh;
	std::vector<float> checkData;
	std::unordered_set<int> selectedCheckpointIndexes; // Contain the ID of the checkpoints from selected quads
	for (size_t index : m_rendererSelectedQuadblockIndexes)
	{
		int checkpointIndex = m_quadblocks[index].GetCheckpoint();
		selectedCheckpointIndexes.insert(checkpointIndex);
	}

	for (Checkpoint& e : checkpoints)
	{
		bool selected = selectedCheckpointIndexes.contains(e.GetIndex());
		const Color& c = selected ? GuiRenderSettings::selectedCheckpointColor : e.GetColor();
		Vertex v = Vertex(Point(e.GetPos().x, e.GetPos().y, e.GetPos().z, c.r, c.g, c.b));
		GeomOctopoint(&v, 0, checkData);
	}

	checkMesh.UpdateMesh(checkData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	m_checkModel.SetMesh(&checkMesh);
}

void Level::GenerateRenderStartpointData(std::array<Spawn, NUM_DRIVERS>& spawns)
{
	static Mesh spawnsMesh;
	std::vector<float> spawnsData;

	for (Spawn& e : spawns)
	{
		Vertex v = Vertex(Point(e.pos.x, e.pos.y, e.pos.z, 0, 128, 255));
		GeomOctopoint(&v, 0, spawnsData);
	}

	spawnsMesh.UpdateMesh(spawnsData, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	m_spawnsModel.SetMesh(&spawnsMesh);
}

void Level::GenerateRenderMinimapBoundsData()
{
	static Mesh minimapBoundsMesh;
	std::vector<float> data;

	if (!m_minimapConfig.enabled) 
	{
		minimapBoundsMesh.Clear();
		m_minimapBoundsModel.SetMesh(&minimapBoundsMesh);
		return;
	}

	constexpr float sqrtThree = 1.44224957031f;

	// Convert from fixed-point to world coordinates
	float minX = static_cast<float>(m_minimapConfig.worldStartX) / static_cast<float>(FP_ONE_GEO);
	float maxX = static_cast<float>(m_minimapConfig.worldEndX) / static_cast<float>(FP_ONE_GEO);
	float minZ = static_cast<float>(m_minimapConfig.worldStartY) / static_cast<float>(FP_ONE_GEO);
	float maxZ = static_cast<float>(m_minimapConfig.worldEndY) / static_cast<float>(FP_ONE_GEO);

	// Add some height to the minimap bounds for better visibility
	float minY = -10.0f;
	float maxY = 10.0f;

	// Magenta color for minimap bounds
	Color c = Color(static_cast<unsigned char>(255), static_cast<unsigned char>(0), static_cast<unsigned char>(255));

	Vertex verts[] = {
		Vertex(Point(minX, minY, minZ, c.r, c.g, c.b)), //---
		Vertex(Point(minX, minY, maxZ, c.r, c.g, c.b)), //--+
		Vertex(Point(minX, maxY, minZ, c.r, c.g, c.b)), //-+-
		Vertex(Point(maxX, minY, minZ, c.r, c.g, c.b)), //+--
		Vertex(Point(maxX, maxY, minZ, c.r, c.g, c.b)), //++-
		Vertex(Point(minX, maxY, maxZ, c.r, c.g, c.b)), //-++
		Vertex(Point(maxX, minY, maxZ, c.r, c.g, c.b)), //+-+
		Vertex(Point(maxX, maxY, maxZ, c.r, c.g, c.b)), //+++
	};

	verts[0].m_normal = Vec3(-1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree);
	verts[1].m_normal = Vec3(-1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree);
	verts[2].m_normal = Vec3(-1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree);
	verts[3].m_normal = Vec3(1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree);
	verts[4].m_normal = Vec3(1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree);
	verts[5].m_normal = Vec3(-1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree);
	verts[6].m_normal = Vec3(1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree);
	verts[7].m_normal = Vec3(1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree);

	constexpr int NUM_EDGES = 12;
	constexpr int EDGE_VERTS = 2;
	const int edgeIndexes[NUM_EDGES][EDGE_VERTS] =
	{
		{0, 1}, {2, 5}, {3, 6}, {4, 7},
		{0, 2}, {1, 5}, {3, 4}, {6, 7},
		{0, 3}, {1, 6}, {2, 4}, {5, 7},
	};

	for (int i = 0; i < NUM_EDGES; i++)
	{
		const int a = edgeIndexes[i][0];
		const int b = edgeIndexes[i][1];
		GeomPoint(verts, a, data);
		GeomPoint(verts, b, data);
		GeomPoint(verts, b, data);
	}

	minimapBoundsMesh.UpdateMesh(data, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals),
		(Mesh::ShaderSettings::DrawWireframe | Mesh::ShaderSettings::DontOverrideShaderSettings));
	m_minimapBoundsModel.SetMesh(&minimapBoundsMesh);
}

void Level::GenerateRenderSelectedBlockData(const Quadblock& quadblock, const Vec3& queryPoint)
{
	m_rendererQueryPoint = queryPoint;

	static Mesh quadblockMesh;
	std::vector<float> data;
	auto AppendSelectedQuadblock = [&](const Quadblock& selected)
		{
			const Vertex* verts = selected.GetUnswizzledVertices();
			bool isQuadblock = selected.IsQuadblock();
			Vertex recoloredVerts[9];
			for (int i = 0; i < (isQuadblock ? 9 : 7); i++) //7 not 6 bc index correction
			{
				Color negated = verts[i].GetColor(true).Negated();
				recoloredVerts[i] = Point(0, 0, 0, negated.r, negated.g, negated.b); //pos reassigned in next line
				recoloredVerts[i].m_pos = verts[i].m_pos;
				recoloredVerts[i].m_normal = verts[i].m_normal;
				if (!isQuadblock && i == 4)
				{ //index correction for tris, since tris use 01234-6
					i++;
				}
			}

			if (isQuadblock)
			{
				//LLOD
				/*for (int triIndex = 0; triIndex < 2; triIndex++)
				{
					GeomPoint(recoloredVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][0], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][1], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][2], data);
				}*/
				//HLOD
				for (int triIndex = 0; triIndex < 8; triIndex++)
				{
					GeomPoint(recoloredVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][0], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][1], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][2], data);
				}
			}
			else
			{
				//LLOD
				/*for (int triIndex = 0; triIndex < 1; triIndex++)
				{
					GeomPoint(recoloredVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][0], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][1], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][2], data);
				}*/
				//HLOD
				for (int triIndex = 0; triIndex < 4; triIndex++)
				{
					GeomPoint(recoloredVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][0], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][1], data);
					GeomPoint(recoloredVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][2], data);
				}
			}
		};

	for (size_t index : m_rendererSelectedQuadblockIndexes)
	{
		if (index < m_quadblocks.size())
		{
			AppendSelectedQuadblock(m_quadblocks[index]);
		}
	}

	Vertex v = Vertex(Point(queryPoint.x, queryPoint.y, queryPoint.z, 255, 0, 0));
	GeomOctopoint(&v, 0, data);

	quadblockMesh.UpdateMesh(data, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals), (Mesh::ShaderSettings::DrawWireframe | Mesh::ShaderSettings::DrawBackfaces | Mesh::ShaderSettings::ForceDrawOnTop | Mesh::ShaderSettings::DrawLinesAA | Mesh::ShaderSettings::DontOverrideShaderSettings | Mesh::ShaderSettings::Blinky));
	m_selectedBlockModel.SetMesh(&quadblockMesh);

	if (GuiRenderSettings::showVisTree)
	{
		std::vector<const BSP*> bspLeaves = m_bsp.GetLeaves();
		size_t myBSPIndex = 0;
		for (size_t bsp_index = 0; bsp_index < bspLeaves.size(); bsp_index++)
		{
			const BSP& bsp = *bspLeaves[bsp_index];
			if (bsp.GetId() == quadblock.GetBSPID()) { myBSPIndex = bsp_index; }
		}

		std::vector<Quadblock*> quadsToSelect;

		for (size_t bsp_index = 0; bsp_index < bspLeaves.size(); bsp_index++)
		{
			const BSP& bsp = *bspLeaves[bsp_index];
			if (m_bspVis.Get(bsp_index, myBSPIndex))
			{
				const std::vector<size_t> qbIndeces = bsp.GetQuadblockIndexes();
				for (size_t qbInd : qbIndeces)
				{
					quadsToSelect.push_back(&m_quadblocks[qbInd]);
				}
			}
		}

		GenerateRenderMultipleQuadsData(quadsToSelect);
	}
}

void Level::GenerateRenderMultipleQuadsData(const std::vector<Quadblock*>& quads)
{
	if (quads.size() == 0)
	{
		m_multipleSelectedQuads.SetMesh(nullptr);
		return;
	}

	static Mesh quadblockMesh;
	std::vector<float> data;
	for (const Quadblock* quadblock_ptr : quads)
	{
		const Quadblock& quadblock = *quadblock_ptr;
		const Vertex* verts = quadblock.GetUnswizzledVertices();
		bool isQuadblock = quadblock.IsQuadblock();
		Vertex recoloredVerts[9];
		for (int i = 0; i < (isQuadblock ? 9 : 7); i++) //7 not 6 bc index correction
		{
			Color negated = verts[i].GetColor(true).Negated();
			recoloredVerts[i] = Point(0, 0, 0, negated.r, negated.g, negated.b); //pos reassigned in next line
			recoloredVerts[i].m_pos = verts[i].m_pos;
			recoloredVerts[i].m_normal = verts[i].m_normal;
			if (!isQuadblock && i == 4)
			{ //index correction for tris, since tris use 01234-6
				i++;
			}
		}

		if (quadblock.IsQuadblock())
		{
			//LLOD
			/*for (int triIndex = 0; triIndex < 2; triIndex++)
			{
				GeomPoint(recoloredVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][0], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][1], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::quadLLODVertArrangements[triIndex][2], data);
			}*/
			//HLOD
			for (int triIndex = 0; triIndex < 8; triIndex++)
			{
				GeomPoint(recoloredVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][0], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][1], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::quadHLODVertArrangements[triIndex][2], data);
			}
		}
		else
		{
			//LLOD
			/*for (int triIndex = 0; triIndex < 1; triIndex++)
			{
				GeomPoint(recoloredVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][0], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][1], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::triLLODVertArrangements[triIndex][2], data);
			}*/
			//HLOD
			for (int triIndex = 0; triIndex < 4; triIndex++)
			{
				GeomPoint(recoloredVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][0], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][1], data);
				GeomPoint(recoloredVerts, FaceIndexConstants::triHLODVertArrangements[triIndex][2], data);
			}
		}
	}

	quadblockMesh.UpdateMesh(data, (Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals), (Mesh::ShaderSettings::DrawWireframe | Mesh::ShaderSettings::DrawBackfaces | Mesh::ShaderSettings::ForceDrawOnTop | Mesh::ShaderSettings::DrawLinesAA | Mesh::ShaderSettings::DontOverrideShaderSettings | Mesh::ShaderSettings::Blinky));
	m_multipleSelectedQuads.SetMesh(&quadblockMesh);
}

void Level::ViewportClickHandleBlockSelection(int pixelX, int pixelY, bool appendSelection, const Renderer& rend)
{
	std::function<std::optional<std::tuple<const Quadblock*, const glm::vec3>>(int, int, std::vector<Quadblock>&, unsigned)> check = [&rend](int pixelCoordX, int pixelCoordY, const std::vector<Quadblock>& qbs, unsigned index)
		{
			std::vector<std::tuple<const Quadblock*, glm::vec3, float>> passed;

			//we're currently checking ALL quadblocks on a click (bad), so we should
			//use an acceleration structure (good thing we can have a BSP :) )
			//although it may be better to use a different acceleration structure.
			//I don't care for right now, clicking causes a little lag spike. TODO.
			//another option to speed this up is to do collision testing for the low LOD instead.
			for (const Quadblock& qb : qbs)
			{
				bool collided = false;
				const Vertex* verts = qb.GetUnswizzledVertices();
				glm::vec3 tri[3];
				bool isQuadblock = qb.IsQuadblock();

				std::tuple<glm::vec3, float> queryResult;
				glm::vec3 worldSpaceRay = rend.ScreenspaceToWorldRay(pixelCoordX, pixelCoordY);
				//high LOD
				for (int triIndex = 0; triIndex < (isQuadblock ? 8 : 4); triIndex++)
				{
					if (isQuadblock)
					{
						tri[0] = glm::vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.z);
						tri[1] = glm::vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.z);
						tri[2] = glm::vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.z);
					}
					else
					{
						tri[0] = glm::vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.z);
						tri[1] = glm::vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.z);
						tri[2] = glm::vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.z);
					}

					queryResult = rend.WorldspaceRayTriIntersection(worldSpaceRay, tri); //if we have multiple collisions in one block, just pick one idc
					collided |= (std::get<1>(queryResult) != -1.0f);

					if (collided) { break; }
				}

				//low LOD THIS ONE MIGHT HAVE A BUG
				//for (int triIndex = 0; triIndex < (isQuadblock ? 2 : 1); triIndex++)
				//{
				//  if (isQuadblock)
				//  {
				//    tri[0] = glm::vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.z);
				//    tri[1] = glm::vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.z);
				//    tri[2] = glm::vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.z);
				//  }
				//  else
				//  {
				//    tri[0] = glm::vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.z);
				//    tri[1] = glm::vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.z);
				//    tri[2] = glm::vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.z);
				//  }

				//  queryResult = rend.WorldspaceRayTriIntersection(worldSpaceRay, tri); //if we have multiple collisions in one block, just pick one idc
				//  collided |= (std::get<1>(queryResult) != -1.0f);

				//  if (collided) { break; }
				//}


				if (collided) { passed.push_back(std::tuple<const Quadblock*, glm::vec3, float>(&qb, std::get<0>(queryResult), std::get<1>(queryResult))); continue; }
			}

			//sort collided blocks by time value (distance from camera).
			std::sort(passed.begin(), passed.end(),
				[](const std::tuple<const Quadblock*, glm::vec3, float>& a, const std::tuple<const Quadblock*, glm::vec3, float>& b) {
					return std::get<2>(a) < std::get<2>(b);
				});

			std::optional<std::tuple<const Quadblock*, glm::vec3>> result;
			//at the very end
			if (passed.size() > 0)
			{
				const auto& tuple = passed[index % passed.size()];
				const Quadblock* qb = std::get<0>(tuple);
				result = std::make_optional(std::tuple<const Quadblock*, glm::vec3>(qb, std::get<1>(tuple)));
			}
			else
			{
				result.reset();
			}
			return result;
		};

	static int lastClickedX = pixelX;
	static int lastClickedY = pixelY;
	static int indenticalClickTimes = -1;

	if (!appendSelection && pixelX == lastClickedX && pixelY == lastClickedY)
	{
		indenticalClickTimes++;
	}
	else
	{
		lastClickedX = pixelX;
		lastClickedY = pixelY;
		indenticalClickTimes = 0;
	}

	std::optional<std::tuple<const Quadblock*, const glm::vec3>> collidedQB = check(pixelX, pixelY, m_quadblocks, indenticalClickTimes);

	if (collidedQB.has_value())
	{
		const Quadblock* clickedQuadblock = std::get<0>(collidedQB.value());
		glm::vec3 p = std::get<1>(collidedQB.value());
		Vec3 point = Vec3(p.x, p.y, p.z);
		size_t clickedIndex = REND_NO_SELECTED_QUADBLOCK;
		for (size_t i = 0; i < m_quadblocks.size(); i++)
		{
			if (&m_quadblocks[i] == clickedQuadblock)
			{
				clickedIndex = i;
				break;
			}
		}

		if (clickedIndex != REND_NO_SELECTED_QUADBLOCK)
		{
			if (!appendSelection) { m_rendererSelectedQuadblockIndexes.clear(); }

			auto selectedIt = std::find(m_rendererSelectedQuadblockIndexes.begin(), m_rendererSelectedQuadblockIndexes.end(), clickedIndex);
			bool alreadySelected = selectedIt != m_rendererSelectedQuadblockIndexes.end();
			if (appendSelection && alreadySelected)
			{
				m_rendererSelectedQuadblockIndexes.erase(selectedIt);
			}
			else if (!alreadySelected)
			{
				m_rendererSelectedQuadblockIndexes.push_back(clickedIndex);
			}
		}

		GenerateRenderSelectedBlockData(*clickedQuadblock, point);
	}
	else
	{
		m_selectedBlockModel.SetMesh();
	}
	GenerateRenderCheckpointData(m_checkpoints);
}
