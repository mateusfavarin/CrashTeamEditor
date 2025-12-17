#include "level.h"
#include "psx_types.h"
#include "io.h"
#include "utils.h"
#include "geo.h"
#include "process.h"
#include "gui_render_settings.h"
#include "renderer.h"
#include "vistree.h"
#include "text3d.h"
#include "simple_level_instances.h"

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

void Level::OpenModelExtractorWindow()
{
	m_showModelExtractorWindow = true;
}

void Level::Clear(bool clearErrors)
{
	m_loaded = false;
	m_showHotReloadWindow = false;
	m_showModelExtractorWindow = false;
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
	m_simpleVisTree = false;
	m_bspVis.Clear();
	m_maxQuadPerLeaf = 31;
	m_maxLeafAxisLength = 64.0f;
	m_distanceNearClip = -1.0f;
	m_distanceFarClip = 1000.0f;
	m_pythonConsole.clear();
	m_saveScript = false;
	m_vrm.clear();
	m_lastAnimTextureCount = 0;
	DeleteMaterials(this);

	for (Model* model : m_models)
	{
		if (model) { model->Clear(model != m_models[LevelModels::LEVEL]); }
	}
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

Model* Level::GetLevelModel()
{
	return m_models[LevelModels::LEVEL];
}

Model* Level::GetBspModel()
{
	return m_models[LevelModels::BSP];
}

Model* Level::GetSpawnModel()
{
	return m_models[LevelModels::SPAWN];
}

Model* Level::GetCheckpointModel()
{
	return m_models[LevelModels::CHECKPOINT];
}

Model* Level::GetSelectedModel()
{
	return m_models[LevelModels::SELECTED];
}

Model* Level::GetMultiSelectedModel()
{
	return m_models[LevelModels::MULTI_SELECTED];
}

Model* Level::GetFilterModel()
{
	return m_models[LevelModels::FILTER];
}

bool Level::GenerateBSP()
{
	std::vector<size_t> quadIndexes;
	for (size_t i = 0; i < m_quadblocks.size(); i++) { quadIndexes.push_back(i); }
	m_bsp.Clear();
	m_bsp.SetQuadblockIndexes(quadIndexes);
	m_bsp.Generate(m_quadblocks, m_maxQuadPerLeaf, m_maxLeafAxisLength);
	if (m_bsp.IsValid())
	{
		GenerateRenderBspData();
		if (m_genVisTree) { m_bspVis = GenerateVisTree(m_quadblocks, &m_bsp, m_simpleVisTree, m_distanceNearClip, m_distanceFarClip); }
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

	UpdateRenderCheckpointData();
	return true;
}


enum class PresetHeader : unsigned
{
	SPAWN, LEVEL, PATH, MATERIAL, TURBO_PAD, ANIM_TEXTURES, SCRIPT
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
					if (json.contains(material + "_visTreeTransparent"))
					{
						m_propVisTreeTransparent.SetPreview(material, json[material + "_visTreeTransparent"]);
						m_propVisTreeTransparent.Apply(material, m_materialToQuadblocks[material], m_quadblocks);
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
						GenerateRenderBspData();
					}
				}
			}
		}
	}
	else if (header == PresetHeader::SCRIPT)
	{
		m_pythonScript = json["script"];
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
			materialJson[key + "_visTreeTransparent"] = m_propVisTreeTransparent.GetBackup(key);
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
	m_models[LevelModels::SELECTED]->GetMesh().Clear();
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
		turboPad.SetVisTreeTransparent(false);
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


	m_bsp.Clear();
	file.seekg(offLev + std::streampos(meshInfo.offBSPNodes));
	std::vector<BSP*> bspArray;
	for (uint32_t i = 0; i < meshInfo.numBSPNodes; i++)
	{
		bspArray.push_back(new BSP());
	}

	for (uint32_t i = 0; i < meshInfo.numBSPNodes; i++)
	{
		uint16_t flag;
		std::streampos nodeStart = file.tellg();
		Read(file, flag);
		file.seekg(nodeStart);

		if (flag & BSPFlags::LEAF)
		{
			PSX::BSPLeaf leaf = {};
			Read(file, leaf);
			bspArray[leaf.id]->PopulateLeaf(leaf, bspArray, m_quadblocks, meshInfo.offQuadblocks, meshInfo.numBSPNodes);
		}
		else
		{
			PSX::BSPBranch branch = {};
			Read(file, branch);
			bspArray[branch.id]->PopulateBranch(branch, bspArray, meshInfo.numBSPNodes);
		}
	}

	if (!bspArray.empty())
	{
		m_bsp = *(bspArray[0]);
		m_bsp.PopulateBranchQuadIndexes();
		if (m_bsp.IsValid()) { GenerateRenderBspData(); }
		else { m_bsp.Clear(); }
	}
	else { m_bsp.Clear(); }

	file.seekg(offLev + std::streampos(header.offCheckpointNodes));
	for (uint32_t i = 0; i < header.numCheckpointNodes; i++)
	{
		PSX::Checkpoint checkpoint = {};
		Read(file, checkpoint);
		m_checkpoints.emplace_back(checkpoint, static_cast<int>(i));
	}
	UpdateRenderCheckpointData();

	m_tropyGhost.clear();
	m_oxideGhost.clear();
	if (header.offExtra > 0)
	{
		file.seekg(offLev + std::streampos(header.offExtra));
		PSX::LevelExtraHeader extraHeader = {};
		Read(file, extraHeader);
		// Read N. Tropy Ghost
		if (extraHeader.count >= PSX::LevelExtra::N_TROPY_GHOST + 1 &&
			extraHeader.offsets[PSX::LevelExtra::N_TROPY_GHOST] > 0)
		{
			file.seekg(offLev + std::streampos(extraHeader.offsets[PSX::LevelExtra::N_TROPY_GHOST]));
			size_t ghostSize = 0;
			if (extraHeader.count > PSX::LevelExtra::N_OXIDE_GHOST && extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST] > 0)
			{
				ghostSize = extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST] - extraHeader.offsets[PSX::LevelExtra::N_TROPY_GHOST];
			}
			else
			{
				ghostSize = header.offLevNavTable - extraHeader.offsets[PSX::LevelExtra::N_TROPY_GHOST];
			}
			m_tropyGhost.resize(ghostSize);
			file.read(reinterpret_cast<char*>(m_tropyGhost.data()), ghostSize);
		}
		// Read N. Oxide Ghost
		if (extraHeader.count >= PSX::LevelExtra::N_OXIDE_GHOST + 1 && extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST] > 0)
		{
			file.seekg(offLev + std::streampos(extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST]));
			size_t ghostSize = header.offLevNavTable - extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST];
			m_oxideGhost.resize(ghostSize);
			file.read(reinterpret_cast<char*>(m_oxideGhost.data()), ghostSize);
		}
	}

	m_loaded = true;
	file.close();
	GenerateRenderLevData();
	return true;
}

