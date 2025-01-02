#pragma once

#include "geo.h"
#include "vertex.h"

#include <string>
#include <unordered_map>
#include <cstdint>

static constexpr size_t NUM_VERTICES_QUADBLOCK = 9;
static constexpr size_t NUM_FACES_QUADBLOCK = 4;

namespace QuadFlags
{
	static constexpr uint16_t NONE = 0;
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
	static const std::unordered_map<std::string, uint16_t> LABELS = {
		{"None", NONE}, {"Invisible", INVISIBLE}, {"Moon Gravity", MOON_GRAVITY}, {"Reflection", REFLECTION},
		{"Kickers (?)", KICKERS}, {"Out of Bounds", OUT_OF_BOUNDS}, {"Never Used (?)", NEVER_USED},
		{"Trigger Script", TRIGGER_SCRIPT }, {"Reverb", REVERB}, {"Kickers Two (?)", KICKERS_TWO},
		{"Mask Grab", MASK_GRAB }, {"Tiger Temple Door", TIGER_TEMPLE_DOOR}, {"Collision Trigger", COLLISION_TRIGGER},
		{"Ground", GROUND}, {"Wall", WALL}, {"No Colision", NO_COLLISION}, {"Invisible Trigger", INVISIBLE_TRIGGER}
	};
};

namespace TerrainType
{
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
	static const std::string DEFAULT = "Asphalt";
	static const std::unordered_map<std::string, uint8_t> LABELS = {
		{"Asphalt", ASPHALT}, {"Dirt", DIRT}, {"Grass", GRASS}, {"Wood", WOOD}, {"Water", WATER}, {"Stone", STONE},
		{"Ice", ICE}, {"Track (?)", TRACK}, {"Icy Road", ICY_ROAD}, {"Snow", SNOW}, {"None (?)", NONE},
		{"Hard Pack (?)", HARD_PACK}, {"Metal", METAL}, {"Fast Water", FAST_WATER}, {"Mud", MUD}, {"Side Slip", SIDE_SLIP},
		{"River Asphalt", RIVER_ASPHALT}, {"Steam Asphalt", STEAM_ASPHALT}, {"Ocean Asphalt", OCEAN_ASPHALT},
		{"Slow Grass", SLOW_GRASS}, {"Slow Dirt", SLOW_DIRT},
	};
};

namespace FaceRotateFlip
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

namespace FaceDrawMode
{
	static constexpr uint32_t DRAW_BOTH = 0;
	static constexpr uint32_t DRAW_LEFT = 1;
	static constexpr uint32_t DRAW_RIGHT = 2;
	static constexpr uint32_t DRAW_NONE = 3;
};

class Quadblock
{
public:
	Quadblock(const std::string& name, Tri& t0, Tri& t1, Tri& t2, Tri& t3, const Vec3& normal, const std::string& material);
	Quadblock(const std::string& name, Quad& q0, Quad& q1, Quad& q2, Quad& q3, const Vec3& normal, const std::string& material);
	const std::string& Name() const;
	const Vec3& Center() const;
	uint8_t Terrain() const;
	uint16_t Flags() const;
	void SetTerrain(uint8_t terrain);
	void SetFlag(uint16_t flag);
	void SetCheckpoint(int index);
	void SetDrawDoubleSided(bool active);
	const BoundingBox& GetBoundingBox() const;
	std::vector<Vertex> GetVertices() const;
	float DistanceClosestVertex(Vec3& out, const Vec3& v) const;
	bool Neighbours(const Quadblock& quadblock, float threshold = 0.1f) const;
	std::vector<uint8_t> Serialize(size_t id, size_t offTextures, size_t offVisibleSet, const std::vector<size_t>& vertexIndexes) const;
	void RenderUI(size_t checkpointCount);

private:
	Vec3 ComputeNormalVector(size_t id0, size_t id1, size_t id2) const;
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
};