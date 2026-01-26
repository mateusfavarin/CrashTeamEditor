#include "text3d.h"

#include <algorithm>
#include <vector>

#include "stb_easy_font.h"

struct StbVertex
{
	float x;
	float y;
	float z;
	unsigned char c[4];
};

static constexpr float DEFAULT_FONT_SCALE = 0.1f;

Text3D::Text3D()
	: Text3D(std::string())
{
}

Text3D::Text3D(const std::string& label, TextAlign align, const Color& color, const Color& backgroundColor)
	: m_label(label)
	, m_align(align)
	, m_color(color)
	, m_backgroundColor(backgroundColor)
{
	SetRotation(Vec3(180.0f, 0.0f, 0.0f));
	SetScale(Vec3(DEFAULT_FONT_SCALE, DEFAULT_FONT_SCALE, DEFAULT_FONT_SCALE));
	UpdateMeshFromLabel();
}

const std::string& Text3D::GetLabel() const
{
	return m_label;
}

void Text3D::SetLabel(const std::string& label)
{
	if (m_label == label) { return; }
	m_label = label;
	UpdateMeshFromLabel();
}

Text3D::TextAlign Text3D::GetAlign() const
{
	return m_align;
}

void Text3D::SetAlign(TextAlign align)
{
	if (m_align == align) { return; }
	m_align = align;
	UpdateMeshFromLabel();
}

const Color& Text3D::GetColor() const
{
	return m_color;
}

void Text3D::SetColor(const Color& color)
{
	m_color = color;
	UpdateMeshFromLabel();
}

const Color& Text3D::GetBackgroundColor() const
{
	return m_backgroundColor;
}

void Text3D::SetBackgroundColor(const Color& color)
{
	m_backgroundColor = color;
	UpdateMeshFromLabel();
}

float Text3D::GetSize() const
{
	return m_size;
}

void Text3D::SetSize(float size)
{
	m_size = size;
	SetScale(DEFAULT_FONT_SCALE * m_size);
}

Mesh& Text3D::GetMesh()
{
	return m_mesh;
}

const Mesh& Text3D::GetMesh() const
{
	return m_mesh;
}

void Text3D::UpdateMeshFromLabel()
{
	m_mesh.Clear();
	if (m_label.empty()) { return; }

	const Vec3 normalUp = Vec3(0.0f, 0.0f, 1.0f);
	char* labelText = const_cast<char*>(m_label.c_str());
	const int textWidth = stb_easy_font_width(labelText);
	const int textHeight = stb_easy_font_height(labelText);
	float xOffset = 0.0f;
	if (m_align == TextAlign::CENTER) { xOffset = -static_cast<float>(textWidth) * 0.5f; }
	else if (m_align == TextAlign::RIGHT) { xOffset = -static_cast<float>(textWidth); }

	std::vector<Tri> triangles;
	if (m_backgroundColor.a > 0 && textWidth > 0 && textHeight > 0)
	{
		const Color bgColor = m_backgroundColor;
		const float z = -0.01f;
		const float width = static_cast<float>(textWidth);
		const float height = static_cast<float>(textHeight);

		Point p0 = Point(xOffset, 0.0f, z, normalUp, bgColor);
		Point p1 = Point(xOffset + width, 0.0f, z, normalUp, bgColor);
		Point p2 = Point(xOffset + width, height, z, normalUp, bgColor);
		Point p3 = Point(xOffset, height, z, normalUp, bgColor);

		triangles.emplace_back(p0, p1, p2);
		triangles.emplace_back(p0, p2, p3);
	}

	const size_t bufferBytes = (m_label.size() + 1) * 270u;
	const size_t maxVerts = bufferBytes / sizeof(StbVertex);
	if (maxVerts == 0) { return; }

	std::vector<StbVertex> vbuf(maxVerts);
	const int quadCount = stb_easy_font_print(0.0f, 0.0f, labelText, nullptr, vbuf.data(), static_cast<int>(bufferBytes));
	if (quadCount <= 0) { return; }

	const size_t safeQuadCount = std::min(static_cast<size_t>(quadCount), vbuf.size() / 4u);
	triangles.reserve(triangles.size() + (safeQuadCount * 2u));

	const Color textColor = m_color;
	for (size_t quadIndex = 0; quadIndex < safeQuadCount; ++quadIndex)
	{
		const StbVertex& v0 = vbuf[quadIndex * 4u + 0u];
		const StbVertex& v1 = vbuf[quadIndex * 4u + 1u];
		const StbVertex& v2 = vbuf[quadIndex * 4u + 2u];
		const StbVertex& v3 = vbuf[quadIndex * 4u + 3u];

		Point p0 = Point(v0.x + xOffset, v0.y, v0.z, normalUp, textColor);
		Point p1 = Point(v1.x + xOffset, v1.y, v1.z, normalUp, textColor);
		Point p2 = Point(v2.x + xOffset, v2.y, v2.z, normalUp, textColor);
		Point p3 = Point(v3.x + xOffset, v3.y, v3.z, normalUp, textColor);

		triangles.emplace_back(p0, p1, p2);
		triangles.emplace_back(p0, p2, p3);
	}

	m_mesh.SetGeometry(triangles, Mesh::RenderFlags::DontOverrideRenderFlags | Mesh::RenderFlags::DrawBackfaces);
}