bool Level::SaveLEV(const std::filesystem::path& path)
{
	#define nameof(x) #x
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
	printf(nameof(offHeader) " = %zx\n", offHeader);
	size_t currOffset = sizeof(header);

	PSX::MeshInfo meshInfo = {};
	const size_t offMeshInfo = currOffset;
	printf(nameof(offMeshInfo) " = %zx\n", offMeshInfo);
	currOffset += sizeof(meshInfo);

	const size_t offTexture = currOffset;
	printf(nameof(offTexture) " = %zx\n", offTexture);
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
			printf(nameof(offAnimData) " = %zx\n", offAnimData);

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
			printf(nameof(offAnimData) " = %zx\n", offAnimData);
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
		printf(nameof(offAnimData) " = %zx\n", offAnimData);
		for (size_t i = 0; i < sizeof(uint32_t); i++) { animData.push_back(0); }
		memcpy(&animData[0], &offAnimData, sizeof(uint32_t));
		animPtrMapOffsets.push_back(0);
	}

	currOffset += (sizeof(PSX::TextureGroup) * texGroups.size()) + animData.size();

	const size_t offQuadblocks = currOffset;
	printf(nameof(offQuadblocks) " = %zx\n", offQuadblocks);
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
	visibleInstancesDummy.push_back(0xFFFFFFFF);
	visibleInstances.push_back({visibleInstancesDummy, currOffset});
	currOffset += visibleInstancesDummy.size() * sizeof(uint32_t);

	std::unordered_map<PSX::VisibleSet, size_t> visibleSetMap;
	std::vector<PSX::VisibleSet> visibleSets;
	const size_t offVisibleSet = currOffset;
	printf(nameof(offVisibleSet) " = %zx\n", offVisibleSet);

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
	printf(nameof(offVertices) " = %zx\n", offVertices);
	std::vector<std::vector<uint8_t>> serializedVertices;
	for (const Vertex& vertex : orderedVertices)
	{
		serializedVertices.push_back(vertex.Serialize());
		currOffset += serializedVertices.back().size();
	}

	const size_t offBSP = currOffset;
	printf(nameof(offBSP) " = %zx\n", offBSP);
	currOffset += bspSize;

	meshInfo.numQuadblocks = static_cast<uint32_t>(serializedQuads.size());
	meshInfo.numVertices = static_cast<uint32_t>(serializedVertices.size());
	meshInfo.offQuadblocks = static_cast<uint32_t>(offQuadblocks);
	meshInfo.offVertices = static_cast<uint32_t>(offVertices);
	meshInfo.unk1 = 0;
	meshInfo.unk2 = 0;
	meshInfo.offBSPNodes = static_cast<uint32_t>(offBSP);
	meshInfo.numBSPNodes = static_cast<uint32_t>(serializedBSPs.size());

	const size_t offCheckpoints = currOffset;
	printf(nameof(offCheckpoints) " = %zx\n", offCheckpoints);
	std::vector<std::vector<uint8_t>> serializedCheckpoints;
	for (const Checkpoint& checkpoint : m_checkpoints)
	{
		serializedCheckpoints.push_back(checkpoint.Serialize());
		currOffset += serializedCheckpoints.back().size();
	}

	const size_t offTropyGhost = m_tropyGhost.empty() ? 0 : currOffset;
	printf(nameof(offTropyGhost) " = %zx\n", offTropyGhost);
	currOffset += m_tropyGhost.size();

	const size_t offOxideGhost = m_oxideGhost.empty() ? 0 : currOffset;
	printf(nameof(offOxideGhost) " = %zx\n", offOxideGhost);
	currOffset += m_oxideGhost.size();

	PSX::LevelExtraHeader extraHeader = {};
	if (offTropyGhost > 0)
	{
		if (offOxideGhost > 0) { extraHeader.count = PSX::LevelExtra::COUNT; }
		else { extraHeader.count = PSX::LevelExtra::N_OXIDE_GHOST; }
	}
	else { extraHeader.count = 0; }
	extraHeader.offsets[PSX::LevelExtra::MINIMAP] = 0;
	extraHeader.offsets[PSX::LevelExtra::SPAWN] = 0;
	extraHeader.offsets[PSX::LevelExtra::CAMERA_END_OF_RACE] = 0;
	extraHeader.offsets[PSX::LevelExtra::CAMERA_DEMO] = 0;
	extraHeader.offsets[PSX::LevelExtra::N_TROPY_GHOST] = static_cast<uint32_t>(offTropyGhost);
	extraHeader.offsets[PSX::LevelExtra::N_OXIDE_GHOST] = static_cast<uint32_t>(offOxideGhost);
	extraHeader.offsets[PSX::LevelExtra::CREDITS] = 0;

	const size_t offExtraHeader = currOffset;
	printf(nameof(offExtraHeader) " = %zx\n", offExtraHeader);
	currOffset += sizeof(extraHeader);

	constexpr size_t BOT_PATH_COUNT = 3;
	std::vector<PSX::NavHeader> navHeaders(BOT_PATH_COUNT);

	const size_t offNavHeaders = currOffset;
	printf(nameof(offNavHeaders) " = %zx\n", offNavHeaders);
	currOffset += navHeaders.size() * sizeof(PSX::NavHeader);

	std::vector<uint32_t> visMemNodesP1(visNodeSize);
	const size_t offVisMemNodesP1 = currOffset;
	printf(nameof(offVisMemNodesP1) " = %zx\n", offVisMemNodesP1);
	currOffset += visMemNodesP1.size() * sizeof(uint32_t);

	std::vector<uint32_t> visMemQuadsP1(visQuadSize);
	const size_t offVisMemQuadsP1 = currOffset;
	printf(nameof(offVisMemQuadsP1) " = %zx\n", offVisMemQuadsP1);
	currOffset += visMemQuadsP1.size() * sizeof(uint32_t);

	std::vector<uint32_t> visMemBSPP1(bspNodes.size() * 2);
	const size_t offVisMemBSPP1 = currOffset;
	printf(nameof(offVisMemBSPP1) " = %zx\n", offVisMemBSPP1);
	currOffset += visMemBSPP1.size() * sizeof(uint32_t);

	PSX::VisualMem visMem = {};
	visMem.offNodes[0] = static_cast<uint32_t>(offVisMemNodesP1);
	visMem.offQuads[0] = static_cast<uint32_t>(offVisMemQuadsP1);
	visMem.offBSP[0] = static_cast<uint32_t>(offVisMemBSPP1);
	const size_t offVisMem = currOffset;
  printf(nameof(offVisMem) " = %zx\n", offVisMem);
	currOffset += sizeof(visMem);

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


	PSX::InstDef cactus = {};
	const size_t offCactus = currOffset;
	printf(nameof(offCactus) " = %zx\n", offCactus);
  currOffset += sizeof(cactus);

	const size_t offInstDefList_ptrArray = currOffset;
	printf(nameof(offInstDefList_ptrArray) " = %zx\n", offInstDefList_ptrArray);
	uint32_t instDefList_ptrArray[2] =
	{
		offCactus,
		0x0, //NULL
	};
	currOffset += sizeof(instDefList_ptrArray);

	const size_t offInstDefList2_ptrArray = currOffset;
	printf(nameof(offInstDefList2_ptrArray) " = %zx\n", offInstDefList2_ptrArray);
	uint32_t instDefList2_ptrArray[2] =
	{
		offCactus,
		0x0, //NULL
	};
	currOffset += sizeof(instDefList2_ptrArray);

	for (auto& set : visibleSets)
	{
		size_t index = visibleSetMap[set];
		visibleSetMap.erase(set);
		set.offVisibleInstances = offInstDefList2_ptrArray;
		visibleSetMap[set] = index;
	}

	header.numInstances = 1;
	header.offInstances = (uint32_t)offCactus;
	header.numModels = 1;
	//header.offModels taken care of a few lines below
	header.offModelInstances = (uint32_t)offInstDefList_ptrArray;

  PSX::Model cactusModel = {};
  const size_t offCactusModel = currOffset;
	printf(nameof(offCactusModel) " = %zx\n", offCactusModel);
  currOffset += sizeof(cactusModel);

	const size_t offModelList_ptrArray = currOffset;
	printf(nameof(offModelList_ptrArray) " = %zx\n", offModelList_ptrArray);
	uint32_t modelList_ptrArray[2] =
	{
		offCactusModel,
		0x0, //NULL
	};
	currOffset += sizeof(modelList_ptrArray);
  header.offModels = (uint32_t)offModelList_ptrArray;

  PSX::ModelHeader cactusModelHeader = {};
  const size_t offCactusModelHeader = currOffset;
	printf(nameof(offCactusModelHeader) " = %zx\n", offCactusModelHeader);
  currOffset += sizeof(cactusModelHeader);

	char commandList[] =
	{
		0x1C, 0x00, 0x00, 0x00, 0x01, 0x10, 0x0E, 0xB8, 0x01, 0x0E, 0x0F, 0x38, 0x01, 0x0C, 0x57, 0x38, 0x02, 0x04, 0x10, 0x18, 0x01, 0x00, 0x57, 0x38, 0x02, 0x2C, 0x11, 0x18, 0x01, 0x00, 0x57, 0x38, 0x02, 0x2C, 0x12, 0x18, 0x01, 0x2A, 0x57, 0x38, 0x02, 0x04, 0x13, 0x18, 0x01, 0x28, 0x57, 0x38, 0x02, 0x2C, 0x14, 0x18, 0x01, 0x10, 0x0E, 0x3C, 0x02, 0x0E, 0x0F, 0x1C, 0x03, 0x26, 0x0E, 0x58, 0x04, 0x24, 0x0F, 0x38, 0x01, 0x22, 0x15, 0x78, 0x05, 0x04, 0x10, 0x7C, 0x06, 0x20, 0x10, 0x18, 0x01, 0x2C, 0x11, 0x3C, 0x02, 0x00, 0x11, 0x18, 0x07, 0x1E, 0x16, 0x38, 0x08, 0x1C, 0x17, 0x78, 0x09, 0x2C, 0x12, 0x7C, 0x0A, 0x26, 0x12, 0x18, 0x09, 0x00, 0x18, 0x38, 0x01, 0x04, 0x13, 0x7C, 0x02, 0x04, 0x13, 0x18, 0x05, 0x2C, 0x14, 0x3C, 0x06, 0x1A, 0x14, 0x18, 0x0B, 0x18, 0x19, 0x38, 0x04, 0x26, 0x0E, 0x7C, 0x03, 0x16, 0x0E, 0x18, 0x04, 0x26, 0x1A, 0x38, 0x01, 0x24, 0x0F, 0x7C, 0x02, 0x14, 0x0F, 0x18, 0x01, 0x1E, 0x1B, 0x38, 0x0C, 0x22, 0x15, 0x7C, 0x0D, 0x1A, 0x14, 0x1C, 0x02, 0x18, 0x19, 0x5C, 0x03, 0x12, 0x19, 0x58, 0x04, 0x16, 0x0E, 0x3C, 0x09, 0x12, 0x57, 0x18, 0x0E, 0x26, 0x1A, 0x3C, 0x07, 0x14, 0x0F, 0x1C, 0x07, 0x12, 0x19, 0x5C, 0x0C, 0x1E, 0x1B, 0x3C, 0x01, 0x1A, 0x14, 0xBC, 0x01, 0x36, 0x0E, 0x38, 0x01, 0x22, 0x15, 0x3C, 0x02, 0x2E, 0x0F, 0x18, 0x01, 0x20, 0x10, 0x3C, 0x02, 0x20, 0x10, 0x18, 0x01, 0x00, 0x11, 0x3C, 0x02, 0x30, 0x11, 0x18, 0x01, 0x00, 0x18, 0x3C, 0x07, 0x0A, 0x15, 0x78, 0x08, 0x1E, 0x16, 0x7C, 0x0E, 0x16, 0x16, 0x18, 0x08, 0x08, 0x19, 0x38, 0x09, 0x1C, 0x17, 0x7C, 0x0A, 0x06, 0x17, 0x18, 0x09, 0x26, 0x12, 0x3C, 0x0A, 0x26, 0x12, 0x18, 0x09, 0x0A, 0x15, 0x3C, 0x0F, 0x00, 0x18, 0x7C, 0x10, 0x0A, 0x15, 0x9C, 0x10, 0x16, 0x16, 0x1C, 0x10, 0x26, 0x12, 0x1C, 0x11, 0x08, 0x57, 0x38, 0x12, 0x06, 0x17, 0x1C, 0x06, 0x08, 0x19, 0x3C, 0x06, 0x16, 0x16, 0x7C, 0x02, 0x30, 0x11, 0x9C, 0x02, 0x00, 0x18, 0x1C, 0x02, 0x32, 0x12, 0x18, 0x01, 0x04, 0x13, 0x3C, 0x02, 0x02, 0x57, 0x18, 0x01, 0x1A, 0x14, 0x3C, 0x02, 0x36, 0x0E, 0x1C, 0x09, 0x34, 0x57, 0x58, 0x12, 0x32, 0x12, 0x5C, 0x06, 0x30, 0x11, 0x3C, 0x06, 0x20, 0x10, 0x7C, 0x06, 0x2E, 0x0F, 0x7C, 0x06, 0x36, 0x0E, 0x7C, 0xFF, 0xFF, 0xFF, 0xFF
	};
  const size_t offCommandList = currOffset;
	printf(nameof(offCommandList) " = %zx\n", offCommandList);
  currOffset += sizeof(commandList);

  PSX::ModelFrame cactusModelFrame = {};
  const size_t offCactusModelFrame = currOffset;
	printf(nameof(offCactusModelFrame) " = %zx\n", offCactusModelFrame);
  currOffset += sizeof(cactusModelFrame);

	char cactusVertexData[] =
	{ //the beginning of this data is definitely cactus vertex data, but we don't know how long it is, so we copied an amount that was large enough that we were sure it was at least "all of it"
		0xAE, 0xB3, 0x00, 0xA3, 0xB3, 0x70, 0x8F, 0xE4, 0x00, 0x83, 0xE9, 0x4E, 0x70, 0xB3, 0x00, 0x58, 0xA9, 0x4D, 0x70, 0x50, 0x00, 0x58, 0x5C, 0x4D, 0x8F, 0x1F, 0x00, 0x83, 0x13, 0x4E, 0xAE, 0x50, 0x00, 0xA4, 0x55, 0x68, 0xCA, 0x59, 0x6E, 0xC9, 0xB1, 0x70, 0xA2, 0xA7, 0x8E, 0x7D, 0xFE, 0x85, 0x52, 0xB1, 0x62, 0x3A, 0xA3, 0x66, 0x38, 0x9F, 0x53, 0x38, 0x67, 0x50, 0x53, 0x53, 0x68, 0x7A, 0x00, 0x99, 0xA3, 0x61, 0x97, 0xC4, 0x51, 0x8E, 0xDD, 0x47, 0xBA, 0xF6, 0x57, 0x8B, 0xF3, 0xB6, 0x92, 0xC4, 0xB4, 0x8C, 0xE0, 0xBB, 0xB4, 0xFE, 0x83, 0xAA, 0x9D, 0x51, 0xF1, 0x9D, 0xB3, 0xF1, 0x7D, 0xE4, 0xF2, 0x5E, 0xB3, 0xF2, 0x3B, 0x61, 0x6A, 0x20, 0x44, 0x98, 0x1B, 0xBF, 0x91, 0x05, 0xB7, 0x75, 0x00, 0x4F, 0x6E, 0x00, 0x6F, 0x8C, 0x5E, 0x51, 0xF2, 0x7D, 0x20, 0xF2, 0x7E, 0x82, 0xFF, 0x00, 0x00, 0x00, 0xF4, 0x47, 0x06, 0x00, 0x00, 0x48, 0x06, 0x00, 0x0C, 0x48, 0x06, 0x00, 0x18, 0x48, 0x06, 0x00, 0x24, 0x48, 0x06, 0x00, 0x30, 0x48, 0x06, 0x00, 0x3C, 0x48, 0x06, 0x00, 0x48, 0x48, 0x06, 0x00, 0x54, 0x48, 0x06, 0x00, 0x60, 0x48, 0x06, 0x00, 0x6C, 0x48, 0x06, 0x00, 0x78, 0x48, 0x06, 0x00, 0x84, 0x48, 0x06, 0x00, 0x90, 0x48, 0x06, 0x00, 0x9C, 0x48, 0x06, 0x00, 0xA8, 0x48, 0x06, 0x00, 0xB4, 0x48, 0x06, 0x00, 0xC0, 0x48, 0x06, 0x00, 0x5F, 0xE0, 0x38, 0x73, 0x5F, 0xFF, 0x7A, 0x00, 0x40, 0xE0, 0x40, 0xE0, 0x5F, 0xFF, 0x38, 0x73, 0x40, 0xE0, 0x7A, 0x00, 0x40, 0xFF, 0x40, 0xFF, 0x5F, 0xE0, 0x38, 0x73, 0x40, 0xE0, 0x7A, 0x00, 0x5F, 0xFF, 0x5F, 0xFF, 0x40, 0xE0, 0x38, 0x73, 0x5F, 0xFF, 0x7A, 0x00, 0x40, 0xFF, 0x40, 0xFF, 0x5F, 0xFF, 0x38, 0x73, 0x5F, 0xE0, 0x7A, 0x00, 0x40, 0xFF, 0x40, 0xFF, 0x5F, 0xE0, 0x38, 0x73, 0x40, 0xFF, 0x7A, 0x00, 0x40, 0xE0, 0x40, 0xE0, 0x5F, 0xE0, 0x38, 0x73, 0x40, 0xE0, 0x7A, 0x00, 0x40, 0xFF, 0x40, 0xFF, 0x5F, 0xE0, 0x38, 0x73, 0x40, 0xFF, 0x7A, 0x00, 0x5F, 0xFF, 0x5F, 0xFF, 0x40, 0xE0, 0x38, 0x73, 0x40, 0xFF, 0x7A, 0x00, 0x5F, 0xE0, 0x5F, 0xE0, 0x40, 0xFF, 0x38, 0x73, 0x5F, 0xE0, 0x7A, 0x00, 0x5F, 0xFF, 0x5F, 0xFF, 0x40, 0xE0, 0x38, 0x73, 0x5F, 0xE0, 0x7A, 0x00, 0x5F, 0xFF, 0x5F, 0xFF, 0x5F, 0xFF, 0x38, 0x73, 0x40, 0xFF, 0x7A, 0x00, 0x40, 0xE0, 0x40, 0xE0, 0x5F, 0xFF, 0x38, 0x73, 0x5F, 0xE0, 0x7A, 0x00, 0x40, 0xE0, 0x40, 0xE0, 0x40, 0xE0, 0x38, 0x73, 0x5F, 0xE0, 0x7A, 0x00, 0x40, 0xFF, 0x40, 0xFF, 0x40, 0xFF, 0x38, 0x73, 0x5F, 0xFF, 0x7A, 0x00, 0x5F, 0xE0, 0x5F, 0xE0, 0x5F, 0xE0, 0x38, 0x73, 0x5F, 0xFF, 0x7A, 0x00, 0x40, 0xFF, 0x40, 0xFF, 0x40, 0xFF, 0x38, 0x73, 0x40, 0xE0, 0x7A, 0x00, 0x5F, 0xE0, 0x5F, 0xE0, 0x40, 0xFF, 0x38, 0x73, 0x5F, 0xE0, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	const size_t offCactusVertexData = currOffset; //not used
	printf(nameof(offCactusVertexData) " = %zx\n", offCactusVertexData);
  currOffset += sizeof(cactusVertexData);

	PSX::TextureLayout cactusTextureLayouts[18] = {};
	memcpy(&cactusTextureLayouts[0],  "\x5F\xE0\x38\x73\x5F\xFF\x7A\x00\x40\xE0\x40\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[1],  "\x5F\xFF\x38\x73\x40\xE0\x7A\x00\x40\xFF\x40\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[2],  "\x5F\xE0\x38\x73\x40\xE0\x7A\x00\x5F\xFF\x5F\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[3],  "\x40\xE0\x38\x73\x5F\xFF\x7A\x00\x40\xFF\x40\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[4],  "\x5F\xFF\x38\x73\x5F\xE0\x7A\x00\x40\xFF\x40\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[5],  "\x5F\xE0\x38\x73\x40\xFF\x7A\x00\x40\xE0\x40\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[6],  "\x5F\xE0\x38\x73\x40\xE0\x7A\x00\x40\xFF\x40\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[7],  "\x5F\xE0\x38\x73\x40\xFF\x7A\x00\x5F\xFF\x5F\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[8],  "\x40\xE0\x38\x73\x40\xFF\x7A\x00\x5F\xE0\x5F\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[9],  "\x40\xFF\x38\x73\x5F\xE0\x7A\x00\x5F\xFF\x5F\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[10], "\x40\xE0\x38\x73\x5F\xE0\x7A\x00\x5F\xFF\x5F\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[11], "\x5F\xFF\x38\x73\x40\xFF\x7A\x00\x40\xE0\x40\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[12], "\x5F\xFF\x38\x73\x5F\xE0\x7A\x00\x40\xE0\x40\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[13], "\x40\xE0\x38\x73\x5F\xE0\x7A\x00\x40\xFF\x40\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[14], "\x40\xFF\x38\x73\x5F\xFF\x7A\x00\x5F\xE0\x5F\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[15], "\x5F\xE0\x38\x73\x5F\xFF\x7A\x00\x40\xFF\x40\xFF", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[16], "\x40\xFF\x38\x73\x40\xE0\x7A\x00\x5F\xE0\x5F\xE0", sizeof(PSX::TextureLayout));
	memcpy(&cactusTextureLayouts[17], "\x40\xFF\x38\x73\x5F\xE0\x7A\x00\x40\xE0\x40\xE0", sizeof(PSX::TextureLayout));
	const size_t offCactusTextureLayouts = currOffset;
	printf(nameof(offCactusTextureLayouts) " = %zx\n", offCactusTextureLayouts);
  currOffset += sizeof(cactusTextureLayouts);

	const size_t offCactusTextureLayout_ptrArray = currOffset;
	printf(nameof(offCactusTextureLayout_ptrArray) " = %zx\n", offCactusTextureLayout_ptrArray);
	uint32_t cactusTextureLayout_ptrArray[18] =
	{
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 0),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 1),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 2),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 3),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 4),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 5),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 6),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 7),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 8),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 9),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 10),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 11),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 12),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 13),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 14),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 15),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 16),
		offCactusTextureLayouts + (sizeof(PSX::TextureLayout) * 17),
	};
  currOffset += sizeof(cactusTextureLayout_ptrArray);

	//PSX::CLUT cactusColorsCLUT = {};
	//memcpy(&cactusColorsCLUT, "\x2F\x33", sizeof(PSX::CLUT));
 // const size_t offCactusCLUT = currOffset;
	//printf(nameof(offCactusCLUT) " = %zx\n", offCactusCLUT);
 // currOffset += sizeof(PSX::CLUT);

	char cactusColorsCLUTData[256] = 
	{
		0x2F, 0x33, 0x19, 0x00, 0xCB, 0xB6, 0x2D, 0x00, 0x9E, 0x8F, 0x29, 0x00, 0x0E, 0x0C, 0x0E, 0x00, 0xD2, 0xBD, 0x2E, 0x00, 0x84, 0x79, 0x26, 0x00, 0x20, 0x22, 0x13, 0x00, 0x24, 0x24, 0x17, 0x00, 0x2F, 0x31, 0x19, 0x00, 0xD0, 0xB9, 0x2E, 0x00, 0x22, 0x22, 0x15, 0x00, 0xC5, 0xB2, 0x2C, 0x00, 0x65, 0x65, 0x29, 0x00, 0x3C, 0x42, 0x16, 0x00, 0x11, 0x0F, 0x11, 0x00, 0x81, 0x76, 0x26, 0x00, 0xFF, 0xE5, 0x38, 0x00, 0x42, 0x46, 0x20, 0x00, 0x12, 0x10, 0x12, 0x00, 0x25, 0x29, 0x2C, 0x00, 0x1A, 0x1C, 0x0C, 0x00, 0x4E, 0x48, 0x20, 0x00, 0x1B, 0x11, 0x2A, 0x00, 0x66, 0x6E, 0x29, 0x00, 0x41, 0x46, 0x1D, 0x00, 0x3D, 0x43, 0x17, 0x00, 0x80, 0x8C, 0x2F, 0x00, 0x62, 0x6B, 0x24, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x95, 0x00, 0x01, 0x00, 0x54, 0x49, 0x06, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x4F, 0x07, 0x82, 0x07, 0x49, 0x03, 0x00, 0x00, 0xB4, 0x49, 0x06, 0x00, 0x74, 0x4B, 0x06, 0x00, 0xC4, 0x4C, 0x06, 0x00, 0x04, 0x51, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04, 0x00, 0x00, 0xAE, 0xFF, 0xEE, 0xFF, 0xAE, 0xFF, 0x52, 0x00, 0x92, 0x00, 0x52, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x52, 0x00, 0xDF, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x57, 0xB8, 0x01, 0x00, 0x57, 0x38, 0x01, 0x00, 0x57, 0x38, 0x02, 0x00, 0x57, 0x18, 0x03, 0x22, 0x57, 0xB8
	};
	const size_t offCactusColorsCLUTData = currOffset;
	printf(nameof(offCactusColorsCLUTData) " = %zx\n", offCactusColorsCLUTData);
  currOffset += sizeof(cactusColorsCLUTData);

	//set up InstDef
	memcpy(&cactus.name, "cactus_saguro#2", 16);
	cactus.offModel = offCactusModel;
	cactus.scale.x = cactus.scale.y = cactus.scale.z = 0x1000;
	cactus.maybeScaleMaybePadding = 0x0;
	cactus.colorRGBA = 0x0;
	//cactus.flags = 0xB;
	//0xB = 1011 (cactus)
	//0xF = 1111 (armadillo)
	cactus.flags = 0xB;
	cactus.unk24 = 0x0;
	cactus.unk28 = 0x0;
	cactus.offInstance = 0x0; //filled in at runtime (connect InstDef to Instance)
	//cactus.pos.x = 0x0C2D;
	//cactus.pos.y = 0x0900;
	//cactus.pos.z = 0xED86;
  //cactus.pos.x = 0x4000;
	cactus.pos.x = 0x0000;
	cactus.pos.y = 0x0000;
	cactus.pos.z = 0x0000;
	cactus.rot.x = 0x0000;
	cactus.rot.y = 0xFF94;
	cactus.rot.z = 0x0000;
	cactus.modelID = 0xFFFFFFFF;

	//set up Model
	memcpy(&cactusModel.name, "cactus_saguro", 14);
	cactusModel.id = 0xFFFF;
	cactusModel.numHeaders = 0x0001;
	cactusModel.offHeaders = offCactusModelHeader;

  //set up ModelHeader
	memcpy(&cactusModelHeader.name, "cactus_saguro_h", 16);
	cactusModelHeader.unk1 = 0x0;
	cactusModelHeader.maxDistanceLOD = 0x2000;
	cactusModelHeader.flags = 0x0000;
	cactusModelHeader.scale.x = 0x271E;
	cactusModelHeader.scale.y = 0x2B3B;
	cactusModelHeader.scale.z = 0x0E5E;
  cactusModelHeader.maybeScaleMaybePadding = 0x0;
	cactusModelHeader.offCommandList = offCommandList;
  cactusModelHeader.offFrameData = offCactusModelFrame;
	cactusModelHeader.offTexLayout = offCactusTextureLayout_ptrArray;
	cactusModelHeader.offColors = offCactusColorsCLUTData;
	cactusModelHeader.unk3 = 0x0;
	cactusModelHeader.numAnimations = 0x0;
	cactusModelHeader.offAnimations = 0x0;
	cactusModelHeader.offAnimtex = 0x0;

	//set up commandList
	//already taken care of up top

	//set up cactusModelFrame
	cactusModelFrame.pos.x = 0xFF65;
	cactusModelFrame.pos.y = 0x0000;
	cactusModelFrame.pos.z = 0xFF7E;
	cactusModelFrame.maybePosMaybePadding = 0x0;
	memcpy(&cactusModelFrame.unk16, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);
	cactusModelFrame.vertexOffset = 0x1C;
	//TODO verts immediately following this??? not sure

	//set up cactusColorsCLUT
	//already taken care of up top

	//char bunchOfFs[] =
	//{
	//	0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	//};
	//const size_t offBunchOfFs = currOffset;
	//printf(nameof(offBunchOfFs) " = %zx\n", offBunchOfFs);
	//currOffset += sizeof(offBunchOfFs);

	//header.offUnk_0x1C = offBunchOfFs;
	//header.offUnk_0x20 = offBunchOfFs;
	//header.offUnk_0xCC = offBunchOfFs;
	//header.offUnk_0xD0 = offBunchOfFs;
	//header.unk_0x170 = offBunchOfFs;

