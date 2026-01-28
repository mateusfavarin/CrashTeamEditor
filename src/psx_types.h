#pragma once

#include "lev.h"

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

	struct VRMHeader
	{
		uint32_t size;
		uint32_t magic;
		uint32_t flags;
		uint32_t len;
		uint16_t x;
		uint16_t y;
		uint16_t width;
		uint16_t height;
	};

	struct BlendMode
	{
		static constexpr uint16_t HALF_TRANSPARENT = 0;
		static constexpr uint16_t ADDITIVE = 1;
		static constexpr uint16_t SUBTRACTIVE = 2;
		static constexpr uint16_t ADDITIVE_TRANSLUCENT = 3;
	};

	union Texpage
	{
		struct
		{
			uint16_t x : 4; /* x * 64 */
			uint16_t y : 1; /* y * 256 */
			uint16_t blendMode : 2; /* (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4) */
			uint16_t texpageColors : 2; /* (0=4bit, 1=8bit, 2=15bit, 3=Reserved) */
			uint16_t unused : 2;
			uint16_t y_VRAM_EXP : 1; /* ununsed in retail */
			uint16_t unused2 : 2;
			uint16_t nop : 2;
		};
		uint16_t self;
	};

	union CLUT
	{
		struct
		{
			uint16_t x : 6; /* X/16  (ie. in 16-halfword steps) */
			uint16_t y : 9; /* 0-511 (ie. in 1-line steps) */
			uint16_t nop : 1; /* Should be 0 */
		};
		uint16_t self;
	};

	struct TextureLayout
	{
		uint8_t u0;
		uint8_t v0;
		CLUT clut;
		uint8_t u1;
		uint8_t v1;
		Texpage texPage;
		uint8_t u2;
		uint8_t v2;
		uint8_t u3;
		uint8_t v3;

		inline bool operator==(const TextureLayout& layout) const
		{
			return u0 == layout.u0 && v0 == layout.v0 &&
						 u1 == layout.u1 && v1 == layout.v1 &&
						 u2 == layout.u2 && v2 == layout.v2 &&
						 u3 == layout.u3 && v3 == layout.v3 &&
						 clut.self == layout.clut.self &&
						 texPage.self == layout.texPage.self;
		}
	};

	struct TextureGroup
	{
		TextureLayout far;
		TextureLayout middle;
		TextureLayout near;
		TextureLayout mosaic;
	};

	struct AnimTex
	{
		uint32_t offActiveFrame;
		uint16_t frameCount;
		int16_t startAtFrame;
		int16_t frameDuration;
		uint16_t frameIndex;
	};

	struct ColorGradient
	{
		int16_t posFrom;
		int16_t posTo;
		PSX::Color colorFrom;
		PSX::Color colorTo;
	};

	struct Stars
	{
		uint16_t numStars;
		uint16_t spread;
		uint16_t seed;
		uint16_t zDepth;
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
		uint32_t offIconsLookup; // 0x3C
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
		uint32_t offExtra; // 0x134
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
		Stars stars; // 0x17C
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

	/* TODO: Figure out what each value actually is for and properly name them */
	enum LevelExtra
	{
		MINIMAP = 0,
		SPAWN = 1, // what
		CAMERA_END_OF_RACE = 2,
		CAMERA_DEMO = 3,
		N_TROPY_GHOST = 4,
		N_OXIDE_GHOST = 5,
		CREDITS = 6, // what
		COUNT = 7,
	};

	struct LevelExtraHeader
	{
		uint32_t count;
		uint32_t offsets[LevelExtra::COUNT];
	};

	// Minimap struct - stored within SpawnType1[ST1_MAP]
	struct Map
	{
		int16_t worldEndX;      // 0x0 - World coordinate bound
		int16_t worldEndY;      // 0x2
		int16_t worldStartX;    // 0x4
		int16_t worldStartY;    // 0x6
		int16_t iconSizeX;      // 0x8 - Size in pixels of minimap icon (width)
		int16_t iconSizeY;      // 0xA - Size in pixels of minimap icon (height)
		int16_t driverDotStartX; // 0xC - Screen position for driver markers (512x252 screen)
		int16_t driverDotStartY; // 0xE
		int16_t mode;           // 0x10 - Orientation mode (0=0°, 1=90°, 2=180°, 3=270°)
		int16_t unk;            // 0x12 - Needed for some levels like Crash Cove (value different from 0 stops drawing top part)
	};

	// Icon struct for minimap textures
	struct Icon
	{
		char name[16];                    // 0x0 - Icon name
		int32_t globalIconArrayIndex;     // 0x10 - Index in global icon array (3=top, 4=bottom)
		TextureLayout texLayout;          // 0x14 - UV and texture page info
	};

	// LevTexLookup - Icon pack header, pointed to by Level::levTexLookup
	struct LevTexLookup
	{
		int32_t numIcon;                  // 0x0 - Number of icons (2 for minimap: top and bottom)
		uint32_t offFirstIcon;            // 0x4 - Pointer to first Icon struct
		int32_t numIconGroup;             // 0x8 - Number of icon groups
		uint32_t offFirstIconGroupPtr;    // 0xC - Pointer to IconGroup pointer array
	};

	// SpawnType1 header - contains count and array of pointers
	struct SpawnType1
	{
		uint32_t count;                   // 0x0 - Number of pointers in the array
		// uint32_t offsets[count];       // Variable-length array of pointers follows
	};

	// Global icon array indices for minimap
	static constexpr int32_t ICON_INDEX_MAP_TOP = 3;
	static constexpr int32_t ICON_INDEX_MAP_BOTTOM = 4;

	// Screen size
	static constexpr int32_t SCREEN_WIDTH = 512;
	static constexpr int32_t SCREEN_HEIGHT = 252;

	struct Vertex
	{
		PSX::Vec3 pos; // 0x0
		uint16_t flags; // 0x6
		PSX::Color colorHi; // 0x8
		PSX::Color colorLo; // 0xC
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
		int16_t triNormalVecDividend[10]; // 0x48
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
		uint16_t flag;
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
		uint16_t flag; // 0x0
		uint16_t id; // 0x2
		PSX::BoundingBox bbox; // 0x4
		uint32_t unk1; // 0x10
		uint32_t offHitbox; // 0x14
		uint32_t numQuads; // 0x18
		uint32_t offQuads; // 0x1C
	};

	struct VisibleSet
	{
		uint32_t offVisibleBSPNodes;
		uint32_t offVisibleQuadblocks;
		uint32_t offVisibleInstances;
		uint32_t offVisibleExtra;

		inline bool operator==(const VisibleSet& v) const
		{
			return offVisibleBSPNodes == v.offVisibleBSPNodes &&
						 offVisibleQuadblocks == v.offVisibleQuadblocks &&
						 offVisibleInstances == v.offVisibleInstances &&
						 offVisibleExtra == v.offVisibleExtra;
		}
	};

	struct VisualMem
	{
		uint32_t offNodes[MAX_NUM_PLAYERS]; // 0x0
		uint32_t offQuads[MAX_NUM_PLAYERS]; // 0x4
		uint32_t offOcean[MAX_NUM_PLAYERS]; // 0x8
		uint32_t offScenery[MAX_NUM_PLAYERS]; // 0xC
		uint32_t offNodesSrc[MAX_NUM_PLAYERS]; // 0x10
		uint32_t offQuadsSrc[MAX_NUM_PLAYERS]; // 0x14
		uint32_t offOceanSrc[MAX_NUM_PLAYERS]; // 0x18
		uint32_t offScenerySrc[MAX_NUM_PLAYERS]; // 0x1C
		uint32_t offBSP[MAX_NUM_PLAYERS]; // 0x20
	};

	struct NavHeader
	{
		uint16_t magic;
		uint16_t numPoints;
		uint32_t posY;
		uint32_t offLastPoint;
		uint16_t physUnk[0x20];
	};
}

