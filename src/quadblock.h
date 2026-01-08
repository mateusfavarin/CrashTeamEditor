#pragma once

#include "geo.h"
#include "vertex.h"
#include "psx_types.h"

#include <string>
#include <unordered_map>
#include <cstdint>
#include <array>
#include <filesystem>
#include <limits>
#include <functional>

static constexpr size_t NUM_FACES_QUADBLOCK = 4;
static constexpr size_t TURBO_PAD_INDEX_NONE = 0;
static constexpr float TURBO_PAD_QUADBLOCK_TRANSLATION = 0.5f;
static constexpr size_t RENDER_INDEX_NONE = std::numeric_limits<size_t>::max();

struct QuadFlags
{
	static constexpr uint16_t INVISIBLE = 1 << 0;
	static constexpr uint16_t MOON_GRAVITY = 1 << 1;
	static constexpr uint16_t REFLECTION = 1 << 2;
	static constexpr uint16_t KICKERS = 1 << 3;
	static constexpr uint16_t OUT_OF_BOUNDS = 1 << 4;
	static constexpr uint16_t NEVER_USED = 1 << 5;
	static constexpr uint16_t TRIGGER_SCRIPT = 1 << 6;
	static constexpr uint16_t REVERB = 1 << 7;
	static constexpr uint16_t KICKERS_TWO = 1 << 8;
	static constexpr uint16_t MASK_GRAB = 1 << 9;
	static constexpr uint16_t TIGER_TEMPLE_DOOR = 1 << 10;
	static constexpr uint16_t COLLISION_TRIGGER = 1 << 11;
	static constexpr uint16_t GROUND = 1 << 12;
	static constexpr uint16_t WALL = 1 << 13;
	static constexpr uint16_t NO_COLLISION = 1 << 14;
	static constexpr uint16_t INVISIBLE_TRIGGER = 1 << 15;
	static constexpr uint16_t DEFAULT = GROUND | COLLISION_TRIGGER;
	static inline const std::unordered_map<std::string, uint16_t> LABELS = {
		{"Invisible", INVISIBLE}, {"Moon Gravity", MOON_GRAVITY}, {"Reflection", REFLECTION},
		{"Kickers (?)", KICKERS}, {"Out of Bounds", OUT_OF_BOUNDS}, {"Never Used (?)", NEVER_USED},
		{"Trigger Script", TRIGGER_SCRIPT }, {"Reverb", REVERB}, {"Kickers Two (?)", KICKERS_TWO},
		{"Mask Grab", MASK_GRAB }, {"Tiger Temple Door", TIGER_TEMPLE_DOOR}, {"Collision Trigger", COLLISION_TRIGGER},
		{"Ground", GROUND}, {"Wall", WALL}, {"No Colision", NO_COLLISION}, {"Invisible Trigger", INVISIBLE_TRIGGER}
	};
};

struct TerrainType
{
	static constexpr uint8_t TURBO_PAD = 1;
	static constexpr uint8_t SUPER_TURBO_PAD = 2;
	static constexpr uint8_t ASPHALT = 0;
	static constexpr uint8_t DIRT = 1;
	static constexpr uint8_t GRASS = 2;
	static constexpr uint8_t WOOD = 3;
	static constexpr uint8_t WATER = 4;
	static constexpr uint8_t STONE = 5;
	static constexpr uint8_t ICE = 6;
	static constexpr uint8_t TRACK = 7;
	static constexpr uint8_t ICY_ROAD = 8;
	static constexpr uint8_t SNOW = 9;
	static constexpr uint8_t NONE = 10;
	static constexpr uint8_t HARD_PACK = 11;
	static constexpr uint8_t METAL = 12;
	static constexpr uint8_t FAST_WATER = 13;
	static constexpr uint8_t MUD = 14;
	static constexpr uint8_t SIDE_SLIP = 15;
	static constexpr uint8_t RIVER_ASPHALT = 16;
	static constexpr uint8_t STEAM_ASPHALT = 17;
	static constexpr uint8_t OCEAN_ASPHALT = 18;
	static constexpr uint8_t SLOW_GRASS = 19;
	static constexpr uint8_t SLOW_DIRT = 20;
	static inline const std::string DEFAULT = "Asphalt";
	static inline const std::unordered_map<std::string, uint8_t> LABELS = {
		{"Asphalt", ASPHALT}, {"Dirt", DIRT}, {"Grass", GRASS}, {"Wood", WOOD}, {"Water", WATER}, {"Stone", STONE},
		{"Ice", ICE}, {"Track (?)", TRACK}, {"Icy Road", ICY_ROAD}, {"Snow", SNOW}, {"None (?)", NONE},
		{"Hard Pack (?)", HARD_PACK}, {"Metal", METAL}, {"Fast Water", FAST_WATER}, {"Mud", MUD}, {"Side Slip", SIDE_SLIP},
		{"River Asphalt", RIVER_ASPHALT}, {"Steam Asphalt", STEAM_ASPHALT}, {"Ocean Asphalt", OCEAN_ASPHALT},
		{"Slow Grass", SLOW_GRASS}, {"Slow Dirt", SLOW_DIRT},
	};
};