#define CALCULATE_OFFSET(s, m, b) static_cast<uint32_t>(offsetof(s, m) + b)

	const size_t offPaddingForMultOfFour = currOffset;
	printf(nameof(offPaddingForMultOfFour) " = %zx\n", offPaddingForMultOfFour);
	size_t paddingSizeForMultOfFour = 4 - (offPaddingForMultOfFour % 4);
	paddingSizeForMultOfFour = (paddingSizeForMultOfFour == 4) ? 0 : paddingSizeForMultOfFour;

	const size_t offPointerMap = currOffset;
	printf(nameof(offPointerMap) " = %zx\n", offPointerMap);

	std::vector<uint32_t> pointerMap =
	{
		CALCULATE_OFFSET(PSX::LevHeader, offMeshInfo, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offInstances, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offModels, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offModelInstances, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offExtra, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offCheckpointNodes, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offVisMem, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offAnimTex, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offLevNavTable, offHeader),

		//CALCULATE_OFFSET(PSX::LevHeader, offUnk_0x1C, offHeader),
		//CALCULATE_OFFSET(PSX::LevHeader, offUnk_0x20, offHeader),
		//CALCULATE_OFFSET(PSX::LevHeader, offUnk_0xCC, offHeader),
		//CALCULATE_OFFSET(PSX::LevHeader, offUnk_0xD0, offHeader),
		//CALCULATE_OFFSET(PSX::LevHeader, unk_0x170, offHeader),

		CALCULATE_OFFSET(PSX::MeshInfo, offQuadblocks, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offVertices, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offBSPNodes, offMeshInfo),
		CALCULATE_OFFSET(PSX::VisualMem, offNodes[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offQuads[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offBSP[0], offVisMem),

		CALCULATE_OFFSET(PSX::InstDef, offModel, offCactus),
		CALCULATE_OFFSET(PSX::Model, offHeaders, offCactusModel),
		CALCULATE_OFFSET(PSX::ModelHeader, offCommandList, offCactusModelHeader),
		CALCULATE_OFFSET(PSX::ModelHeader, offFrameData, offCactusModelHeader),
		CALCULATE_OFFSET(PSX::ModelHeader, offTexLayout, offCactusModelHeader),
		CALCULATE_OFFSET(PSX::ModelHeader, offColors, offCactusModelHeader),
		(uint32_t)offInstDefList_ptrArray,
		(uint32_t)offInstDefList2_ptrArray,
		(uint32_t)offModelList_ptrArray,
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 0),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 1),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 2),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 3),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 4),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 5),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 6),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 7),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 8),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 9),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 10),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 11),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 12),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 13),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 14),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 15),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 16),
		(uint32_t)(offCactusTextureLayout_ptrArray + sizeof(uint32_t) * 17),
	};

	if (offTropyGhost != 0) { pointerMap.push_back(CALCULATE_OFFSET(PSX::LevelExtraHeader, offsets[PSX::LevelExtra::N_TROPY_GHOST], offExtraHeader)); }
	if (offOxideGhost != 0) { pointerMap.push_back(CALCULATE_OFFSET(PSX::LevelExtraHeader, offsets[PSX::LevelExtra::N_OXIDE_GHOST], offExtraHeader)); }

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

	#undef CALCULATE_OFFSET

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
  Write(file, &cactus, sizeof(cactus));
	Write(file, &instDefList_ptrArray[0], sizeof(instDefList_ptrArray));
	Write(file, &instDefList2_ptrArray[0], sizeof(instDefList2_ptrArray));
	Write(file, &cactusModel, sizeof(cactusModel));
	Write(file, &modelList_ptrArray[0], sizeof(modelList_ptrArray));
  Write(file, &cactusModelHeader, sizeof(cactusModelHeader));
	Write(file, &commandList, sizeof(commandList));
  Write(file, &cactusModelFrame, sizeof(cactusModelFrame));
	Write(file, &cactusVertexData[0], sizeof(cactusVertexData));
  Write(file, &cactusTextureLayouts[0], sizeof(cactusTextureLayouts));
	Write(file, &cactusTextureLayout_ptrArray[0], sizeof(cactusTextureLayout_ptrArray));
  Write(file, &cactusColorsCLUTData, sizeof(cactusColorsCLUTData));
	//Write(file, &bunchOfFs[0], sizeof(bunchOfFs));
	uint32_t fourBytesOfZero = 0;
	if (paddingSizeForMultOfFour > 0)
	{
		printf("WARNING: HAD TO PAD %d BYTES\n", paddingSizeForMultOfFour);
		Write(file, &fourBytesOfZero, paddingSizeForMultOfFour);
	}
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
		if (command == "o")
		{
			bool isLevInstStub = false;
			for (const auto& e : SimpleLevelInstances::mesh_prefixes_to_enum)
			{
				if (tokens[1].starts_with(e.first))
				{
					//for position, you need to get all the verts that belong to this mesh, calculate their average pos.
					//for rotation, idk because obj file format bakes the rotation into the position.
					//maybe calculate it based on the normal of the quad that it's sitting on?
					Vec3 pos{};
					Vec3 rot{}; //don't know for now
					//m_levInstPosRot[e.second] = std::make_tuple(pos, rot);
					m_levelInstancesModels.push_back(Model(&SimpleLevelInstances::GetMeshInstance(e.second), glm::vec3(pos.x, pos.y, pos.z), glm::vec3(1.f, 1.f, 1.f), glm::quat(0.f, 0.f, 0.f, 0.f)));
					isLevInstStub = true;
					break;
				}
			}
			//if (isLevInstStub) { continue; } we want to skip the whole object
			if (tokens.size() < 2 || meshMap.contains(tokens[1]))
			{
				ret = false;
				m_showLogWindow = true;
				m_invalidQuadblocks.emplace_back(tokens[1], "Duplicated mesh name.");
				continue;
			}
			currQuadblockName = tokens[1];
			currQuadblockGoodUV = true;
			meshMap[currQuadblockName] = false;
			quadblockCount++;
		}
		else if (command == "v")
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
						m_propVisTreeTransparent.SetDefaultValue(material, false);
						m_propTerrain.RegisterMaterial(this);
						m_propQuadFlags.RegisterMaterial(this);
						m_propDoubleSided.RegisterMaterial(this);
						m_propCheckpoints.RegisterMaterial(this);
						m_propTurboPads.RegisterMaterial(this);
						m_propSpeedImpact.RegisterMaterial(this);
						m_propCheckpointPathable.RegisterMaterial(this);
						m_propVisTreeTransparent.RegisterMaterial(this);
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
			const bool semiTransparent = texture.IsSemiTransparent();
			m_propVisTreeTransparent.SetDefaultValue(material, semiTransparent);

			const std::filesystem::path& texPath = texture.GetPath();
			const std::vector<size_t>& quadblockIndexes = m_materialToQuadblocks[material];
			for (const size_t index : quadblockIndexes)
			{
				m_quadblocks[index].SetTexPath(texPath);
				m_quadblocks[index].SetVisTreeTransparent(semiTransparent);
			}
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