template<>
struct std::hash<PSX::TextureLayout>
{
	inline std::size_t operator()(const PSX::TextureLayout& key) const
	{
		uint32_t pos0 = key.u0 | (key.v0 << 8) | (key.u1 << 16) | (key.v1 << 24);
		uint32_t pos1 = key.u2 | (key.v2 << 8) | (key.u3 << 16) | (key.v3 << 24);
		uint32_t extra = key.clut.self | (key.texPage.self << 16);
		return ((((std::hash<uint32_t>()(pos0) ^ (std::hash<uint32_t>()(pos1) << 1)) >> 1) ^ (std::hash<uint32_t>()(extra) << 1)) >> 2);
	}
};

template<>
struct std::hash<PSX::VisibleSet>
{
	inline std::size_t operator()(const PSX::VisibleSet& key) const
	{
		return ((((std::hash<uint32_t>()(key.offVisibleBSPNodes) ^ (std::hash<uint32_t>()(key.offVisibleQuadblocks) << 1)) >> 1) ^ (std::hash<uint32_t>()(key.offVisibleInstances) << 1)) >> 2) ^ (std::hash<uint32_t>()(key.offVisibleExtra) << 2);
	}
};

static constexpr int16_t FP_ONE = 0x1000;
static constexpr int16_t FP_ONE_GEO = 64;
static constexpr int16_t FP_ONE_CP = 8;

