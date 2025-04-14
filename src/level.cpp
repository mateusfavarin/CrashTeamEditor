#include "level.h"
#include "psx_types.h"
#include "io.h"
#include "utils.h"
#include "geo.h"
#include "process.h"
#include "gui_render_settings.h"
#include "renderer.h"

#include <fstream>
#include <unordered_set>
#include <map>

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

bool Level::Loaded() const
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
	DeleteMaterials(this);
}

const std::string& Level::GetName() const
{
	return m_name;
}

const std::vector<Quadblock>& Level::GetQuadblocks() const
{
	return m_quadblocks;
}

const std::filesystem::path& Level::GetParentPath() const
{
	return m_parentPath;
}

bool Level::GenerateBSP()
{
	std::vector<size_t> quadIndexes;
	for (size_t i = 0; i < m_quadblocks.size(); i++) { quadIndexes.push_back(i); }
	m_bsp.Clear();
	m_bsp.SetQuadblockIndexes(quadIndexes);
	m_bsp.Generate(m_quadblocks, MAX_QUADBLOCKS_LEAF, MAX_LEAF_AXIS_LENGTH);
	if (m_bsp.Valid())
	{
		GenerateRenderBspData(m_bsp);
		return true;
	}
	m_bsp.Clear();
	return false;
}

