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
	if (json.contains("x")) { v.x = json["x"]; }
	if (json.contains("y")) { v.y = json["y"]; }
	if (json.contains("z")) { v.z = json["z"]; }
}

void to_json(nlohmann::json& json, const Color& c)
{
	json = {{"r", c.Red()}, {"g", c.Green()}, {"b", c.Blue()}, {"a", c.a}};
}

void from_json(const nlohmann::json& json, Color& c)
{
	bool a = false;
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	if (json.contains("r")) { json.at("r").get_to(r); }
	if (json.contains("g")) { json.at("g").get_to(g); }
	if (json.contains("b")) { json.at("b").get_to(b); }
	if (json.contains("a")) { json.at("a").get_to(a); }
	c = Color(r, g, b, a);
}

void to_json(nlohmann::json& json, const Spawn& spawn)
{
	json = {{"pos", spawn.pos}, {"rot", spawn.rot}};
}

void from_json(const nlohmann::json& json, Spawn& spawn)
{
	if (json.contains("pos")) { json.at("pos").get_to(spawn.pos); }
	if (json.contains("rot")) { json.at("rot").get_to(spawn.rot); }
}

void to_json(nlohmann::json& json, const ColorGradient& spawn)
{
	json = {{"posFrom", spawn.posFrom}, {"posTo", spawn.posTo}, {"colorFrom", spawn.colorFrom}, {"colorTo", spawn.colorTo}};
}

void from_json(const nlohmann::json& json, ColorGradient& spawn)
{
	if (json.contains("posFrom")) { json.at("posFrom").get_to(spawn.posFrom); }
	if (json.contains("posTo")) { json.at("posTo").get_to(spawn.posTo); }
	if (json.contains("colorFrom")) { json.at("colorFrom").get_to(spawn.colorFrom); }
	if (json.contains("colorTo")) { json.at("colorTo").get_to(spawn.colorTo); }
}

void to_json(nlohmann::json& json, const Stars& stars)
{
    json = {
        {"numStars", stars.numStars},
        {"spread", stars.spread},
        {"seed", stars.seed},
        {"depth", stars.zDepth}
    };
}

void from_json(const nlohmann::json& json, Stars& stars)
{
    if (json.contains("numStars")) { stars.numStars = json["numStars"]; }
    if (json.contains("spread")) { stars.spread = json["spread"]; }
    if (json.contains("seed")) { stars.seed = json["seed"]; }
    if (json.contains("depth")) { stars.zDepth = json["depth"]; }
}

void to_json(nlohmann::json& json, const MinimapConfig& minimap)
{
	json = {
		{"enabled", minimap.enabled},
		{"worldEndX", minimap.worldEndX},
		{"worldEndY", minimap.worldEndY},
		{"worldStartX", minimap.worldStartX},
		{"worldStartY", minimap.worldStartY},
		{"iconSizeX", minimap.iconSizeX},
		{"iconSizeY", minimap.iconSizeY},
		{"driverDotStartX", minimap.driverDotStartX},
		{"driverDotStartY", minimap.driverDotStartY},
		{"orientationMode", minimap.orientationMode},
		{"unk", minimap.unk},
		{"topTexturePath", minimap.topTexturePath.string()},
		{"bottomTexturePath", minimap.bottomTexturePath.string()}
	};
}

void from_json(const nlohmann::json& json, MinimapConfig& minimap)
{
	if (json.contains("enabled")) { json.at("enabled").get_to(minimap.enabled); }
	if (json.contains("worldEndX")) { json.at("worldEndX").get_to(minimap.worldEndX); }
	if (json.contains("worldEndY")) { json.at("worldEndY").get_to(minimap.worldEndY); }
	if (json.contains("worldStartX")) { json.at("worldStartX").get_to(minimap.worldStartX); }
	if (json.contains("worldStartY")) { json.at("worldStartY").get_to(minimap.worldStartY); }
	if (json.contains("iconSizeX")) { json.at("iconSizeX").get_to(minimap.iconSizeX); }
	if (json.contains("iconSizeY")) { json.at("iconSizeY").get_to(minimap.iconSizeY); }
	if (json.contains("driverDotStartX")) { json.at("driverDotStartX").get_to(minimap.driverDotStartX); }
	if (json.contains("driverDotStartY")) { json.at("driverDotStartY").get_to(minimap.driverDotStartY); }
	if (json.contains("orientationMode")) { json.at("orientationMode").get_to(minimap.orientationMode); }
	else if (json.contains("mode")) { json.at("mode").get_to(minimap.orientationMode); } // backwards compatibility
	if (json.contains("unk")) { json.at("unk").get_to(minimap.unk); }

	if (json.contains("topTexturePath"))
	{
		std::string path;
		json.at("topTexturePath").get_to(path);
		if (!path.empty()) { minimap.topTexturePath = path; }
	}

	if (json.contains("bottomTexturePath"))
	{
		std::string path;
		json.at("bottomTexturePath").get_to(path);
		if (!path.empty()) { minimap.bottomTexturePath = path; }
	}

	// Load textures after setting paths
	minimap.LoadTextures();
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
	json["color"] = m_color;

	if (m_left) { json["left"] = nlohmann::json(); m_left->ToJson(json["left"], quadblocks); }
	if (m_right) { json["right"] = nlohmann::json(); m_right->ToJson(json["right"], quadblocks); }
}

