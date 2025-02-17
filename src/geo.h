#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

struct Color
{
	Color() : r(0.0f), g(0.0f), b(0.0f), a(false) {};
	Color(float r, float g, float b) : r(r), g(g), b(b), a(false) {};
	Color(unsigned char r, unsigned char g, unsigned char b) : a(false)
	{
		this->r = static_cast<float>(r) / 255.0f;
		this->g = static_cast<float>(g) / 255.0f;
		this->b = static_cast<float>(b) / 255.0f;
		this->rb = r;
		this->gb = g;
		this->bb = b;
	};
	Color(double hue, double sat, double value) : a(false) {
		double      hh, p, q, t, ff;
		long        i;

		if (sat == 0) {
			this->r = this->g = this->b = 0;
			return;
		}
		else {
			hh = hue;
			if (hh >= 360.0) hh = 0.0;
			hh /= 60.0;
			i = (long)hh;
			ff = hh - i;
			p = value * (1.0 - sat);
			q = value * (1.0 - (sat * ff));
			t = value * (1.0 - (sat * (1.0 - ff)));

			switch (i) {
			case 0:
				this->r = value;
				this->g = t;
				this->b = p;
				this->rb = static_cast<unsigned char>(value * 255.0f);
				this->gb = static_cast<unsigned char>(t * 255.0f);
				this->bb = static_cast<unsigned char>(p * 255.0f);
				break;
			case 1:
				this->r = q;
				this->g = value;
				this->b = p;
				this->rb = static_cast<unsigned char>(q * 255.0f);
				this->gb = static_cast<unsigned char>(value * 255.0f);
				this->bb = static_cast<unsigned char>(p * 255.0f);
				break;
			case 2:
				this->r = p;
				this->g = value;
				this->b = t;
				this->rb = static_cast<unsigned char>(p * 255.0f);
				this->gb = static_cast<unsigned char>(value * 255.0f);
				this->bb = static_cast<unsigned char>(t * 255.0f);
				break;
			case 3:
				this->r = p;
				this->g = q;
				this->b = value;
				this->rb = static_cast<unsigned char>(p * 255.0f);
				this->gb = static_cast<unsigned char>(q * 255.0f);
				this->bb = static_cast<unsigned char>(value * 255.0f);
				break;
			case 4:
				this->r = t;
				this->g = p;
				this->b = value;
				this->rb = static_cast<unsigned char>(t * 255.0f);
				this->gb = static_cast<unsigned char>(p * 255.0f);
				this->bb = static_cast<unsigned char>(value * 255.0f);
				break;
			case 5:
			default:
				this->r = value;
				this->g = p;
				this->b = q;
				this->rb = static_cast<unsigned char>(value * 255.0f);
				this->gb = static_cast<unsigned char>(p * 255.0f);
				this->bb = static_cast<unsigned char>(q * 255.0f);
				break;
			}
		}
	}
	float* Data() { return &r; }
	inline bool operator==(const Color& color) const { return (r == color.r) && (g == color.g) && (b == color.b) && (a == color.a); }

	float r, g, b;
	unsigned char rb, gb, bb;
	bool a;
};

template<>
struct std::hash<Color>
{
	inline std::size_t operator()(const Color& key) const
	{
		return ((((std::hash<float>()(key.r) ^ (std::hash<float>()(key.g) << 1)) >> 1) ^ (std::hash<float>()(key.b) << 1)) >> 2) ^ (std::hash<bool>()(key.a) << 2);
	}
};

struct Vec3
{
	Vec3() : x(0.0f), y(0.0f), z(0.0f) {};
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {};
	float* Data() { return &x; }
	inline float Length() const { return static_cast<float>(std::sqrt((x * x) + (y * y) + (z * z))); }
	inline Vec3 Cross(const Vec3& v) const { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - v.x * y }; }

	inline Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
	inline Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
	inline Vec3 operator*(float n) const { return { x * n, y * n, z * n }; }
	inline Vec3 operator/(float n) const { return { x / n, y / n, z / n }; }
	inline bool operator>(float n) const { return x > n && y > n && z > n; }
	inline bool operator<(float n) const { return x < n && y < n && z < n; }
	inline bool operator==(const Vec3& v) const { return (x == v.x) && (y == v.y) && (z == v.z); }
	inline Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
	inline Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	inline Vec3& operator*=(float n) { x *= n; y *= n; z *= n; return *this; }
	inline Vec3& operator/=(float n) { x /= n; y /= n; z /= n; return *this; }

	float x;
	float y;
	float z;
};

template<>
struct std::hash<Vec3>
{
	inline std::size_t operator()(const Vec3& key) const
	{
		return ((std::hash<float>()(key.x) ^ (std::hash<float>()(key.y) << 1)) >> 1) ^ (std::hash<float>()(key.z) << 1);
	}
};

struct BoundingBox
{
	Vec3 min;
	Vec3 max;

	float Area() const;
	float SemiPerimeter() const;
	Vec3 AxisLength() const;
	Vec3 Midpoint() const;
	void RenderUI() const;
};

struct Point
{
	Vec3 pos;
	Vec3 normal;
	Color color;

	Point() {};
	Point(float x, float y, float z)
	{
		pos = Vec3(x, y, z);
		color = Color((unsigned char)255u, 255u, 255u);
		normal = Vec3();
	};
	Point(float x, float y, float z, unsigned char r, unsigned char g, unsigned char b)
	{
		pos = Vec3(x, y, z);
		color = Color(r, g, b);
		normal = Vec3();
	};
};

struct Tri
{
	Tri() : p() {};
	Tri(const Point& p0, const Point& p1, const Point& p2);

	Point p[3];
};

struct Quad
{
	Quad() : p() {};
	Quad(const Point& p0, const Point& p1, const Point& p2, const Point& p3);

	Point p[4];
};

template<typename T>
T Clamp(T n, T min, T max)
{
	return std::max(std::min(n, max), min);
}

template<typename T>
void Swap(T& x, T& y)
{
	T temp = y;
	y = x;
	x = temp;
}
