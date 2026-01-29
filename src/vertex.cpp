#include "vertex.h"

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

Vertex::Vertex(const PSX::Vertex& vertex)
{
	m_pos = ConvertPSXVec3(vertex.pos, FP_ONE_GEO);
	m_flags = vertex.flags;
	m_colorHigh = ConvertColor(vertex.colorHi);
	m_colorLow = ConvertColor(vertex.colorLo);
	m_normal = {0.0f, 1.0f, 0.0f};
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

std::vector<Primitive> Vertex::ToGeometry(bool highColor) const
{
	constexpr float radius = 0.5f;
	constexpr float sqrtThree = 1.44224957031f;
	constexpr size_t vertsPerOctopoint = 24;
	constexpr size_t trisPerOctopoint = vertsPerOctopoint / 3;
	const Vec3 radiusDirection[trisPerOctopoint] =
	{
		{1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, -1.0f},
		{-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f},
	};

	Vertex v(*this);
	std::vector<Primitive> triangles;
	triangles.reserve(trisPerOctopoint);

	auto AppendTri = [&](const Vec3& dir)
		{
			std::vector<Point> points;
			points.reserve(3);
			v.m_normal = Vec3((1.f / sqrtThree) * dir.x, (1.f / sqrtThree) * dir.y, (1.f / sqrtThree) * dir.z);
			v.m_pos.x += (radius * dir.x); points.emplace_back(v.m_pos, v.m_normal, v.GetColor(highColor)); v.m_pos.x -= (radius * dir.x);
			v.m_pos.y += (radius * dir.y); points.emplace_back(v.m_pos, v.m_normal, v.GetColor(highColor)); v.m_pos.y -= (radius * dir.y);
			v.m_pos.z += (radius * dir.z); points.emplace_back(v.m_pos, v.m_normal, v.GetColor(highColor)); v.m_pos.z -= (radius * dir.z);
			triangles.push_back(Tri(points[0], points[1], points[2]));
		};

	for (const Vec3& dir : radiusDirection) { AppendTri(dir); }
	return triangles;
}