void Level::InitModels(Renderer& renderer)
{
	m_models[LevelModels::LEVEL] = renderer.CreateModel();
	m_models[LevelModels::LEVEL]->SetRenderCondition([]() { return GuiRenderSettings::showLevel; });

	m_models[LevelModels::BSP] = m_models[LevelModels::LEVEL]->AddModel();
	m_models[LevelModels::BSP]->SetRenderCondition([]() { return GuiRenderSettings::showBspRectTree; });

	m_models[LevelModels::SPAWN] = m_models[LevelModels::LEVEL]->AddModel();
	m_models[LevelModels::SPAWN]->SetRenderCondition([]() { return GuiRenderSettings::showStartpoints; });

	m_models[LevelModels::CHECKPOINT] = m_models[LevelModels::LEVEL]->AddModel();
	m_models[LevelModels::CHECKPOINT]->SetRenderCondition([]() { return GuiRenderSettings::showCheckpoints; });

	m_models[LevelModels::SELECTED] = m_models[LevelModels::LEVEL]->AddModel();

	m_models[LevelModels::MULTI_SELECTED] = m_models[LevelModels::LEVEL]->AddModel();
	m_models[LevelModels::MULTI_SELECTED]->SetRenderCondition([]() { return GuiRenderSettings::showVisTree; });

	m_models[LevelModels::FILTER] = m_models[LevelModels::LEVEL]->AddModel();
	m_models[LevelModels::FILTER]->SetRenderCondition([]() { return GuiRenderSettings::filterActive; });
}

