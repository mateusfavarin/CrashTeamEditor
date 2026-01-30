#include "geo.h"

Tri::Tri(const Point& p0, const Point& p1, const Point& p2)
	: Primitive(PrimitiveType::TRI, 3)
{
	p[0] = p0; p[1] = p1, p[2] = p2;
}

Quad::Quad(const Point& p0, const Point& p1, const Point& p2, const Point& p3)
	: Primitive(PrimitiveType::QUAD, 4)
{
	p[0] = p0; p[1] = p1; p[2] = p2; p[3] = p3;
}

Line::Line(const Point& p0, const Point& p1)
	: Primitive(PrimitiveType::LINE, 2)
{
	p[0] = p0; p[1] = p1;
}

float BoundingBox::Area() const
{
	Vec3 dist = max - min;
	return dist.x * dist.y * dist.z;
}

float BoundingBox::SemiPerimeter() const
{
	Vec3 dist = max - min;
	return dist.x + dist.y + dist.z;
}

Vec3 BoundingBox::AxisLength() const
{
	return max - min;
}

Vec3 BoundingBox::Midpoint() const
{
	return (max + min) / 2;
}

std::vector<Primitive> BoundingBox::ToGeometry() const
{
	constexpr size_t numCorners = 8;
	constexpr size_t numEdges = 12;
	constexpr float sqrtThree = 1.44224957031f;
	const Vec3 corners[numCorners] =
	{
		Vec3(min.x, min.y, min.z),
		Vec3(min.x, min.y, max.z),
		Vec3(min.x, max.y, min.z),
		Vec3(max.x, min.y, min.z),
		Vec3(max.x, max.y, min.z),
		Vec3(min.x, max.y, max.z),
		Vec3(max.x, min.y, max.z),
		Vec3(max.x, max.y, max.z),
	};
	const Vec3 cornerNormals[numCorners] =
	{
		Vec3(-1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree),
		Vec3(-1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree),
		Vec3(-1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree),
		Vec3(1.f / sqrtThree, -1.f / sqrtThree, -1.f / sqrtThree),
		Vec3(1.f / sqrtThree, 1.f / sqrtThree, -1.f / sqrtThree),
		Vec3(-1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree),
		Vec3(1.f / sqrtThree, -1.f / sqrtThree, 1.f / sqrtThree),
		Vec3(1.f / sqrtThree, 1.f / sqrtThree, 1.f / sqrtThree),
	};
	const int edgeIndices[numEdges][2] =
	{
		{0, 1}, {2, 5}, {3, 6}, {4, 7},
		{0, 2}, {1, 5}, {3, 4}, {6, 7},
		{0, 3}, {1, 6}, {2, 4}, {5, 7},
	};

	std::vector<Primitive> primitives;
	primitives.reserve(numEdges);
	for (int edgeIndex = 0; edgeIndex < 12; edgeIndex++)
	{
		const int a = edgeIndices[edgeIndex][0];
		const int b = edgeIndices[edgeIndex][1];
		Line line;
		line.texture = std::string();
		line.p[0].pos = corners[a];
		line.p[0].normal = cornerNormals[a];
		line.p[0].color = Color();
		line.p[0].uv = Vec2();
		line.p[1].pos = corners[b];
		line.p[1].normal = cornerNormals[b];
		line.p[1].color = Color();
		line.p[1].uv = Vec2();
		primitives.push_back(line);
	}

	return primitives;
}

Color::Color(double hue, double sat, double value)
{
	a = 255u;

	if (sat == 0)
	{
		r = g = b = 0;
		return;
	}

	long i;
	double hh, p, q, t, ff;

	hh = hue;
	if (hh >= 360.0) hh = 0.0;
	hh /= 60.0;
	i = static_cast<long>(hh);
	ff = hh - i;
	p = value * (1.0 - sat);
	q = value * (1.0 - (sat * ff));
	t = value * (1.0 - (sat * (1.0 - ff)));

	t = Clamp(t, 0.0, 1.0);
	p = Clamp(p, 0.0, 1.0);
	q = Clamp(q, 0.0, 1.0);
	value = Clamp(value, 0.0, 1.0);

	switch (i) {
	case 0:
		r = static_cast<unsigned char>(value * 255.0);
		g = static_cast<unsigned char>(t * 255.0);
		b = static_cast<unsigned char>(p * 255.0);
		break;
	case 1:
		r = static_cast<unsigned char>(q * 255.0);
		g = static_cast<unsigned char>(value * 255.0);
		b = static_cast<unsigned char>(p * 255.0);
		break;
	case 2:
		r = static_cast<unsigned char>(p * 255.0);
		g = static_cast<unsigned char>(value * 255.0);
		b = static_cast<unsigned char>(t * 255.0);
		break;
	case 3:
		r = static_cast<unsigned char>(p * 255.0);
		g = static_cast<unsigned char>(q * 255.0);
		b = static_cast<unsigned char>(value * 255.0);
		break;
	case 4:
		r = static_cast<unsigned char>(t * 255.0);
		g = static_cast<unsigned char>(p * 255.0);
		b = static_cast<unsigned char>(value * 255.0);
		break;
	case 5:
	default:
		r = static_cast<unsigned char>(value * 255.0);
		g = static_cast<unsigned char>(p * 255.0);
		b = static_cast<unsigned char>(q * 255.0);
		break;
	}
}