enum class PresetHeader : unsigned
{
	SPAWN, LEVEL, PATH, MATERIAL, TURBO_PAD, ANIM_TEXTURES
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
					if (m_bsp.Valid())
					{
						m_bsp.Clear();
						GenerateRenderBspData(m_bsp);
					}
				}
			}
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
			materialJson[key + "_trigger"] = m_propTurboPads.GetBackup(key);
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
	return true;
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
		turboPad.TranslateNormalVec(0.25f);
		turboPad.SetCheckpoint(-1);
		turboPad.SetCheckpointStatus(false);
		turboPad.SetName(quadblock.GetName() + (stp ? "_stp" : "_tp"));
		turboPad.SetFlag(QuadFlags::TRIGGER_SCRIPT | QuadFlags::INVISIBLE_TRIGGER | QuadFlags::WALL | QuadFlags::DEFAULT);
		turboPad.SetTerrain(stp ? TerrainType::SUPER_TURBO_PAD : TerrainType::TURBO_PAD);
		turboPad.SetTurboPadIndex(TURBO_PAD_INDEX_NONE);
		turboPad.SetHide(true);

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
	PSX::LevHeader header;
	Read(file, header);
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

	if (m_bsp.Empty()) { GenerateBSP(); }

	std::vector<const BSP*> bspNodes = m_bsp.GetTree();
	std::vector<const BSP*> orderedBSPNodes(bspNodes.size());
	for (const BSP* bsp : bspNodes) { orderedBSPNodes[bsp->Id()] = bsp; }

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

	std::vector<uint8_t> vrm = PackVRM(textures);
	const bool hasVRM = !vrm.empty();

	std::vector<uint8_t> animData;
	std::vector<size_t> animPtrMapOffsets;
	std::vector<PSX::TextureGroup> texGroups;
	std::unordered_map<PSX::TextureLayout, size_t> savedLayouts;
	if (hasVRM)
	{
		for (auto& [from, to] : copyTextureAttributes)
		{
			to->CopyVRAMAttributes(*from);
		}

		for (auto& [material, texture] : m_materialToTexture)
		{
			std::vector<size_t>& quadIndexes = m_materialToQuadblocks[material];
			for (size_t index : quadIndexes)
			{
				Quadblock& currQuad = m_quadblocks[index];
				if (currQuad.IsAnimated()) { continue; }
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
		Write(vrmFile, vrm.data(), vrm.size());
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
	/*
		TODO: run some sort of visibility algorithm,
		generate visible nodes/quads depending on the needs of every quadblock.
	*/
	std::vector<uint32_t> visibleNodeAll(visNodeSize, 0xFFFFFFFF);
	for (const BSP* bsp : orderedBSPNodes)
	{
		if (bsp->GetFlags() & BSPFlags::INVISIBLE) { visibleNodeAll[bsp->Id() / BITS_PER_SLOT] &= ~(1 << (bsp->Id() % BITS_PER_SLOT)); }
	}
	visibleNodes.push_back({visibleNodeAll, currOffset});
	currOffset += visibleNodeAll.size() * sizeof(uint32_t);

	std::vector<uint32_t> visibleQuadsAll(visQuadSize, 0xFFFFFFFF);
	size_t quadIndex = 0;
	for (const Quadblock* quad : orderedQuads)
	{
		if (quad->GetFlags() & (QuadFlags::INVISIBLE | QuadFlags::INVISIBLE_TRIGGER))
		{
			visibleQuadsAll[quadIndex / BITS_PER_SLOT] &= ~(1 << (quadIndex % BITS_PER_SLOT));
		}
		quadIndex++;
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

	size_t quadCount = 0;
	for (const Quadblock* quad : orderedQuads)
	{
		/* TODO: read quadblock data */
		PSX::VisibleSet set = {};
		set.offVisibleBSPNodes = static_cast<uint32_t>(std::get<size_t>(visibleNodes[0]));
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

		PSX::Quadblock* serializedQuad = reinterpret_cast<PSX::Quadblock*>(serializedQuads[quadCount++].data());
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

	const size_t offPointerMap = currOffset;

	header.offMeshInfo = static_cast<uint32_t>(offMeshInfo);
	header.offAnimTex = static_cast<uint32_t>(offAnimData);
	for (size_t i = 0; i < NUM_DRIVERS; i++)
	{
		header.driverSpawn[i].pos = ConvertVec3(m_spawn[i].pos, FP_ONE_GEO);
		header.driverSpawn[i].rot = ConvertVec3(m_spawn[i].rot);
	}
	header.config = m_configFlags;
	for (size_t i = 0; i < NUM_GRADIENT; i++)
	{
		header.skyGradient[i].posFrom = ConvertFloat(m_skyGradient[i].posFrom, 1u);
		header.skyGradient[i].posTo = ConvertFloat(m_skyGradient[i].posTo, 1u);
		header.skyGradient[i].colorFrom = ConvertColor(m_skyGradient[i].colorFrom);
		header.skyGradient[i].colorTo = ConvertColor(m_skyGradient[i].colorTo);
	}
	header.offExtra = static_cast<uint32_t>(offExtraHeader);
	header.numCheckpointNodes = static_cast<uint32_t>(m_checkpoints.size());
	header.offCheckpointNodes = static_cast<uint32_t>(offCheckpoints);
	header.offVisMem = static_cast<uint32_t>(offVisMem);
	header.offLevNavTable = static_cast<uint32_t>(offNavHeaders);

#define CALCULATE_OFFSET(s, m, b) static_cast<uint32_t>(offsetof(s, m) + b)

	std::vector<uint32_t> pointerMap =
	{
		CALCULATE_OFFSET(PSX::LevHeader, offMeshInfo, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offExtra, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offCheckpointNodes, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offVisMem, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offAnimTex, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offLevNavTable, offNavHeaders),
		CALCULATE_OFFSET(PSX::MeshInfo, offQuadblocks, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offVertices, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offBSPNodes, offMeshInfo),
		CALCULATE_OFFSET(PSX::VisualMem, offNodes[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offQuads[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offBSP[0], offVisMem),
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
			Vec2 uv = {std::stof(tokens[1]), 1.0f - std::stof(tokens[2])};
			if (currQuadblockGoodUV && (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1))
			{
				m_showLogWindow = true;
				currQuadblockGoodUV = false;
				m_invalidQuadblocks.emplace_back(currQuadblockName, "UV outside of expect range [0.0f, 1.0f].");
			}
			uvs.emplace_back(uv);
		}
		else if (command == "o")
		{
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
				m_showLogWindow = true;
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
				m_showLogWindow = true;
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
						m_propTerrain.RegisterMaterial(this);
						m_propQuadFlags.RegisterMaterial(this);
						m_propDoubleSided.RegisterMaterial(this);
						m_propCheckpoints.RegisterMaterial(this);
						m_propTurboPads.RegisterMaterial(this);
					}
				}
				if (isQuadblock)
				{
					Quad& q0 = quadMap[currQuadblockName][0];
					Quad& q1 = quadMap[currQuadblockName][1];
					Quad& q2 = quadMap[currQuadblockName][2];
					Quad& q3 = quadMap[currQuadblockName][3];
					try
					{
						m_quadblocks.emplace_back(currQuadblockName, q0, q1, q2, q3, averageNormal, material, currQuadblockGoodUV);
						meshMap[currQuadblockName] = true;
					}
					catch (const QuadException& e)
					{
						ret = false;
						m_showLogWindow = true;
						m_invalidQuadblocks.emplace_back(currQuadblockName, e.what());
					}
				}
				else
				{
					Tri& t0 = triMap[currQuadblockName][0];
					Tri& t1 = triMap[currQuadblockName][1];
					Tri& t2 = triMap[currQuadblockName][2];
					Tri& t3 = triMap[currQuadblockName][3];
					try
					{
						m_quadblocks.emplace_back(currQuadblockName, t0, t1, t2, t3, averageNormal, material, currQuadblockGoodUV);
						meshMap[currQuadblockName] = true;
					}
					catch (const QuadException& e)
					{
						ret = false;
						m_showLogWindow = true;
						m_invalidQuadblocks.emplace_back(currQuadblockName, e.what());
					}
				}
			}
		}
	}
	file.close();

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
	constexpr size_t GHOST_SIZE_ADDR = 0x80280000;
	constexpr size_t GHOST_DATA_ADDR = 0x80280004;
	data.resize(Process::At<uint32_t>(GHOST_SIZE_ADDR));
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

void Level::RefreshTextureStores()
{
	int texturePathsIdCounter = 0;
	std::map<int, std::filesystem::path> texturePaths;

	for (size_t matIndex = 0; matIndex < m_materialToQuadblocks.size(); matIndex++)
	{
		auto begin = m_materialToQuadblocks.begin();
		std::advance(begin, matIndex);
		const std::string& mat = begin->first;
		const std::vector<size_t>& quadblockIndeces = begin->second;
		for (size_t qbIndexIndex = 0; qbIndexIndex < quadblockIndeces.size(); qbIndexIndex++)
		{
			const auto& texPath = m_materialToTexture[mat].GetPath();
			int texIndex;

			auto findResult = std::find_if(texturePaths.begin(), texturePaths.end(), [&texPath](const auto& pair) { return pair.second == texPath; });
			if (findResult != texturePaths.end())
			{ //this path already exists in the map
				texIndex = findResult->first;
			}
			else
			{
				texIndex = texturePathsIdCounter;
				texturePaths[texIndex] = texPath;
				texturePathsIdCounter++;
			}
		}
	}

	m_highLODMesh.SetTextureStore(texturePaths);
	m_lowLODMesh.SetTextureStore(texturePaths);
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

void Level::GeomUVs(const Quadblock& qb, int quadInd, int vertInd, std::vector<float>& data, int textureIndex)
{

	/* 062 is triblock
	p0 -- p1 -- p2
	|  q0 |  q1 |
	p3 -- p4 -- p5
	|  q2 |  q3 |
	p6 -- p7 -- p8
	*/
	const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& uvs = qb.GetUVs();
	const QuadUV& quv = uvs[quadInd];
	int vertIndInUvs = 9999999;
	switch (quadInd)
	{
	case 0:
		switch (vertInd)
		{
		case 0:
			vertIndInUvs = 0;
			break;
		case 1:
			vertIndInUvs = 1;
			break;
		case 3:
			vertIndInUvs = 2;
			break;
		case 4:
			vertIndInUvs = 3;
			break;
		}
		break;
	case 1:
		switch (vertInd)
		{
		case 1:
			vertIndInUvs = 0;
			break;
		case 2:
			vertIndInUvs = 1;
			break;
		case 4:
			vertIndInUvs = 2;
			break;
		case 5:
			vertIndInUvs = 3;
			break;
		}
		break;
	case 2:
		switch (vertInd)
		{
		case 3:
			vertIndInUvs = 0;
			break;
		case 4:
			vertIndInUvs = 1;
			break;
		case 6:
			vertIndInUvs = 2;
			break;
		case 7:
			vertIndInUvs = 3;
			break;
		}
		break;
	case 3:
		switch (vertInd)
		{
		case 4:
			vertIndInUvs = 0;
			break;
		case 5:
			vertIndInUvs = 1;
			break;
		case 7:
			vertIndInUvs = 2;
			break;
		case 8:
			vertIndInUvs = 3;
			break;
		}
		break;
	case 4: //low LOD
		switch (vertInd)
		{
		case 0:
			vertIndInUvs = 0;
			break;
		case 2:
			vertIndInUvs = 1;
			break;
		case 6:
			vertIndInUvs = 2;
			break;
		case 8:
			vertIndInUvs = 3;
			break;
		}
		break;
	}

	const Vec2& uv = quv[vertIndInUvs];
	data.push_back(uv.x);
	data.push_back(uv.y);
	union texIndexPun
	{
		int i;
		float f;
	} texIndexP;
	texIndexP.i = textureIndex;
	data.push_back(texIndexP.f); //todo, do this in geometry shader since ID is the same for all 3 verts for this tri
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

void Level::GeomBoundingRect(const BSP* b, int depth, std::vector<float>& data)
{
	constexpr float sqrtThree = 1.44224957031f;

	if (GuiRenderSettings::bspTreeMaxDepth < depth)
	{
		GuiRenderSettings::bspTreeMaxDepth = depth;
	}
	const BoundingBox& bb = b->GetBoundingBox();
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

	if (GuiRenderSettings::bspTreeTopDepth <= depth && GuiRenderSettings::bspTreeBottomDepth >= depth) {
		GeomPoint(verts, 2, data); //-+-
		GeomPoint(verts, 1, data); //--+
		GeomPoint(verts, 0, data); //---
		GeomPoint(verts, 5, data); //-++
		GeomPoint(verts, 1, data); //--+
		GeomPoint(verts, 2, data); //-+-

		GeomPoint(verts, 6, data); //+-+
		GeomPoint(verts, 3, data); //+--
		GeomPoint(verts, 0, data); //---
		GeomPoint(verts, 0, data); //---
		GeomPoint(verts, 1, data); //--+
		GeomPoint(verts, 6, data); //+-+

		GeomPoint(verts, 4, data); //++-
		GeomPoint(verts, 2, data); //-+-
		GeomPoint(verts, 0, data); //---
		GeomPoint(verts, 0, data); //---
		GeomPoint(verts, 3, data); //+--
		GeomPoint(verts, 4, data); //++-

		GeomPoint(verts, 7, data); //+++
		GeomPoint(verts, 4, data); //++-
		GeomPoint(verts, 3, data); //+--
		GeomPoint(verts, 3, data); //+--
		GeomPoint(verts, 6, data); //+-+
		GeomPoint(verts, 7, data); //+++

		GeomPoint(verts, 7, data); //+++
		GeomPoint(verts, 6, data); //+-+
		GeomPoint(verts, 5, data); //-++
		GeomPoint(verts, 5, data); //-++
		GeomPoint(verts, 6, data); //+-+
		GeomPoint(verts, 1, data); //--+

		GeomPoint(verts, 5, data); //-++
		GeomPoint(verts, 4, data); //++-
		GeomPoint(verts, 7, data); //+++
		GeomPoint(verts, 2, data); //-+-
		GeomPoint(verts, 4, data); //++-
		GeomPoint(verts, 5, data); //-++
	}

	if (b->GetLeftChildren() != nullptr)
	{
		GeomBoundingRect(b->GetLeftChildren(), depth + 1, data);
	}
	if (b->GetRightChildren() != nullptr)
	{
		GeomBoundingRect(b->GetRightChildren(), depth + 1, data);
	}
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

	int texturePathsIdCounter = 0;
	std::map<int, std::filesystem::path> texturePaths;

	for (size_t matIndex = 0; matIndex < m_materialToQuadblocks.size(); matIndex++)
	{
		auto begin = m_materialToQuadblocks.begin();
		std::advance(begin, matIndex);
		const std::string& mat = begin->first;
		const std::vector<size_t>& quadblockIndeces = begin->second;
		for (size_t qbIndexIndex = 0; qbIndexIndex < quadblockIndeces.size(); qbIndexIndex++)
		{
			size_t qbIndex = quadblockIndeces[qbIndexIndex];
			const Quadblock& qb = m_quadblocks[qbIndex];
			const Vertex* verts = qb.GetUnswizzledVertices();
			const auto& texPath = m_materialToTexture[mat].GetPath();
			int texIndex;

			auto findResult = std::find_if(texturePaths.begin(), texturePaths.end(), [&texPath](const auto& pair) { return pair.second == texPath; });
			if (findResult != texturePaths.end())
			{ //this path already exists in the map
				texIndex = findResult->first;
			}
			else
			{
				texIndex = texturePathsIdCounter;
				texturePaths[texIndex] = texPath;
				texturePathsIdCounter++;
			}

			//clockwise point ordering
			if (qb.IsQuadblock())
			{
				constexpr size_t HIGH_LOD_VERT_COUNT = 9;
				const std::array<int, HIGH_LOD_VERT_COUNT> highLODIndexes = {0, 1, 2, 3, 4, 5, 6, 7, 8};
				for (int index : highLODIndexes)
				{
					GeomOctopoint(verts, index, vertexHighLODData);
				}

				constexpr size_t LOW_LOD_VERT_COUNT = 4;
				const std::array<int, LOW_LOD_VERT_COUNT> lowLODIndexes = {0, 2, 6, 8};
				for (int index : lowLODIndexes)
				{
					GeomOctopoint(verts, index, vertexLowLODData);
				}

				for (int triIndex = 0; triIndex < 8; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, highLODData);
					GeomUVs(qb, triIndex / 2, firstPointIndex, highLODData, texIndex); //triIndex / 2 (integer division) because 2 tris per quad

					int secondPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, highLODData);
					GeomUVs(qb, triIndex / 2, secondPointIndex, highLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::quadHLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, highLODData);
					GeomUVs(qb, triIndex / 2, thirdPointIndex, highLODData, texIndex);
				}

				for (int triIndex = 0; triIndex < 2; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, lowLODData);
					GeomUVs(qb, 4, firstPointIndex, lowLODData, texIndex);

					int secondPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, lowLODData);
					GeomUVs(qb, 4, secondPointIndex, lowLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::quadLLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, lowLODData);
					GeomUVs(qb, 4, thirdPointIndex, lowLODData, texIndex);
				}
			}
			else
			{
				constexpr size_t HIGH_LOD_VERT_COUNT = 6;
				const std::array<int, HIGH_LOD_VERT_COUNT> highLODIndexes = {0, 1, 2, 3, 4, 6};
				for (int index : highLODIndexes)
				{
					GeomOctopoint(verts, index, vertexHighLODData);
				}

				constexpr size_t LOW_LOD_VERT_COUNT = 3;
				const std::array<int, LOW_LOD_VERT_COUNT> lowLODIndexes = {0, 2, 6};
				for (int index : lowLODIndexes)
				{
					GeomOctopoint(verts, index, vertexLowLODData);
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
					GeomUVs(qb, quadBlockIndex, firstPointIndex, highLODData, texIndex);

					int secondPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, highLODData);
					GeomUVs(qb, quadBlockIndex, secondPointIndex, highLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::triHLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, highLODData);
					GeomUVs(qb, quadBlockIndex, thirdPointIndex, highLODData, texIndex);
				}

				for (int triIndex = 0; triIndex < 1; triIndex++)
				{
					int firstPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][0];
					GeomPoint(verts, firstPointIndex, lowLODData);
					GeomUVs(qb, 4, firstPointIndex, lowLODData, texIndex);

					int secondPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][1];
					GeomPoint(verts, secondPointIndex, lowLODData);
					GeomUVs(qb, 4, secondPointIndex, lowLODData, texIndex);

					int thirdPointIndex = FaceIndexConstants::triLLODVertArrangements[triIndex][2];
					GeomPoint(verts, thirdPointIndex, lowLODData);
					GeomUVs(qb, 4, thirdPointIndex, lowLODData, texIndex);
				}
			}
		}
	}

	m_highLODMesh.UpdateMesh(highLODData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals | Mesh::VBufDataType::STUV | Mesh::VBufDataType::TexIndex), Mesh::ShaderSettings::None);
	m_highLODMesh.SetTextureStore(texturePaths);
	//m_highLODLevelModel.SetMesh(&m_highLODMesh);

	m_lowLODMesh.UpdateMesh(lowLODData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals | Mesh::VBufDataType::STUV | Mesh::VBufDataType::TexIndex), Mesh::ShaderSettings::None);
	m_lowLODMesh.SetTextureStore(texturePaths);
	//m_lowLODLevelModel.SetMesh(&m_lowLODMesh);

	m_vertexHighLODMesh.UpdateMesh(vertexHighLODData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	//m_pointsHighLODLevelModel.SetMesh(&m_vertexHighLODMesh);

	m_vertexLowLODMesh.UpdateMesh(vertexLowLODData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	//m_pointsLowLODLevelModel.SetMesh(&m_vertexLowLODMesh);
}

void Level::GenerateRenderBspData(const BSP& bsp)
{
	static Mesh bspMesh;
	std::vector<float> bspData;

	GuiRenderSettings::bspTreeMaxDepth = 0;
	GeomBoundingRect(&bsp, 0, bspData);

	bspMesh.UpdateMesh(bspData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	m_bspModel.SetMesh(&bspMesh);
}

void Level::GenerateRenderCheckpointData(std::vector<Checkpoint>& checkpoints)
{
	static Mesh checkMesh;
	std::vector<float> checkData;

	for (Checkpoint& e : checkpoints)
	{
		Vertex v = Vertex(Point(e.GetPos().x, e.GetPos().y, e.GetPos().z, 255, 0, 128));
		GeomOctopoint(&v, 0, checkData);
	}

	checkMesh.UpdateMesh(checkData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
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

	spawnsMesh.UpdateMesh(spawnsData, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals), Mesh::ShaderSettings::None);
	m_spawnsModel.SetMesh(&spawnsMesh);
}

void Level::GenerateRenderSelectedBlockData(const Quadblock& quadblock, const Vec3& queryPoint)
{
	static Mesh quadblockMesh;
	std::vector<float> data;
	const Vertex* verts = quadblock.GetUnswizzledVertices();
	bool isQuadblock = quadblock.IsQuadblock();
	Vertex recoloredVerts[9];
	for (int i = 0; i < (isQuadblock ? 9 : 6); i++)
	{
		Color negated = verts[i].GetColor(true).Negated();
		recoloredVerts[i] = Point(0, 0, 0, negated.r, negated.g, negated.b); //pos reassigned in next line
		recoloredVerts[i].m_pos = verts[i].m_pos;
		recoloredVerts[i].m_normal = verts[i].m_normal;
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

	Vertex v = Vertex(Point(queryPoint.x, queryPoint.y, queryPoint.z, 255, 0, 0));
	GeomOctopoint(&v, 0, data);

	quadblockMesh.UpdateMesh(data, (Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals), (Mesh::ShaderSettings::DrawWireframe | Mesh::ShaderSettings::DrawBackfaces | Mesh::ShaderSettings::ForceDrawOnTop | Mesh::ShaderSettings::DrawLinesAA | Mesh::ShaderSettings::DontOverrideShaderSettings | Mesh::ShaderSettings::Blinky));
	m_selectedBlockModel.SetMesh(&quadblockMesh);
}

void Level::ViewportClickHandleBlockSelection(int pixelX, int pixelY, const Renderer& rend)
{
	std::function<std::optional<std::tuple<const Quadblock, const glm::vec3>>(int, int, std::vector<Quadblock>&, unsigned)> check = [&rend](int pixelCoordX, int pixelCoordY, std::vector<Quadblock>& qbs, unsigned index)
		{
			std::vector<std::tuple<Quadblock, glm::vec3, float>> passed;

			//we're currently checking ALL quadblocks on a click (bad), so we should
			//use an acceleration structure (good thing we can have a BSP :) )
			//although it may be better to use a different acceleration structure.
			//I don't care for right now, clicking causes a little lag spike. TODO.
			//another option to speed this up is to do collision testing for the low LOD instead.
			for (Quadblock& qb : qbs)
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
				//    tri[0] = glm::vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.z);
				//    tri[1] = glm::vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.z);
				//    tri[2] = glm::vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.z);
				//  }
				//  else
				//  {
				//    tri[0] = glm::vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.z);
				//    tri[1] = glm::vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.z);
				//    tri[2] = glm::vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.z);
				//  }

				//  queryResult = rend.WorldspaceRayTriIntersection(worldSpaceRay, tri); //if we have multiple collisions in one block, just pick one idc
				//  collided |= (std::get<1>(queryResult) != -1.0f);

				//  if (collided) { break; }
				//}


				if (collided) { passed.push_back(std::make_tuple(qb, std::get<0>(queryResult), std::get<1>(queryResult))); continue; }
			}

			//sort collided blocks by time value (distance from camera).
			std::sort(passed.begin(), passed.end(),
				[](const std::tuple<Quadblock, glm::vec3, float>& a, const std::tuple<Quadblock, glm::vec3, float>& b) {
					return std::get<2>(a) < std::get<2>(b);
				});

			std::optional<std::tuple<Quadblock, glm::vec3>> result;
			//at the very end
			if (passed.size() > 0)
			{
				auto tuple = passed[index % passed.size()];
				result = std::make_optional(std::make_tuple(std::get<0>(tuple), std::get<1>(tuple)));
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

	if (pixelX == lastClickedX && pixelY == lastClickedY)
	{
		indenticalClickTimes++;
	}
	else
	{
		lastClickedX = pixelX;
		lastClickedY = pixelY;
		indenticalClickTimes = 0;
	}

	std::optional<std::tuple<const Quadblock, const glm::vec3>> collidedQB = check(pixelX, pixelY, m_quadblocks, indenticalClickTimes);

	if (collidedQB.has_value())
	{
		glm::vec3 p = std::get<1>(collidedQB.value());
		Vec3 point = Vec3(p.x, p.y, p.z);
		GenerateRenderSelectedBlockData(std::get<0>(collidedQB.value()), point);
	}
	else
	{
		m_selectedBlockModel.SetMesh();
	}
}