void Level::GenerateRenderLevData()
{
	if (!m_models[LevelModels::LEVEL] || !m_models[LevelModels::FILTER]) { return; }

	std::vector<Primitive> levTriangles;
	std::vector<Primitive> filterTriangles;
	levTriangles.reserve(m_quadblocks.size() * 8);
	filterTriangles.reserve(m_quadblocks.size() * 8);

	auto CountPrimitiveTriangles = [](const std::vector<Primitive>& primitives)
		{
			size_t count = 0;
			for (const Primitive& primitive : primitives) { count += (primitive.type == PrimitiveType::QUAD) ? 2 : 1; }
			return count;
		};

	size_t triangleOffset = 0;
	for (Quadblock& qb : m_quadblocks)
	{
		qb.SetRenderPrimitiveIndex(triangleOffset);
		std::vector<Primitive> qbTriangles = qb.ToGeometry(false);
		std::vector<Primitive> qbFilterTriangles = qb.ToGeometry(true);
		levTriangles.insert(levTriangles.end(), qbTriangles.begin(), qbTriangles.end());
		filterTriangles.insert(filterTriangles.end(), qbFilterTriangles.begin(), qbFilterTriangles.end());
		const size_t qbTriCount = CountPrimitiveTriangles(qbTriangles);
		triangleOffset += qbTriCount;
	}

	m_models[LevelModels::LEVEL]->GetMesh().SetGeometry(levTriangles, Mesh::RenderFlags::AllowPointRender | Mesh::RenderFlags::QuadblockLod, Mesh::ShaderFlags::None);
	m_models[LevelModels::FILTER]->GetMesh().SetGeometry(filterTriangles,
		Mesh::RenderFlags::DrawWireframe | Mesh::RenderFlags::DrawBackfaces | Mesh::RenderFlags::ForceDrawOnTop | Mesh::RenderFlags::DrawLinesAA | Mesh::RenderFlags::DontOverrideRenderFlags | Mesh::RenderFlags::ThickLines | Mesh::RenderFlags::QuadblockLod,
		Mesh::ShaderFlags::DiscardZeroColor);
}

