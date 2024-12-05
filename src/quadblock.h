#pragma once

#include "math.h"
#include "vertex.h"

#include <string>
#include <cstdint>

static constexpr size_t NUM_VERTICES_QUADBLOCK = 9;

enum class QuadFlags : uint16_t
{
	NONE = 0,
	INVISIBLE = 1 << 0,
	MOON_GRAVITY = 1 << 1,
	REFLECTION = 1 << 2,
	KICKERS = 1 << 3,
	OUT_OF_BOUNDS = 1 << 4,
	TRIGGER = 1 << 6,
	REVERB = 1 << 7,
	MASK_GRAB = 1 << 9,
	TRIGGER_COLLISION = 1 << 11,
	GROUND = 1 << 12,
	WALL = 1 << 13,
	NO_COLLISION = 1 << 14,
};

inline QuadFlags& operator|=(QuadFlags& a, const QuadFlags& b)
{
	a = static_cast<QuadFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	return a;
}

inline QuadFlags& operator&=(QuadFlags& a, const QuadFlags& b)
{
	a = static_cast<QuadFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	return a;
}

inline QuadFlags operator~(const QuadFlags& a)
{
	return static_cast<QuadFlags>(~static_cast<uint32_t>(a));
}

inline QuadFlags operator|(const QuadFlags& a, const QuadFlags& b)
{
	return static_cast<QuadFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline QuadFlags operator&(const QuadFlags& a, const QuadFlags& b)
{
	return static_cast<QuadFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

enum class TerrainType : uint8_t
{
	ASPHALT = 0,
	DIRT = 1,
	GRASS = 2,
	WOOD = 3,
	WATER = 4,
	STONE = 5,
	ICE = 6,
	TRACK = 7,
	ICY_ROAD = 8,
	SNOW = 9,
	NONE = 10,
	HARD_PACK = 11,
	METAL = 12,
	FAST_WATER = 13,
	MUD = 14,
	SIDE_SLIP = 15,
	RIVER_ASPHALT = 16,
	STEAM_ASPHALT = 17,
	OCEAN_ASPHALT = 18,
	SLOW_GRASS = 19,
	SLOW_DIRT = 20,
};

class Quadblock
{
public:
	Quadblock(const std::string& name, Quad& q0, Quad& q1, Quad& q2, Quad& q3, const Vec3& normal);
	const std::string& Name() const;
	const Vec3& Center() const;
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
	int m_checkpointIndex;
	QuadFlags m_flags;
	TerrainType m_terrain;
};