void Path::FromJson(const nlohmann::json& json, const std::vector<Quadblock>& quadblocks)
{
	std::vector<std::string> quadStart, quadEnd, quadIgnore;
	if (json.contains("color")) { json.at("color").get_to(m_color); }
	if (json.contains("index")) { json.at("index").get_to(m_index); }
	if (json.contains("hasLeft") && json.contains("left"))
	{
		bool hasLeft = false;
		json.at("hasLeft").get_to(hasLeft);
		if (hasLeft)
		{
			m_left = new Path();
			m_left->FromJson(json.at("left"), quadblocks);
		}
	}
	if (json.contains("hasRight") && json.contains("right"))
	{
		bool hasRight = false;
		json.at("hasRight").get_to(hasRight);
		if (hasRight)
		{
			m_right = new Path();
			m_right->FromJson(json.at("right"), quadblocks);
		}
	}
	if (json.contains("quadStart")) { json.at("quadStart").get_to(quadStart); }
	if (json.contains("quadEnd")) { json.at("quadEnd").get_to(quadEnd); }
	if (json.contains("quadIgnore")) { json.at("quadIgnore").get_to(quadIgnore); }

	std::unordered_map<std::string, size_t> quadNameMap;
	for (size_t i = 0; i < quadblocks.size(); i++) { quadNameMap[quadblocks[i].GetName()] = i; }
	for (const std::string& name : quadStart) { if (quadNameMap.contains(name)) { m_quadIndexesStart.push_back(quadNameMap[name]); } }
	for (const std::string& name : quadEnd) { if (quadNameMap.contains(name)) { m_quadIndexesEnd.push_back(quadNameMap[name]); } }
	for (const std::string& name : quadIgnore) { if (quadNameMap.contains(name)) { m_quadIndexesIgnore.push_back(quadNameMap[name]); } }
}

void AnimTexture::FromJson(const nlohmann::json& json, std::vector<Quadblock>& quadblocks, const std::filesystem::path& parentPath)
{
	if (!json.contains("path")) { return; }
	std::filesystem::path path = json["path"];
	if (!std::filesystem::exists(path))
	{
		path = parentPath / path.filename();
		if (!std::filesystem::exists(path)) { return; }
	}
	if (!ReadAnimation(path)) { ClearAnimation(); return; }

	m_path = path;
	if (json.contains("name")) { m_name = json["name"]; }
	if (json.contains("startAt")) { m_startAtFrame = json["startAt"]; }
	if (json.contains("duration")) { m_duration = json["duration"]; }
	if (json.contains("rotation")) { m_rotation = json["rotation"]; }
	if (json.contains("horMirror")) { m_horMirror = json["horMirror"]; }
	if (json.contains("verMirror")) { m_verMirror = json["verMirror"]; }

	if (m_horMirror) { MirrorFrames(true); }
	if (m_verMirror) { MirrorFrames(false); }
	RotateFrames(m_rotation);

	if (json.contains("blendModes"))
	{
		std::vector<uint16_t> blendModes = json["blendModes"];
		if (blendModes.size() == m_textures.size())
		{
			for (size_t i = 0; i < m_textures.size(); i++) { m_textures[i].SetBlendMode(blendModes[i]); }
		}
	}
	if (json.contains("quads"))
	{
		std::unordered_set<std::string> quadNames = json["quads"];
		for (size_t i = 0; i < quadblocks.size(); i++)
		{
			if (quadNames.contains(quadblocks[i].GetName()))
			{
				m_quadblockIndexes.push_back(i);
				quadblocks[i].SetAnimated(true);
			}
		}
	}
}

void AnimTexture::ToJson(nlohmann::json& json, const std::vector<Quadblock>& quadblocks) const
{
	json["path"] = m_path;
	json["name"] = m_name;
	json["startAt"] = m_startAtFrame;
	json["duration"] = m_duration;
	json["rotation"] = m_rotation;
	json["horMirror"] = m_horMirror;
	json["verMirror"] = m_verMirror;

	std::unordered_set<std::string> quadNames;
	for (size_t index : m_quadblockIndexes) { quadNames.insert(quadblocks[index].GetName()); }
	json["quads"] = quadNames;

	std::vector<uint16_t> blendModes;
	for (const Texture& tex : m_textures) { blendModes.push_back(tex.GetBlendMode()); }
	json["blendModes"] = blendModes;
}
