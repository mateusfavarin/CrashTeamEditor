#pragma once

#include "geo.h"

#include <cstdint>

static constexpr size_t NUM_GRADIENT = 3;
static constexpr size_t NUM_DRIVERS = 8;
static constexpr size_t NUM_LEV_CONFIG_FLAGS = 3;
static constexpr size_t GHOST_DATA_FILESIZE = 0x3E00;

struct LevConfigFlags
{
	static constexpr uint32_t NONE = 0;
	static constexpr uint32_t ENABLE_SKYBOX_GRADIENT = 1 << 0;
	static constexpr uint32_t MASK_GRAB_UNDERWATER = 1 << 1;
	static constexpr uint32_t ANIMATE_WATER_VERTEX = 1 << 2;
};

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