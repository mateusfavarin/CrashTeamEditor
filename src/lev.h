#pragma once

#include "math.h"

#include <cstdint>

static constexpr size_t NUM_GRADIENT = 3;
static constexpr size_t NUM_DRIVERS = 8;

enum class LevConfigFlags : uint32_t
{
	NONE = 0,
	ENABLE_SKYBOX_GRADIENT = 1 << 0,
	MASK_GRAB_UNDERWATER = 1 << 1,
	ANIMATE_WATER_VERTEX = 1 << 2,
};

static constexpr size_t NUM_LEV_CONFIG_FLAGS = 3;

inline LevConfigFlags& operator|=(LevConfigFlags& a, const LevConfigFlags& b)
{
	a = static_cast<LevConfigFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	return a;
}

inline LevConfigFlags& operator&=(LevConfigFlags& a, const LevConfigFlags& b)
{
	a = static_cast<LevConfigFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	return a;
}

inline LevConfigFlags operator~(const LevConfigFlags& a)
{
	return static_cast<LevConfigFlags>(~static_cast<uint32_t>(a));
}

struct Spawn
{
	Vec3 pos;
	Vec3 rot;
};

struct ColorGradient
{
	float posFrom;
	float posTo;
	Color colorFrom;
	Color colorTo;
};