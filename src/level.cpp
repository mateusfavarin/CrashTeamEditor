#include "level.h"
#include "psx_types.h"
#include "io.h"
#include "utils.h"
#include "geo.h"

#include <fstream>

bool Level::Load(const std::filesystem::path& filename)
{
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

bool Level::Ready()
{
	return m_bsp.Valid();
}

void Level::Clear()
{
	for (size_t i = 0; i < NUM_DRIVERS; i++) { m_spawn[i] = Spawn(); }
	for (size_t i = 0; i < NUM_GRADIENT; i++) { m_skyGradient[i] = ColorGradient(); }
	m_configFlags = LevConfigFlags::NONE;
	m_clearColor = Color();
	m_name.clear();
	m_bspStatusMessage.clear();
	m_quadblocks.clear();
	m_checkpoints.clear();
	m_bsp.Clear();
	m_materialToQuadblocks.clear();
	m_materialTerrainPreview.clear();
	m_checkpointPaths.clear();
}

bool Level::LoadLEV(const std::filesystem::path& levFile)
{
	Clear();

	std::ifstream file(levFile, std::ios::binary);
	uint32_t offPointerMap;
	Read(file, offPointerMap);
	PSX::LevHeader header;
	Read(file, header);
	file.close();
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
	*		- Array of PVS
	*		- Array of quadblocks
	*		- Array of VisibleSets
	*		- Array of vertices
	*		- Array of BSP
	*		- Array of checkpoints
	*		- SpawnType
	*		- VisMem
	*		- PointerMap
	*/
	std::filesystem::path filepath = path / (m_name + ".lev");
	std::ofstream file(filepath, std::ios::binary);

	std::vector<const BSP*> bspNodes = m_bsp.GetTree();
	std::vector<const BSP*> orderedBSPNodes(bspNodes.size());
	for (const BSP* bsp : bspNodes) { orderedBSPNodes[bsp->Id()] = bsp; }

	PSX::LevHeader header = {};
	const size_t offHeader = 0;
	size_t currOffset = sizeof(header);

	PSX::MeshInfo meshInfo = {};
	const size_t offMeshInfo = currOffset;
	currOffset += sizeof(meshInfo);

	/*
		TODO: Add texture support. For now, load default white texture
	*/
	PSX::TextureLayout tex = {};
	tex.clut = 32 | (20 << 6);
	tex.texPage = (512 >> 6) | ((0 >> 8) << 4) | (0 << 5) | (0 << 7);
	tex.u0 = 0;		tex.v0 = 0;
	tex.u1 = 15;	tex.v1 = 0;
	tex.u2 = 0;		tex.v2 = 15;
	tex.u3 = 15;	tex.v3 = 15;

	/*
		TODO: Add texture support. For now, load default white texture
	*/
	PSX::TextureGroup texGroup = {};
	texGroup.far = tex;
	texGroup.middle = tex;
	texGroup.near = tex;
	texGroup.mosaic = tex;

	const size_t offTexture = currOffset;
	currOffset += sizeof(texGroup);

	std::vector<PSX::VisibleSet> visibleSets(m_quadblocks.size());
	const size_t offVisibleSet = currOffset;
	currOffset += visibleSets.size() * sizeof(PSX::VisibleSet);

	const size_t offQuadblocks = currOffset;
	std::vector<std::vector<uint8_t>> serializedBSPs;
	std::vector<std::vector<uint8_t>> serializedQuads;
	std::vector<const Quadblock*> orderedQuads;
	std::unordered_map<Vertex, size_t> vertexMap;
	std::unordered_map<size_t, size_t> quadIndexMap;
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
			serializedQuads.push_back(quadblock.Serialize(quadIndex, offTexture, offVisibleSet + sizeof(PSX::VisibleSet) * quadIndex, verticesIndexes));
			orderedQuads.push_back(&quadblock);
			currOffset += serializedQuads.back().size();
			quadIndexMap[index] = quadIndex;
		}
	}

	std::vector<std::vector<uint32_t>> visibleNodesPerQuadblock;
	std::vector<std::vector<uint32_t>> visibleQuadsPerQuadblock;
	size_t visibleSetsProcessed = 0;
	size_t sizeLargestVisNodes = 0;
	size_t sizeLargestVisQuads = 0;
	const size_t offVisibility = currOffset;
	for (const Quadblock* quad : orderedQuads)
	{
		/* TODO: read quadblock data */
		size_t visNodeSize = static_cast<size_t>(std::ceil(static_cast<float>(bspNodes.size()) / 32.0f));
		sizeLargestVisNodes = std::max(sizeLargestVisNodes, visNodeSize);
		std::vector<uint32_t> visibleNodes(visNodeSize, 0xFFFFFFFF);
		visibleNodesPerQuadblock.push_back(visibleNodes);
		visibleSets[visibleSetsProcessed].offVisibleBSPNodes = static_cast<uint32_t>(currOffset);
		currOffset += visibleNodes.size() * sizeof(uint32_t);

		size_t visQuadSize = static_cast<size_t>(std::ceil(static_cast<float>(m_quadblocks.size()) / 32.0f));
		sizeLargestVisQuads = std::max(sizeLargestVisQuads, visQuadSize);
		std::vector<uint32_t> visibleQuads(visQuadSize, 0xFFFFFFFF);
		visibleQuadsPerQuadblock.push_back(visibleQuads);
		visibleSets[visibleSetsProcessed].offVisibleQuadblocks = static_cast<uint32_t>(currOffset);
		currOffset += visibleQuads.size() * sizeof(uint32_t);

		visibleSets[visibleSetsProcessed].offVisibleInstances = 0;
		visibleSets[visibleSetsProcessed].offVisibleExtra = 0;
		visibleSetsProcessed++;
	}

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

	PSX::SpawnType spawnType = {};
	spawnType.count = 0;
	const size_t offSpawnType = currOffset;
	currOffset += sizeof(spawnType);

	std::vector<uint32_t> visMemNodesP1(sizeLargestVisNodes);
	const size_t offVisMemNodesP1 = currOffset;
	currOffset += visMemNodesP1.size() * sizeof(uint32_t);

	std::vector<uint32_t> visMemQuadsP1(sizeLargestVisQuads);
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
	header.offSpawnType_1 = static_cast<uint32_t>(offSpawnType);
	header.numCheckpointNodes = static_cast<uint32_t>(m_checkpoints.size());
	header.offCheckpointNodes = static_cast<uint32_t>(offCheckpoints);
	header.offVisMem = static_cast<uint32_t>(offVisMem);

