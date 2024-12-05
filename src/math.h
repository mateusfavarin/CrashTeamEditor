#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

struct Color
{
	Color() : r(0.0f), g(0.0f), b(0.0f), a(false) {};
	Color(unsigned red, unsigned green, unsigned blue) : a(false)
	{
		r = static_cast<float>(red) / 255.0f;
		g = static_cast<float>(green) / 255.0f;
		b = static_cast<float>(blue) / 255.0f;
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
	inline float Length() const { return std::sqrtf((x * x) + (y * y) + (z * z)); }
	inline Vec3 Cross(const Vec3& v) const { return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - v.x * y }; }

	inline Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z}; }
	inline Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
	inline Vec3 operator*(float n) const { return { x * n, y * n, z * n }; }
	inline Vec3 operator/(float n) const { return {x / n, y / n, z / n}; }
	inline bool operator>(float n) const { return x > n && y > n && z > n; }
	inline bool operator<(float n) const { return x < n && y < n && z < n; }
	inline bool operator==(const Vec3& v) const { return (x == v.x) && (y == v.y) && (z == v.z); }

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
	void RenderUI() const;
};

struct Quad
{
	Quad() : p() {};
	Quad(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3);

	Vec3 p[4];
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