void Level::UpdateAnimationRenderData()
{
	if (!m_models[LevelModels::LEVEL]) { return; }

	for (const AnimTexture& animTex : m_animTextures)
	{
		if (!animTex.IsPopulated()) { continue; }
		const std::vector<Texture>& textures = animTex.GetTextures();
		const AnimTextureFrame& frame = animTex.GetRenderFrame();
		for (size_t qbIndex : animTex.GetQuadblockIndexes())
		{
			Quadblock& qb = m_quadblocks[qbIndex];
			const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& uvs = frame.uvs;
			const size_t basePrimitiveIndex = qb.GetRenderPrimitiveIndex();
			if (basePrimitiveIndex == RENDER_INDEX_NONE) { continue; }

			const std::filesystem::path texturePath = textures[frame.textureIndex].GetPath();
			std::vector<Primitive> qbTriangles = qb.ToGeometry(false, &uvs, &texturePath);
			size_t primitiveIndex = basePrimitiveIndex;
			for (const Primitive& primitive : qbTriangles)
			{
				primitiveIndex = m_models[LevelModels::LEVEL]->GetMesh().UpdatePrimitive(primitive, primitiveIndex);
			}
		}
	}
}

void Level::UpdateFilterRenderData(const Quadblock& qb)
{
	if (!m_models[LevelModels::FILTER]) { return; }

	const size_t basePrimitiveIndex = qb.GetRenderPrimitiveIndex();
	if (basePrimitiveIndex == RENDER_INDEX_NONE) { return; }

	std::vector<Primitive> qbFilterTriangles = qb.ToGeometry(true);
	size_t primitiveIndex = basePrimitiveIndex;
	for (const Primitive& primitive : qbFilterTriangles)
	{
		primitiveIndex = m_models[LevelModels::FILTER]->GetMesh().UpdatePrimitive(primitive, primitiveIndex);
	}
}

