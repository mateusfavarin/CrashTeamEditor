#pragma once

#include "geo.h"
#include "psx_types.h"

#include <cstdint>

struct VertexFlags
{
	static constexpr uint16_t NONE = 0;
};

class Vertex
{
public:
	Vertex();
	Vertex(const Point& point);
	Vertex(const PSX::Vertex& vertex);
	void RenderUI(size_t index, bool& editedPos);
	std::vector<uint8_t> Serialize() const;
	Color GetColor(bool high) const;
	std::vector<Primitive> ToGeometry(bool highColor = true) const;
	inline bool operator==(const Vertex& v) const { return (m_pos == v.m_pos) && (m_flags == v.m_flags) && (m_colorHigh == v.m_colorHigh) && (m_colorLow == v.m_colorLow); };

public:
	Vec3 m_pos;
	Vec3 m_normal;

private:
	uint16_t m_flags;
	Color m_colorHigh;
	Color m_colorLow;

	friend std::hash<Vertex>;
};

template<>
struct std::hash<Vertex>
{
	inline std::size_t operator()(const Vertex& key) const
	{
		return ((((std::hash<Vec3>()(key.m_pos) ^ (std::hash<uint16_t>()(key.m_flags) << 1)) >> 1) ^ (std::hash<Color>()(key.m_colorHigh) << 1)) >> 2) ^ (std::hash<Color>()(key.m_colorLow) << 2);
	}
};
