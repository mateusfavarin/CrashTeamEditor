#pragma once

#include "math.h"
#include "vertex.h"

#include <string>
#include <unordered_map>
#include <cstdint>

static constexpr size_t NUM_VERTICES_QUADBLOCK = 9;

namespace QuadFlags
{
	static constexpr uint16_t NONE = 0;
	static constexpr uint16_t INVISIBLE = 1 << 0;
	static constexpr uint16_t MOON_GRAVITY = 1 << 1;
	static constexpr uint16_t REFLECTION = 1 << 2;
	static constexpr uint16_t KICKERS = 1 << 3;
	static constexpr uint16_t OUT_OF_BOUNDS = 1 << 4;
	static constexpr uint16_t TRIGGER = 1 << 6;
	static constexpr uint16_t REVERB = 1 << 7;
	static constexpr uint16_t MASK_GRAB = 1 << 9;
	static constexpr uint16_t TRIGGER_COLLISION = 1 << 11;
	static constexpr uint16_t GROUND = 1 << 12;
	static constexpr uint16_t WALL = 1 << 13;
	static constexpr uint16_t NO_COLLISION = 1 << 14;
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
	static const std::unordered_map<std::string, uint8_t> LABELS = {
		{"Asphalt", ASPHALT}, {"Dirt", DIRT}, {"Grass", GRASS}, {"Wood", WOOD}, {"Water", WATER}, {"Stone", STONE},
		{"Ice", ICE}, {"Track (?)", TRACK}, {"Icy Road", ICY_ROAD}, {"Snow", SNOW}, {"None (?)", NONE},
		{"Hard Pack (?)", HARD_PACK}, {"Metal", METAL}, {"Fast Water", FAST_WATER}, {"Mud", MUD}, {"Side Slip", SIDE_SLIP},
		{"River Asphalt", RIVER_ASPHALT}, {"Steam Asphalt", STEAM_ASPHALT}, {"Ocean Asphalt", OCEAN_ASPHALT},
		{"Slow Grass", SLOW_GRASS}, {"Slow Dirt", SLOW_DIRT},
	};
};

class Quadblock
{
public:
	Quadblock(const std::string& name, Quad& q0, Quad& q1, Quad& q2, Quad& q3, const Vec3& normal, const std::string& material);
	const std::string& Name() const;
	const Vec3& Center() const;
	uint8_t Terrain() const;
	void SetTerrain(uint8_t terrain);
	const BoundingBox& GetBoundingBox() const;
	std::vector<Vertex> GetVertices() const;
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
	Vertex m_p[NUM_VERTICES_QUADBLOCK];
	BoundingBox m_bbox;
	std::string m_name;
	std::string m_material;
	int m_checkpointIndex;
	uint16_t m_flags;
	uint8_t m_terrain;
};