struct FaceRotateFlip
{
	static constexpr uint32_t NONE = 0;
	static constexpr uint32_t ROTATE_90 = 1;
	static constexpr uint32_t ROTATE_180 = 2;
	static constexpr uint32_t ROTATE_270 = 3;
	static constexpr uint32_t FLIP_ROTATE_270 = 4;
	static constexpr uint32_t FLIP_ROTATE_180 = 5;
	static constexpr uint32_t FLIP_ROTATE_90 = 6;
	static constexpr uint32_t FLIP = 7;
};

struct FaceDrawMode
{
	static constexpr uint32_t DRAW_BOTH = 0;
	static constexpr uint32_t DRAW_LEFT = 1;
	static constexpr uint32_t DRAW_RIGHT = 2;
	static constexpr uint32_t DRAW_NONE = 3;
};

struct FaceIndexConstants
{
	static constexpr int quadHLODVertArrangements[][3] =
	{
		{ 3, 0, 1 },
		{ 1, 4, 3 },
		{ 4, 1, 2 },
		{ 2, 5, 4 },
		{ 6, 3, 4 },
		{ 4, 7, 6 },
		{ 7, 4, 5 },
		{ 5, 8, 7 },
	};

	static constexpr int quadLLODVertArrangements[][3] =
	{
		{ 6, 0, 2 },
		{ 2, 8, 6 },
	};

	static constexpr int triHLODVertArrangements[][3] =
	{
		{ 3, 0, 1 },
		{ 1, 4, 3 },
		{ 4, 1, 2 },
		{ 6, 3, 4 },
	};

	static constexpr int triLLODVertArrangements[][3] =
	{
		{ 6, 0, 2 },
	};
};

enum class QuadblockTrigger
{
	NONE, TURBO_PAD, SUPER_TURBO_PAD
};

class Quadblock;
typedef std::function<void(const Quadblock&)> UpdateFilterCallback;