void Level::GenerateRenderBspData()
{
	if (!m_models[LevelModels::BSP]) { return; }

	struct NodeDepth
	{
		const BSP* node;
		int depth;
	};
	std::vector<Primitive> triangles;
	std::vector<NodeDepth> stack;
	stack.push_back({&m_bsp, 0});
	GuiRenderSettings::bspTreeMaxDepth = 0;
	while (!stack.empty())
	{
		const NodeDepth entry = stack.back();
		stack.pop_back();
		if (!entry.node) { continue; }

		if (GuiRenderSettings::bspTreeMaxDepth < entry.depth)
		{
			GuiRenderSettings::bspTreeMaxDepth = entry.depth;
		}

		const bool drawDepth = (GuiRenderSettings::bspTreeTopDepth <= entry.depth && GuiRenderSettings::bspTreeBottomDepth >= entry.depth);
		if (drawDepth)
		{
			const Color c = Color(entry.depth * 30.0, 1.0, 1.0);
			std::vector<Primitive> nodeTriangles = entry.node->GetBoundingBox().ToGeometry();
			for (Primitive& primitive : nodeTriangles)
			{
				for (unsigned i = 0; i < primitive.pointCount; i++) { primitive.p[i].color = c; }
				triangles.push_back(primitive);
			}
		}

		if (entry.node->GetLeftChildren() != nullptr)
		{
			stack.push_back({entry.node->GetLeftChildren(), entry.depth + 1});
		}
		if (entry.node->GetRightChildren() != nullptr)
		{
			stack.push_back({entry.node->GetRightChildren(), entry.depth + 1});
		}
	}

	m_models[LevelModels::BSP]->GetMesh().SetGeometry(triangles, Mesh::RenderFlags::DrawWireframe | Mesh::RenderFlags::DontOverrideRenderFlags);
}

void Level::UpdateRenderCheckpointData()
{
	Model* checkpointModel = m_models[LevelModels::CHECKPOINT];
	if (!checkpointModel) { return; }

	checkpointModel->ClearModels();
	if (m_checkpoints.empty())
	{
		checkpointModel->GetMesh().Clear();
		return;
	}

	std::vector<Primitive> checkTriangles;
	checkTriangles.reserve(m_checkpoints.size() * 8);
	std::unordered_set<int> selectedCheckpointIndexes;
	for (size_t index : m_rendererSelectedQuadblockIndexes)
	{
		int checkpointIndex = m_quadblocks[index].GetCheckpoint();
		selectedCheckpointIndexes.insert(checkpointIndex);
	}

	constexpr float labelHeightOffset = 1.5f;
	for (const Checkpoint& e : m_checkpoints)
	{
		bool selected = selectedCheckpointIndexes.contains(e.GetIndex());
		const Color& c = selected ? GuiRenderSettings::selectedCheckpointColor : e.GetColor();
		Vertex v = Vertex(Point(e.GetPos().x, e.GetPos().y, e.GetPos().z, c.r, c.g, c.b));
		const std::vector<Primitive> tris = v.ToGeometry();
		checkTriangles.insert(checkTriangles.end(), tris.begin(), tris.end());

		Model* label = checkpointModel->AddModel();
		label->GetMesh().SetGeometry("CP " + std::to_string(e.GetIndex()), Text3D::Align::CENTER, Color(c.r, c.g, c.b, static_cast<unsigned char>(255u)));
		Vec3 labelPos = e.GetPos();
		labelPos.y += labelHeightOffset;
		label->SetPosition(labelPos);
	}

	checkpointModel->GetMesh().SetGeometry(checkTriangles, Mesh::RenderFlags::DrawBackfaces | Mesh::RenderFlags::DontOverrideRenderFlags);
}

