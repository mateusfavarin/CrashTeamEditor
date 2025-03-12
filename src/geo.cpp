#include "geo.h"

Tri::Tri(const Point& p0, const Point& p1, const Point& p2)
{
	p[0] = p0; p[1] = p1, p[2] = p2;
}

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

Vec3 BoundingBox::AxisLength() const
{
	return max - min;
}

Vec3 BoundingBox::Midpoint() const
{
	return (max + min) / 2;
}

Color::Color(double hue, double sat, double value)
{
	a = false;

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