#include "io.h"
#include "geo.h"
#include "utils.h"

#include <unordered_map>
#include <filesystem>

void to_json(nlohmann::json& json, const Vec3& v)
{
	json = {{"x", v.x}, {"y", v.y}, {"z", v.z}};
}

void from_json(const nlohmann::json& json, Vec3& v)
{
	json.at("x").get_to(v.x);
	json.at("y").get_to(v.y);
	json.at("z").get_to(v.z);
}

void to_json(nlohmann::json& json, const Color& c)
{
	json = {{"r", c.Red()}, {"g", c.Green()}, {"b", c.Blue()}, {"a", c.a}};
}

void from_json(const nlohmann::json& json, Color& c)
{
	bool a;
	float r, g, b;
	json.at("r").get_to(r);
	json.at("g").get_to(g);
	json.at("b").get_to(b);
	json.at("a").get_to(a);
	c = Color(r, g, b, a);
}

void to_json(nlohmann::json& json, const Spawn& spawn)
{
	json = {{"pos", spawn.pos}, {"rot", spawn.rot}};
}

void from_json(const nlohmann::json& json, Spawn& spawn)
{
	json.at("pos").get_to(spawn.pos);
	json.at("rot").get_to(spawn.rot);
}

void to_json(nlohmann::json& json, const ColorGradient& spawn)
{
	json = {{"posFrom", spawn.posFrom}, {"posTo", spawn.posTo}, {"colorFrom", spawn.colorFrom}, {"colorTo", spawn.colorTo}};
}

void from_json(const nlohmann::json& json, ColorGradient& spawn)
{
	json.at("posFrom").get_to(spawn.posFrom);
	json.at("posTo").get_to(spawn.posTo);
	json.at("colorFrom").get_to(spawn.colorFrom);
	json.at("colorTo").get_to(spawn.colorTo);
}

void ReadBinaryFile(std::vector<uint8_t>& v, const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	v.resize(size);
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(v.data()), size);
	file.close();
}

void Path::ToJson(nlohmann::json& json, const std::vector<Quadblock>& quadblocks) const
{
	json = {{"index", m_index}, {"hasLeft", m_left != nullptr}, {"hasRight", m_right != nullptr}};

	std::vector<std::string> quadStart, quadEnd, quadIgnore;
	for (size_t index : m_quadIndexesStart) { quadStart.push_back(quadblocks[index].GetName()); }
	for (size_t index : m_quadIndexesEnd) { quadEnd.push_back(quadblocks[index].GetName()); }
	for (size_t index : m_quadIndexesIgnore) { quadIgnore.push_back(quadblocks[index].GetName()); }
	json["quadStart"] = quadStart;
	json["quadEnd"] = quadEnd;
	json["quadIgnore"] = quadIgnore;

	if (m_left) { json["left"] = nlohmann::json(); m_left->ToJson(json["left"], quadblocks); }
	if (m_right) { json["right"] = nlohmann::json(); m_right->ToJson(json["right"], quadblocks); }
}

void Path::FromJson(const nlohmann::json& json, const std::vector<Quadblock>& quadblocks)
{
	bool hasLeft = false;
	bool hasRight = false;
	std::vector<std::string> quadStart, quadEnd, quadIgnore;
	json.at("index").get_to(m_index);
	json.at("hasLeft").get_to(hasLeft); if (hasLeft) { m_left = new Path(); m_left->FromJson(json.at("left"), quadblocks); }
	json.at("hasRight").get_to(hasRight); if (hasRight) { m_right = new Path(); m_right->FromJson(json.at("right"), quadblocks); }
	json.at("quadStart").get_to(quadStart);
	json.at("quadEnd").get_to(quadEnd);
	json.at("quadIgnore").get_to(quadIgnore);

	std::unordered_map<std::string, size_t> quadNameMap;
	for (size_t i = 0; i < quadblocks.size(); i++) { quadNameMap[quadblocks[i].GetName()] = i; }
	for (const std::string& name : quadStart) { if (quadNameMap.contains(name)) { m_quadIndexesStart.push_back(quadNameMap[name]); } }
	for (const std::string& name : quadEnd) { if (quadNameMap.contains(name)) { m_quadIndexesEnd.push_back(quadNameMap[name]); } }
	for (const std::string& name : quadIgnore) { if (quadNameMap.contains(name)) { m_quadIndexesIgnore.push_back(quadNameMap[name]); } }
}