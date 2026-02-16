#pragma once

#include "lev.h"
#include "path.h"
#include "quadblock.h"
#include "animtexture.h"
#include "minimap.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <vector>
#include <cstdint>

void to_json(nlohmann::json& json, const Vec3& v);
void from_json(const nlohmann::json& json, Vec3& v);

void to_json(nlohmann::json& json, const Color& c);
void from_json(const nlohmann::json& json, Color& c);

void to_json(nlohmann::json& json, const Spawn& spawn);
void from_json(const nlohmann::json& json, Spawn& spawn);

void to_json(nlohmann::json& json, const ColorGradient& spawn);
void from_json(const nlohmann::json& json, ColorGradient& spawn);

void to_json(nlohmann::json& json, const Stars& stars);
void from_json(const nlohmann::json& json, Stars& stars);

void to_json(nlohmann::json& json, const MinimapConfig& minimap);
void from_json(const nlohmann::json& json, MinimapConfig& minimap);

void ReadBinaryFile(std::vector<uint8_t>& v, const std::filesystem::path& path);

template<typename T> static inline void Read(std::ifstream& file, T& data)
{
	file.read(reinterpret_cast<char*>(&data), sizeof(data));
}

template<typename T> static inline void Write(std::ofstream& file, T* data, size_t size)
{
	file.write(reinterpret_cast<const char*>(data), size);
}