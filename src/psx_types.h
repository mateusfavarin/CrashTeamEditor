#pragma once

#include "math.h"
#include "lev.h"
#include "vertex.h"
#include "quadblock.h"
#include "bsp.h"

#include <cstdint>
#include <cmath>

namespace PSX
{
	static constexpr size_t MAX_NUM_PLAYERS = 4;

	struct Vec3
	{
		int16_t x;
		int16_t y;
		int16_t z;
	};

	struct Color
	{
		uint32_t r : 8;
		uint32_t g : 8;
		uint32_t b : 8;
		uint32_t a : 8;
	};

	struct Spawn
	{
		PSX::Vec3 pos;
		PSX::Vec3 rot;
	};

	struct BoundingBox
	{
		PSX::Vec3 min;
		PSX::Vec3 max;
	};

	struct TextureLayout
	{
		uint8_t u0;
		uint8_t v0;
		uint16_t clut;
		uint8_t u1;
		uint8_t v1;
		uint16_t texPage;
		uint8_t u2;
		uint8_t v2;
		uint8_t u3;
		uint8_t v3;
	};

	struct TextureGroup
	{
		TextureLayout far;
		TextureLayout middle;
		TextureLayout near;
		TextureLayout mosaic;
	};

	struct ColorGradient
	{
		int16_t posFrom;
		int16_t posTo;
		PSX::Color colorFrom;
		PSX::Color colorTo;
	};

	struct Weather
	{
		uint32_t numParticles; // 0x0
		uint16_t maxParticles; // 0x4
		int16_t vanishRate; // 0x6
		uint8_t unk_0x8[0x10]; // 0x8
		PSX::Vec3 camPos; // 0x18
		uint16_t unk_0x1E; // 0x1E - maybe padding
		PSX::Color top; // 0x20
		PSX::Color bottom; // 0x24
		uint32_t renderMode[2]; // 0x28
	};

	struct LevHeader
	{
		uint32_t offMeshInfo; // 0x0
		uint32_t offSkybox; // 0x4
		uint32_t offAnimTex; // 0x8
		uint32_t numInstances; // 0xC
		uint32_t offInstances; // 0x10
		uint32_t numModels; // 0x14
		uint32_t offModels; // 0x18
		uint32_t offUnk_0x1C; // 0x1C
		uint32_t offUnk_0x20; // 0x20
		uint32_t offModelInstances; // 0x24
		uint32_t offUnk_0x28; // 0x28
		uint32_t null_0x2C; // 0x2C
		uint32_t null_0x30; // 0x30
		uint32_t numWaterVertices; // 0x34
		uint32_t offWaterVertices; // 0x38
		uint32_t offLevTexLookup; // 0x3C
		uint32_t offIcons; // 0x40
		uint32_t offEnvironmentMap; // 0x44
		PSX::ColorGradient skyGradient[NUM_GRADIENT]; // 0x48
		PSX::Spawn driverSpawn[NUM_DRIVERS]; // 0x6C
		uint32_t offUnk_0xCC; // 0xCC
		uint32_t offUnk_0xD0; // 0xD0
		uint32_t offLowTexArray; // 0xD4
		PSX::Color clear; // 0xD8
		uint32_t config; // 0xDC
		uint32_t offBuildStart; // 0xE0
		uint32_t offBuildEnd; // 0xE4
		uint32_t offBuildType; // 0xE8
		uint8_t unk_0xEC[0x18]; // 0xEC
		PSX::Weather weather; // 0x104
		uint32_t offSpawnType_1; // 0x134
		uint32_t numSpawnType_2; // 0x138
		uint32_t offSpawnType_2; // 0x13C
		uint32_t numSpawnType_2_posRot; // 0x140
		uint32_t offSpawnType_2_posRot; // 0x144
		uint32_t numCheckpointNodes; // 0x148
		uint32_t offCheckpointNodes; // 0x14C
		uint8_t unk_0x150[0x10]; // 0x150
		PSX::Color gradientClear[3]; // 0x160
		uint32_t unk_0x16C; // 0x16C
		uint32_t unk_0x170; // 0x170
		uint32_t numSCVertices; // 0x174
		uint32_t offSCVertices; // 0x178
		uint8_t unk_0x17C[8]; // 0x17C
		uint8_t splitLines[4]; // 0x184
		uint32_t offLevNavTable; // 0x188
		uint32_t unk_0x18C; // 0x18C
		uint32_t offVisMem; // 0x190
		uint8_t footer[0x60]; // 0x194
	};

