#pragma once

#include "utils.h"

#include <vector>
#include <cstdint>
#include <cmath>
#include <string>

struct Color
{
 	Color() : r(0u), g(0u), b(0u), a(false) {};
 	Color(float r, float g, float b) : r(static_cast<unsigned char>(Clamp(r * 255.0f, 0.0f, 255.0f))), g(static_cast<unsigned char>(Clamp(g * 255.0f, 0.0f, 255.0f))), b(static_cast<unsigned char>(Clamp(b * 255.0f, 0.0f, 255.0f))), a(false) {};
 	Color(float r, float g, float b, bool a) : r(static_cast<unsigned char>(Clamp(r * 255.0f, 0.0f, 255.0f))), g(static_cast<unsigned char>(Clamp(g * 255.0f, 0.0f, 255.0f))), b(static_cast<unsigned char>(Clamp(b * 255.0f, 0.0f, 255.0f))), a(a) {};
	Color(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b), a(false) {};
	Color(unsigned char r, unsigned char g, unsigned char b, bool a) : r(r), g(g), b(b), a(a) {};
	Color(double hue, double sat, double value);
 	inline bool operator==(const Color& color) const { return (r == color.r) && (g == color.g) && (b == color.b) && (a == color.a); }
 	inline float Red() const { return static_cast<float>(r) / 255.0f; }
 	inline float Green() const { return static_cast<float>(g) / 255.0f; }
 	inline float Blue() const { return static_cast<float>(b) / 255.0f; }
 	inline Color Negated() const
 	{
 		return Color(static_cast<unsigned char>(255 - r), static_cast<unsigned char>(255 - g), static_cast<unsigned char>(255 - b));
 	}

 	unsigned char r, g, b;
 	bool a;
};

template<>
struct std::hash<Color>
{
	inline std::size_t operator()(const Color& key) const
	{
		return ((((std::hash<float>()(key.Red()) ^ (std::hash<float>()(key.Green()) << 1)) >> 1) ^ (std::hash<float>()(key.Blue()) << 1)) >> 2) ^ (std::hash<bool>()(key.a) << 2);
	}
};

struct Vec2
{
	Vec2() : x(0.0f), y(0.0f) {};
	Vec2(float x, float y) : x(x), y(y) {};
	inline float* Data() { return &x; }
	inline const float* Data() const { return &x; }

	inline bool operator==(const Vec2& v) const { return (x == v.x) && (y == v.y); }
	inline bool operator!=(const Vec2& v) const { return !(*this == v); }

	float x;
	float y;
};

typedef std::array<Vec2, 4> QuadUV;

struct Vec3
{
	Vec3() : x(0.0f), y(0.0f), z(0.0f) {};
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {};
	inline float* Data() { return &x; }
	inline const float* Data() const { return &x; }
	inline float Length() const { return static_cast<float>(std::sqrt((x * x) + (y * y) + (z * z))); }
	inline float LengthSquared() const { return (x * x) + (y * y) + (z * z); }
	inline Vec3 Cross(const Vec3& v) const { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - v.x * y }; }
	inline float Dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
	inline void Normalize() { const float len = Length(); x /= len; y /= len; z /= len; }

	inline Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
	inline Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
	inline Vec3 operator*(float n) const { return { x * n, y * n, z * n }; }
	inline Vec3 operator/(float n) const { return { x / n, y / n, z / n }; }
	inline bool operator>(float n) const { return x > n && y > n && z > n; }
	inline bool operator<(float n) const { return x < n && y < n && z < n; }
	inline bool operator==(const Vec3& v) const { return (x == v.x) && (y == v.y) && (z == v.z); }
	inline bool operator!=(const Vec3& v) const { return !(*this == v); }
	inline Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
	inline Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	inline Vec3& operator*=(float n) { x *= n; y *= n; z *= n; return *this; }
	inline Vec3& operator/=(float n) { x /= n; y /= n; z /= n; return *this; }

	static Vec3 Zero() { return Vec3(); }
	static Vec3 One() { return Vec3(1.0f, 1.0f, 1.0f); }

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

struct Tri;
struct BoundingBox
{
	Vec3 min;
	Vec3 max;

	float Area() const;
	float SemiPerimeter() const;
	Vec3 AxisLength() const;
	Vec3 Midpoint() const;
	std::vector<Tri> ToGeometry() const;
	void RenderUI() const;
};

struct Point
{
	Vec3 pos;
	Vec3 normal;
	Color color;
	Vec2 uv;

	Point() {};
	Point(float x, float y, float z)
	{
		pos = Vec3(x, y, z);
		color = Color(static_cast<unsigned char>(128), 128, 128);
		normal = Vec3();
		uv = Vec2();
	};
	Point(float x, float y, float z, unsigned char r, unsigned char g, unsigned char b)
	{
		pos = Vec3(x, y, z);
		color = Color(r, g, b);
		normal = Vec3();
		uv = Vec2();
	};
};

struct Tri
{
	Tri() : p() {};
	Tri(const Point& p0, const Point& p1, const Point& p2);

	Point p[3];
	std::string texture;
};

struct Quad
{
	Quad() : p() {};
	Quad(const Point& p0, const Point& p1, const Point& p2, const Point& p3);

	Point p[4];
	std::string texture;
};