#define CALCULATE_OFFSET(s, m, b) static_cast<uint32_t>(offsetof(s, m) + b)

	std::vector<uint32_t> pointerMap =
	{
		CALCULATE_OFFSET(PSX::LevHeader, offMeshInfo, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offSpawnType_1, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offCheckpointNodes, offHeader),
		CALCULATE_OFFSET(PSX::LevHeader, offVisMem, offHeader),
		CALCULATE_OFFSET(PSX::MeshInfo, offQuadblocks, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offVertices, offMeshInfo),
		CALCULATE_OFFSET(PSX::MeshInfo, offBSPNodes, offMeshInfo),
		CALCULATE_OFFSET(PSX::VisualMem, offNodes[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offQuads[0], offVisMem),
		CALCULATE_OFFSET(PSX::VisualMem, offBSP[0], offVisMem),
	};

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
		offCurrVisibleSet += sizeof(PSX::VisibleSet);
	}

	const size_t pointerMapBytes = pointerMap.size() * sizeof(uint32_t);

	Write(file, &offPointerMap, sizeof(uint32_t));
	Write(file, &header, sizeof(header));
	Write(file, &meshInfo, sizeof(meshInfo));
	Write(file, &texGroup, sizeof(texGroup));
	Write(file, visibleSets.data(), visibleSets.size() * sizeof(PSX::VisibleSet));
	for (const auto& serializedQuad : serializedQuads) { Write(file, serializedQuad.data(), serializedQuad.size()); }
	for (size_t i = 0; i < visibleNodesPerQuadblock.size(); i++)
	{
		Write(file, visibleNodesPerQuadblock[i].data(), visibleNodesPerQuadblock[i].size() * sizeof(uint32_t));
		Write(file, visibleQuadsPerQuadblock[i].data(), visibleQuadsPerQuadblock[i].size() * sizeof(uint32_t));
	}
	for (const auto& serializedVertex : serializedVertices) { Write(file, serializedVertex.data(), serializedVertex.size()); }
	for (const auto& serializedBSP : serializedBSPs) { Write(file, serializedBSP.data(), serializedBSP.size()); }
	for (const auto& serializedCheckpoint : serializedCheckpoints) { Write(file, serializedCheckpoint.data(), serializedCheckpoint.size()); }
	Write(file, &spawnType, sizeof(spawnType));
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
	Clear();

	std::string line;
	std::ifstream file(objFile);
	m_name = objFile.filename().replace_extension().string();

	std::unordered_map<std::string, std::vector<Quad>> vertexIndexMap;
	std::unordered_map<std::string, std::vector<Vec3>> normalIndexMap;
	std::unordered_map<std::string, std::string> materialMap;
	std::vector<Point> vertices;
	std::vector<Vec3> normals;
	std::string currQuadblockName;
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
		else if (command == "o")
		{
			if (tokens.size() < 2) { continue; }
			currQuadblockName = tokens[1];
		}
		else if (command == "usemtl")
		{
			if (tokens.size() < 2) { continue; }
			if (currQuadblockName.empty() || materialMap.contains(currQuadblockName)) { return false; }
			materialMap[currQuadblockName] = tokens[1];
		}
		else if (command == "f")
		{
			if (currQuadblockName.empty()) { return false; }
			if (tokens.size() < 5) { continue; }
			if (!vertexIndexMap.contains(currQuadblockName)) { vertexIndexMap[currQuadblockName] = std::vector<Quad>(); }

			std::vector<std::string> token0 = Split(tokens[1], '/');
			std::vector<std::string> token1 = Split(tokens[2], '/');
			std::vector<std::string> token2 = Split(tokens[3], '/');
			std::vector<std::string> token3 = Split(tokens[4], '/');
			int i0 = std::stoi(token0[0]) - 1;
			int i1 = std::stoi(token1[0]) - 1;
			int i2 = std::stoi(token2[0]) - 1;
			int i3 = std::stoi(token3[0]) - 1;
			int ni0 = std::stoi(token0[2]) - 1;
			int ni1 = std::stoi(token1[2]) - 1;
			int ni2 = std::stoi(token2[2]) - 1;
			int ni3 = std::stoi(token3[2]) - 1;

			vertexIndexMap[currQuadblockName].emplace_back(vertices[i0], vertices[i1], vertices[i2], vertices[i3]);
			normalIndexMap[currQuadblockName].push_back(normals[ni0]);
			normalIndexMap[currQuadblockName].push_back(normals[ni1]);
			normalIndexMap[currQuadblockName].push_back(normals[ni2]);
			normalIndexMap[currQuadblockName].push_back(normals[ni3]);
			if (vertexIndexMap[currQuadblockName].size() == 4)
			{
				Quad& q0 = vertexIndexMap[currQuadblockName][0];
				Quad& q1 = vertexIndexMap[currQuadblockName][1];
				Quad& q2 = vertexIndexMap[currQuadblockName][2];
				Quad& q3 = vertexIndexMap[currQuadblockName][3];
				Vec3 averageNormal = Vec3();
				for (const Vec3& normal : normalIndexMap[currQuadblockName])
				{
					averageNormal = averageNormal + normal;
				}
				averageNormal = averageNormal / averageNormal.Length();
				std::string material;
				if (materialMap.contains(currQuadblockName))
				{
					material = materialMap[currQuadblockName];
					m_materialToQuadblocks[material].push_back(m_quadblocks.size());
					m_materialTerrainPreview[material] = TerrainType::DEFAULT;
					m_materialTerrainBackup[material] = TerrainType::DEFAULT;
					m_materialQuadflagsPreview[material] = QuadFlags::DEFAULT;
					m_materialQuadflagsBackup[material] = QuadFlags::DEFAULT;
				}
				m_quadblocks.emplace_back(currQuadblockName, q0, q1, q2, q3, averageNormal, material);
			}
		}
	}
	file.close();
	return true;
}