void Level::GenerateRenderStartpointData()
{
	if (!m_models[LevelModels::SPAWN]) { return; }

	std::vector<Primitive> spawnsTriangles;
	spawnsTriangles.reserve(m_spawn.size() * 8);

	for (const Spawn& e : m_spawn)
	{
		Vertex v = Vertex(Point(e.pos.x, e.pos.y, e.pos.z, 0, 128, 255));
		const std::vector<Primitive> tris = v.ToGeometry();
		spawnsTriangles.insert(spawnsTriangles.end(), tris.begin(), tris.end());
	}

	m_models[LevelModels::SPAWN]->GetMesh().SetGeometry(spawnsTriangles, Mesh::RenderFlags::DrawBackfaces | Mesh::RenderFlags::DontOverrideRenderFlags);
}

void Level::GenerateRenderSelectedBlockData(const Quadblock& quadblock, const Vec3& queryPoint)
{
	if (!m_models[LevelModels::SELECTED]) { return; }

	m_rendererQueryPoint = queryPoint;

	std::vector<Primitive> triangles;
	triangles.reserve(m_rendererSelectedQuadblockIndexes.size() * 8 + 8);

	const std::filesystem::path emptyTexturePath;
	const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1> emptyUvs = {};
	for (size_t index : m_rendererSelectedQuadblockIndexes)
	{
		const Quadblock& qb = m_quadblocks[index];
		std::vector<Primitive> qbTriangles = qb.ToGeometry(false, &emptyUvs, &emptyTexturePath);
		for (Primitive& primitive : qbTriangles)
		{
			for (unsigned i = 0; i < primitive.pointCount; i++) { primitive.p[i].color = primitive.p[i].color.Negated(); }
			triangles.push_back(primitive);
		}
	}

	Vertex v = Vertex(Point(queryPoint.x, queryPoint.y, queryPoint.z, 255, 0, 0));
	const std::vector<Primitive> queryTriangles = v.ToGeometry();
	triangles.insert(triangles.end(), queryTriangles.begin(), queryTriangles.end());

	m_models[LevelModels::SELECTED]->GetMesh().SetGeometry(triangles,
		Mesh::RenderFlags::DrawWireframe | Mesh::RenderFlags::DrawBackfaces | Mesh::RenderFlags::ForceDrawOnTop | Mesh::RenderFlags::DrawLinesAA | Mesh::RenderFlags::DontOverrideRenderFlags | Mesh::RenderFlags::QuadblockLod,
		Mesh::ShaderFlags::Blinky);

	if (GuiRenderSettings::showVisTree)
	{
		std::vector<const BSP*> bspLeaves = m_bsp.GetLeaves();
		size_t myBSPIndex = 0;
		for (size_t bsp_index = 0; bsp_index < bspLeaves.size(); bsp_index++)
		{
			const BSP& bsp = *bspLeaves[bsp_index];
			if (bsp.GetId() == quadblock.GetBSPID()) { myBSPIndex = bsp_index; }
		}

		std::vector<Primitive> multiTriangles;
		for (size_t bsp_index = 0; bsp_index < bspLeaves.size(); bsp_index++)
		{
			const BSP& bsp = *bspLeaves[bsp_index];
			if (m_bspVis.Get(bsp_index, myBSPIndex))
			{
				const std::vector<size_t> qbIndeces = bsp.GetQuadblockIndexes();
				for (size_t qbInd : qbIndeces)
				{
					Quadblock& qb = m_quadblocks[qbInd];
					std::vector<Primitive> qbTriangles = qb.ToGeometry(false, &emptyUvs, &emptyTexturePath);
					for (Primitive& primitive : qbTriangles)
					{
						for (unsigned i = 0; i < primitive.pointCount; i++) { primitive.p[i].color = primitive.p[i].color.Negated(); }
						multiTriangles.push_back(primitive);
					}
				}
			}
		}

		m_models[LevelModels::MULTI_SELECTED]->GetMesh().SetGeometry(multiTriangles,
			Mesh::RenderFlags::DrawWireframe | Mesh::RenderFlags::DrawBackfaces | Mesh::RenderFlags::ForceDrawOnTop | Mesh::RenderFlags::DrawLinesAA | Mesh::RenderFlags::DontOverrideRenderFlags | Mesh::RenderFlags::QuadblockLod,
			Mesh::ShaderFlags::Blinky);
	}
}

void Level::ViewportClickHandleBlockSelection(int pixelX, int pixelY, bool appendSelection, const Renderer& rend)
{
	std::function<std::optional<std::tuple<const Quadblock*, const glm::vec3>>(int, int, std::vector<Quadblock>&, unsigned)> check = [&rend](int pixelCoordX, int pixelCoordY, const std::vector<Quadblock>& qbs, unsigned index)
		{
			std::vector<std::tuple<const Quadblock*, glm::vec3, float>> passed;

			for (const Quadblock& qb : qbs)
			{
				bool collided = false;
				const Vertex* verts = qb.GetUnswizzledVertices();
				glm::vec3 tri[3];
				bool isQuadblock = qb.IsQuadblock();

				std::tuple<glm::vec3, float> queryResult;
				glm::vec3 worldSpaceRay = rend.ScreenspaceToWorldRay(pixelCoordX, pixelCoordY);

				tri[0] = glm::vec3(verts[0].m_pos.x, verts[0].m_pos.y, verts[0].m_pos.z);
				tri[1] = glm::vec3(verts[2].m_pos.x, verts[2].m_pos.y, verts[2].m_pos.z);
				tri[2] = glm::vec3(verts[6].m_pos.x, verts[6].m_pos.y, verts[6].m_pos.z);

				queryResult = rend.WorldspaceRayTriIntersection(worldSpaceRay, tri);
				collided |= (std::get<1>(queryResult) != -1.0f);

				if (collided) { passed.push_back(std::tuple<const Quadblock*, glm::vec3, float>(&qb, std::get<0>(queryResult), std::get<1>(queryResult))); continue; }

				if (!isQuadblock) { continue; }

				tri[0] = glm::vec3(verts[2].m_pos.x, verts[2].m_pos.y, verts[2].m_pos.z);
				tri[1] = glm::vec3(verts[6].m_pos.x, verts[6].m_pos.y, verts[6].m_pos.z);
				tri[2] = glm::vec3(verts[8].m_pos.x, verts[8].m_pos.y, verts[8].m_pos.z);

				queryResult = rend.WorldspaceRayTriIntersection(worldSpaceRay, tri);
				collided |= (std::get<1>(queryResult) != -1.0f);

				if (collided) { passed.push_back(std::tuple<const Quadblock*, glm::vec3, float>(&qb, std::get<0>(queryResult), std::get<1>(queryResult))); continue; }
			}

			// sort collided blocks by time value (distance from camera).
			std::sort(passed.begin(), passed.end(),
				[](const std::tuple<const Quadblock*, glm::vec3, float>& a, const std::tuple<const Quadblock*, glm::vec3, float>& b) {
					return std::get<2>(a) < std::get<2>(b);
				});

			std::optional<std::tuple<const Quadblock*, glm::vec3>> result;
			if (passed.size() > 0)
			{
				const auto& tuple = passed[index % passed.size()];
				const Quadblock* qb = std::get<0>(tuple);
				result = std::make_optional(std::tuple<const Quadblock*, glm::vec3>(qb, std::get<1>(tuple)));
			}
			else { result.reset(); }
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
		m_models[LevelModels::SELECTED]->GetMesh().Clear();
	}
	UpdateRenderCheckpointData();
}