static inline int16_t ConvertFloat(float x, int16_t one = FP_ONE) { return static_cast<int16_t>(x * static_cast<float>(one)); };
static inline int16_t ConvertAngle(float x) { return static_cast<int16_t>((x * static_cast<float>(FP_ONE)) / 360.0f); }
static inline float ConvertFP(int16_t fp, int16_t one = FP_ONE) { return static_cast<float>(fp) / static_cast<float>(one); }
static inline float ConvertFPAngle(int16_t fp) { return (static_cast<float>(fp) * 360.0f) / static_cast<float>(FP_ONE); }

static inline PSX::Vec3 ConvertAngle(const Vec3& v)
{
	PSX::Vec3 out = {};
	out.x = ConvertAngle(v.x);
	out.y = ConvertAngle(v.y);
	out.z = ConvertAngle(v.z);
	return out;
}

static inline Vec3 ConvertPSXAngle(const PSX::Vec3& v)
{
	Vec3 out = {};
	out.x = ConvertFPAngle(v.x);
	out.y = ConvertFPAngle(v.y);
	out.z = ConvertFPAngle(v.z);
	return out;
}

static inline PSX::Vec3 ConvertVec3(const Vec3& v, int16_t one = FP_ONE)
{
	PSX::Vec3 out = {};
	out.x = ConvertFloat(v.x, one);
	out.y = ConvertFloat(v.y, one);
	out.z = ConvertFloat(v.z, one);
	return out;
}

static inline Vec3 ConvertPSXVec3(const PSX::Vec3& v, int16_t one = FP_ONE)
{
	Vec3 out = {};
	out.x = ConvertFP(v.x, one);
	out.y = ConvertFP(v.y, one);
	out.z = ConvertFP(v.z, one);
	return out;
}

static inline Color ConvertColor(const PSX::Color& c)
{
	Color out = {};
	out.r = c.r;
	out.g = c.g;
	out.b = c.b;
	out.a = c.a == 1;
	return out;
}

static inline PSX::Color ConvertColor(const Color& c)
{
	PSX::Color out = {};
	out.r = c.r;
	out.g = c.g;
	out.b = c.b;
	out.a = c.a ? 1 : 0;
	return out;
}

static inline PSX::Stars ConvertStars(const Stars& stars)
{
    PSX::Stars out = {};
    out.numStars = stars.numStars;
    out.spread = stars.spread ? 1u : 0u;
    out.seed = stars.seed;
    out.zDepth = stars.zDepth;
    return out;
}

static inline Stars ConvertStars(const PSX::Stars& stars)
{
    Stars out = {};
    out.numStars = stars.numStars;
    out.spread = stars.spread == 1;
    out.seed = stars.seed;
    out.zDepth = stars.zDepth;
    return out;
}