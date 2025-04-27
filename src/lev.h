#pragma once

#include "geo.h"

#include <vector>
#include <string>
#include <cstdint>

static constexpr size_t NUM_GRADIENT = 3;
static constexpr size_t NUM_DRIVERS = 8;
static constexpr size_t NUM_LEV_CONFIG_FLAGS = 3;
static constexpr size_t GHOST_DATA_FILESIZE = 0x3E00;
static constexpr size_t NUM_VERTICES_QUADBLOCK = 9;
static constexpr size_t OT_SIZE = 1024;

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

struct Stars
{
    uint16_t numStars;
    bool spread;
    uint16_t seed;
    uint16_t zDepth;
};

static const std::vector<std::string> CTR_CHARACTERS = {
	"Crash Bandicoot", "Dr. Neo Cortex", "Coco Bandicoot", "Tiny Tiger",
	"N. Gin", "Dingodile", "Polar", "Pura", "Pinstripe", "Papu Papu",
	"Ripper Roo", "Komodo Joe", "N. Tropy", "Penta Penguin",
	"Fake Crash", "Nitrous Oxide"
};