#include "math.h"

Quad::Quad(const Point& p0, const Point& p1, const Point& p2, const Point& p3)
{
	p[0] = p0; p[1] = p1; p[2] = p2; p[3] = p3;
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
