#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

struct Color
{
	Color() : r(0.0f), g(0.0f), b(0.0f), a(false) {};
	Color(float r, float g, float b) : r(r), g(g), b(b), a(false) {};
	Color(unsigned r, unsigned g, unsigned b) : a(false)
	{
		this->r = static_cast<float>(r) / 255.0f;
		this->g = static_cast<float>(g) / 255.0f;
		this->b = static_cast<float>(b) / 255.0f;
	};
	float* Data() { return &r; }
	inline bool operator==(const Color& color) const { return (r == color.r) && (g == color.g) && (b == color.b) && (a == color.a); }

	float r;
	float g;
	float b;
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
		color = Color(255u, 255u, 255u);
		normal = Vec3();
	};
	Point(float x, float y, float z, unsigned r, unsigned g, unsigned b)
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