class Quadblock
{
public:
	Quadblock(const std::string& name, Tri& t0, Tri& t1, Tri& t2, Tri& t3, const Vec3& normal, const std::string& material, bool hasUV, UpdateFilterCallback filterCallback);
	Quadblock(const std::string& name, Quad& q0, Quad& q1, Quad& q2, Quad& q3, const Vec3& normal, const std::string& material, bool hasUV, UpdateFilterCallback filterCallback);
	Quadblock(const PSX::Quadblock& quadblock, const std::vector<PSX::Vertex>& vertices, UpdateFilterCallback filterCallback);
	const std::string& GetName() const;
	const Vec3& GetCenter() const;
	Vec3 GetNormal() const;
	uint8_t GetTerrain() const;
	uint16_t GetFlags() const;
	QuadblockTrigger GetTrigger() const;
	size_t GetTurboPadIndex() const;
	size_t GetBSPID() const;
	void SetBSPID(size_t id) const;
	bool GetHide() const;
	bool GetAnimated() const;
	bool GetFilter() const;
	const Color& GetFilterColor() const;
	bool GetCheckpointStatus() const;
	bool GetCheckpointPathable() const;
	const QuadUV& GetQuadUV(size_t quad) const;
	const std::filesystem::path& GetTexPath() const;
	const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& GetUVs() const;
	size_t GetRenderHighLodPointIndex() const;
	size_t GetRenderLowLodPointIndex() const;
	size_t GetRenderHighLodUVIndex() const;
	size_t GetRenderLowLodUVIndex() const;
	size_t GetRenderHighLodOctoPointIndex() const;
	size_t GetRenderLowLodOctoPointIndex() const;
	size_t GetRenderFilterHighLodEdgeIndex() const;
	size_t GetRenderFilterLowLodEdgeIndex() const;
	void SetRenderIndices(size_t highPointIndex, size_t lowPointIndex, size_t highUvIndex, size_t lowUvIndex, size_t highOctoIndex, size_t lowOctoIndex);
	void SetRenderFilterIndices(size_t highEdgeIndex, size_t lowEdgeIndex);
	void SetTerrain(uint8_t terrain);
	void SetFlag(uint16_t flag);
	void SetCheckpoint(int index);
	int GetCheckpoint() const;
	void SetDrawDoubleSided(bool active);
	void SetCheckpointStatus(bool active);
	void SetCheckpointPathable(bool pathable);
	void SetName(const std::string& name);
	void SetTurboPadIndex(size_t index);
	void SetHide(bool active);
	void SetTextureID(size_t id, size_t quad);
	void SetAnimTextureOffset(size_t relOffset, size_t levOffset, size_t quad);
	bool IsQuadblock() const;
	void SetTrigger(QuadblockTrigger trigger);
	void SetTexPath(const std::filesystem::path& path);
	void SetAnimated(bool animated);
	void SetFilter(bool filter);
	void SetFilterColor(const Color& color);
	void SetSpeedImpact(int speed);
	void Translate(float ratio, const Vec3& direction);
	const BoundingBox& GetBoundingBox() const;
	std::vector<Vertex> GetVertices() const;
	const Vertex* const GetUnswizzledVertices() const;
	float DistanceClosestVertex(Vec3& out, const Vec3& v) const;
	bool Neighbours(const Quadblock& quadblock, float threshold = 0.1f) const;
	std::vector<uint8_t> Serialize(size_t id, size_t offTextures, const std::vector<size_t>& vertexIndexes) const;
	bool RenderUI(size_t checkpointCount, bool& resetBsp);
	Vec3 ComputeNormalVector(size_t id0, size_t id1, size_t id2) const;

private:
	void SetDefaultValues();
	void ComputeBoundingBox();

private:
	/*
		p0 -- p1 -- p2
		|  q0 |  q1 |
		p3 -- p4 -- p5
		|  q2 |  q3 |
		p6 -- p7 -- p8
	*/
	bool m_triblock;
	bool m_animated;
	bool m_filter;
	bool m_checkpointPathable;
	bool m_checkpointStatus;
	bool m_hide;
	Vertex m_p[NUM_VERTICES_QUADBLOCK];
	BoundingBox m_bbox;
	std::string m_name;
	std::string m_material;
	int m_checkpointIndex;
	uint32_t m_faceDrawMode[NUM_FACES_QUADBLOCK];
	uint32_t m_faceRotateFlip[NUM_FACES_QUADBLOCK];
	bool m_doubleSided;
	uint16_t m_flags;
	uint8_t m_terrain;
	QuadblockTrigger m_trigger;
	int m_downforce;
	size_t m_turboPadIndex;
	Color m_filterColor;
	mutable size_t m_bspID;
	std::array<QuadUV, NUM_FACES_QUADBLOCK + 1> m_uvs; /* Last id is reserved for low tex */
	std::array<size_t, NUM_FACES_QUADBLOCK + 1> m_textureIDs = { 0, 0, 0, 0, 0 };
	std::array<size_t, NUM_FACES_QUADBLOCK + 1> m_animTexOffset = {0, 0, 0, 0, 0};
	std::filesystem::path m_texPath;
	size_t m_renderHighLodPointIndex = RENDER_INDEX_NONE;
	size_t m_renderLowLodPointIndex = RENDER_INDEX_NONE;
	size_t m_renderHighLodUVIndex = RENDER_INDEX_NONE;
	size_t m_renderLowLodUVIndex = RENDER_INDEX_NONE;
	size_t m_renderHighLodOctoPointIndex = RENDER_INDEX_NONE;
	size_t m_renderLowLodOctoPointIndex = RENDER_INDEX_NONE;
	size_t m_renderFilterHighLodEdgeIndex = RENDER_INDEX_NONE;
	size_t m_renderFilterLowLodEdgeIndex = RENDER_INDEX_NONE;
	UpdateFilterCallback m_filterCallback;
};

class QuadException : public std::exception
{
public:
  explicit QuadException(const std::string& message) : m_message(message) {};
  const char* what() const throw() { return m_message.c_str(); }
private:
  std::string m_message;
};
