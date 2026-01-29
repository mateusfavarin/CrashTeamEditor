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

constexpr float DEFAULT_FONT_SCALE = 0.1f;

std::vector<Primitive> Text3D::ToGeometry(const std::string& label, Align align, const Color& color)
{
	std::vector<Primitive> primitives;
	if (label.empty()) { return primitives; }

	const Vec3 normalUp = Vec3(0.0f, 0.0f, 1.0f);
	char* labelText = const_cast<char*>(label.c_str());
	const int textWidth = stb_easy_font_width(labelText);
	float xOffset = 0.0f;
	if (align == Align::CENTER) { xOffset = -static_cast<float>(textWidth) * 0.5f; }
	else if (align == Align::RIGHT) { xOffset = -static_cast<float>(textWidth); }

	const size_t bufferBytes = (label.size() + 1) * 270u;
	const size_t maxVerts = bufferBytes / sizeof(StbVertex);
	if (maxVerts == 0) { return primitives; }

	std::vector<StbVertex> vbuf(maxVerts);
	const int quadCount = stb_easy_font_print(0.0f, 0.0f, labelText, nullptr, vbuf.data(), static_cast<int>(bufferBytes));
	if (quadCount <= 0) { return primitives; }

	const size_t safeQuadCount = std::min(static_cast<size_t>(quadCount), vbuf.size() / 4u);
	primitives.reserve(safeQuadCount);

	auto ApplyTransform = [](Point& point, float scale)
		{
			point.pos.x *= scale;
			point.pos.y *= scale;
			point.pos.z *= scale;

			point.pos.y = -point.pos.y;
			point.pos.z = -point.pos.z;
		};

	for (size_t quadIndex = 0; quadIndex < safeQuadCount; ++quadIndex)
	{
		const StbVertex& v0 = vbuf[quadIndex * 4u + 0u];
		const StbVertex& v1 = vbuf[quadIndex * 4u + 1u];
		const StbVertex& v2 = vbuf[quadIndex * 4u + 2u];
		const StbVertex& v3 = vbuf[quadIndex * 4u + 3u];

		Point p0 = Point(v0.x + xOffset, v0.y, v0.z, normalUp, color);
		Point p1 = Point(v1.x + xOffset, v1.y, v1.z, normalUp, color);
		Point p2 = Point(v2.x + xOffset, v2.y, v2.z, normalUp, color);
		Point p3 = Point(v3.x + xOffset, v3.y, v3.z, normalUp, color);

		ApplyTransform(p0, DEFAULT_FONT_SCALE);
		ApplyTransform(p1, DEFAULT_FONT_SCALE);
		ApplyTransform(p2, DEFAULT_FONT_SCALE);
		ApplyTransform(p3, DEFAULT_FONT_SCALE);

		Quad quad;
		quad.p[0] = p1;
		quad.p[1] = p0;
		quad.p[2] = p2;
		quad.p[3] = p3;
		primitives.push_back(quad);
	}

	return primitives;
}
