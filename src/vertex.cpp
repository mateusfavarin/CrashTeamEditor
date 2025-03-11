#include "vertex.h"
#include "psx_types.h"

#include <string>
#include <cstring>

Vertex::Vertex()
{
	m_pos = Vec3();
	m_normal = Vec3();
	m_flags = VertexFlags::NONE;
	m_colorHigh = Color(static_cast<unsigned char>(255u), 255u, 255u);
	m_colorLow = Color(static_cast<unsigned char>(255u), 255u, 255u);
}

Vertex::Vertex(const Point& point)
{
	m_pos = point.pos;
	m_normal = point.normal;
	m_flags = VertexFlags::NONE;
	m_colorHigh = point.color;
	m_colorLow = point.color;
}

std::vector<uint8_t> Vertex::Serialize() const
{
	PSX::Vertex v = {};
	std::vector<uint8_t> buffer(sizeof(v));
	v.pos = ConvertVec3(m_pos, FP_ONE_GEO);
	v.flags = VertexFlags::NONE;
	v.colorHi = ConvertColor(m_colorHigh);
	v.colorLo = ConvertColor(m_colorLow);
	std::memcpy(buffer.data(), &v, sizeof(v));
	return buffer;
}

Color Vertex::GetColor(bool high) const
{
	return high ? m_colorHigh : m_colorLow;
}