	struct MeshInfo
	{
		uint32_t numQuadblocks;
		uint32_t numVertices;
		uint32_t unk1;
		uint32_t offQuadblocks;
		uint32_t offVertices;
		uint32_t unk2;
		uint32_t offBSPNodes;
		uint32_t numBSPNodes;
	};

	struct SpawnType
	{
		uint32_t count;
		/* TODO: void* pointers[0] */
	};

	struct Vertex
	{
		PSX::Vec3 pos;
		uint16_t flags;
		PSX::Color colorHi;
		PSX::Color colorLo;
	};

	struct Quadblock
	{
		uint16_t index[NUM_VERTICES_QUADBLOCK]; // 0x0
		uint16_t flags; // 0x12
		uint32_t drawOrderLow; // 0x14
		uint32_t drawOrderHigh; // 0x18
		uint32_t offMidTextures[4]; // 0x1C
		PSX::BoundingBox bbox; // 0x2C
		uint8_t terrain; // 0x38
		uint8_t weatherIntensity; // 0x39
		uint8_t weatherVanishRate; // 0x3A
		int8_t speedImpact; // 0x3B
		uint16_t id; // 0x3C
		uint8_t checkpointIndex; // 0x3E
		uint8_t triNormalVecBitshift; // 0x3F
		uint32_t offLowTexture; // 0x40
		uint32_t offVisibleSet; // 0x44
		uint16_t triNormalVecDividend[10]; // 0x48
	};

	struct Checkpoint
	{
		PSX::Vec3 pos;
		uint16_t distToFinish;
		uint8_t linkUp;
		uint8_t linkLeft;
		uint8_t linkDown;
		uint8_t linkRight;
	};

	struct BSPBranch
	{
		BSPFlags flag;
		uint16_t id;
		PSX::BoundingBox bbox;
		PSX::Vec3 axis;
		uint16_t unk1;
		uint16_t leftChild;
		uint16_t rightChild;
		uint16_t unk2;
		uint16_t unk3;
	};

	struct BSPLeaf
	{
		BSPFlags flag;
		uint16_t id;
		PSX::BoundingBox bbox;
		uint32_t unk1;
		uint32_t offHitbox;
		uint32_t numQuads;
		uint32_t offQuads;
	};

	struct VisibleSet
	{
		uint32_t offVisibleBSPNodes;
		uint32_t offVisibleQuadblocks;
		uint32_t offVisibleInstances;
		uint32_t offVisibleExtra;
	};

	struct VisualMem
	{
		uint32_t offNodes[MAX_NUM_PLAYERS];
		uint32_t offQuads[MAX_NUM_PLAYERS];
		uint32_t offOcean[MAX_NUM_PLAYERS];
		uint32_t offScenery[MAX_NUM_PLAYERS];
		uint32_t offNodesSrc[MAX_NUM_PLAYERS];
		uint32_t offQuadsSrc[MAX_NUM_PLAYERS];
		uint32_t offOceanSrc[MAX_NUM_PLAYERS];
		uint32_t offScenerySrc[MAX_NUM_PLAYERS];
		uint32_t offBSP[MAX_NUM_PLAYERS];
	};
}

static constexpr int16_t FP_ONE = 0x1000;
static constexpr int16_t FP_ONE_GEO = 64;
static inline int16_t ConvertFloat(float x, int16_t one = FP_ONE) { return static_cast<int16_t>(x * static_cast<float>(one)); };
static inline PSX::Vec3 ConvertVec3(Vec3 v, int16_t one = FP_ONE)
{
	PSX::Vec3 out = {};
	out.x = ConvertFloat(v.x, one);
	out.y = ConvertFloat(v.y, one);
	out.z = ConvertFloat(v.z, one);
	return out;
}
static inline PSX::Color ConvertColor(Color c)
{
	PSX::Color out = {};
	out.r = static_cast<uint8_t>(std::round(c.r * 255.0f));
	out.g = static_cast<uint8_t>(std::round(c.g * 255.0f));
	out.b = static_cast<uint8_t>(std::round(c.b * 255.0f));
	out.a = c.a ? 1 : 0;
	return out;
}