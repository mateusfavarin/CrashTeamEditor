#include "animtexture.h"
#include "level.h"

#include <unordered_map>
#include <unordered_set>

AnimTexture::AnimTexture(const std::filesystem::path& path, const std::vector<std::string>& usedNames)
{
	m_path = path;
	std::string origName = path.filename().replace_extension().string();
	m_name = origName;
	int repetitionCount = 1;
	while (true)
	{
		bool validName = true;
		for (const std::string& usedName : usedNames)
		{
			if (m_name == usedName) { validName = false; break; }
		}
		if (validName) { break; }
		m_name = origName + " (" + std::to_string(repetitionCount++) + ")";
	}
	ReadAnimation(path);
}

bool AnimTexture::Empty() const
{
	return m_frames.empty();
}

const std::vector<AnimTextureFrame>& AnimTexture::GetFrames() const
{
	return m_frames;
}

const std::vector<Texture>& AnimTexture::GetTextures() const
{
	return m_textures;
}

std::vector<uint8_t> AnimTexture::Serialize(size_t offsetFirstFrame, size_t offTextures) const
{
	std::vector<uint8_t> buffer(sizeof(PSX::AnimTex));
	PSX::AnimTex animTex = {};
	animTex.offActiveFrame = static_cast<uint32_t>(offTextures + (offsetFirstFrame * sizeof(PSX::TextureGroup)));
	animTex.frameCount = static_cast<uint16_t>(m_frames.size());
	animTex.startAtFrame = static_cast<int16_t>(m_startAtFrame);
	animTex.frameDuration = static_cast<int16_t>(m_duration);
	animTex.frameIndex = 0;
	memcpy(buffer.data(), &animTex, sizeof(PSX::AnimTex));
	return buffer;
}

const std::string& AnimTexture::GetName() const
{
	return m_name;
}

bool AnimTexture::IsPopulated() const
{
	return !Empty() && !m_quadblockIndexes.empty();
}

void AnimTexture::CopyParameters(const AnimTexture& animTex)
{
	m_startAtFrame = animTex.m_startAtFrame;
	m_duration = animTex.m_duration;
	m_rotation = animTex.m_rotation;
	m_textures = animTex.m_textures;
	m_frames = animTex.m_frames;
}

void AnimTexture::FromJson(const nlohmann::json& json, std::vector<Quadblock>& quadblocks)
{
	std::filesystem::path path = json["path"];
	ReadAnimation(path);
	m_name = json["name"];
	m_startAtFrame = json["startAt"];
	m_duration = json["duration"];
	m_rotation = json["rotation"];
	m_horMirror = json["horMirror"];
	m_verMirror = json["verMirror"];

	if (m_horMirror) { MirrorFrames(true); }
	if (m_verMirror) { MirrorFrames(false); }
	RotateFrames(m_rotation);

	std::vector<uint16_t> blendModes = json["blendModes"];
	if (blendModes.size() == m_textures.size())
	{
		for (size_t i = 0; i < m_textures.size(); i++) { m_textures[i].SetBlendMode(blendModes[i]); }
	}
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

const std::vector<size_t>& AnimTexture::GetQuadblockIndexes() const
{
	return m_quadblockIndexes;
}

bool AnimTexture::ReadAnimation(const std::filesystem::path& path)
{
	Level dummy;
	if (!dummy.Load(path)) { return false; }

	std::unordered_map<std::filesystem::path, size_t> loadedPaths;
	const std::vector<Quadblock>& quadblocks = dummy.GetQuadblocks();
	for (const Quadblock& quadblock : quadblocks)
	{
		const auto& uvs = quadblock.GetUVs();
		const std::filesystem::path& texPath = quadblock.GetTexPath();
		if (loadedPaths.contains(texPath)) { m_frames.emplace_back(loadedPaths.at(texPath), uvs); continue; }

		size_t index = m_textures.size();
		loadedPaths.insert({texPath, index});
		m_frames.emplace_back(index, uvs);
		m_textures.emplace_back(texPath);
	}
	m_startAtFrame = 0;
	m_duration = 0;
	m_rotation = 0;
	m_horMirror = false;
	m_verMirror = false;
	m_previewQuadIndex = 0;
	m_manualOrientation = false;
	return true;
}

void AnimTexture::MirrorQuadUV(bool horizontal, std::array<QuadUV, 5>& uvs)
{
	if (horizontal)
	{
		auto SwapHor = [](QuadUV& uv1, QuadUV& uv2)
			{
				Swap(uv1[0], uv2[1]);
				Swap(uv1[1], uv2[0]);
				Swap(uv1[2], uv2[3]);
				Swap(uv1[3], uv2[2]);
			};

		SwapHor(uvs[0], uvs[1]);
		SwapHor(uvs[2], uvs[3]);
		Swap(uvs[4][0], uvs[4][1]);
		Swap(uvs[4][2], uvs[4][3]);
	}
	else
	{
		auto SwapVer = [](QuadUV& uv1, QuadUV& uv2)
			{
				Swap(uv1[0], uv2[2]);
				Swap(uv1[2], uv2[0]);
				Swap(uv1[1], uv2[3]);
				Swap(uv1[3], uv2[1]);
			};

		SwapVer(uvs[0], uvs[2]);
		SwapVer(uvs[1], uvs[3]);
		Swap(uvs[4][0], uvs[4][2]);
		Swap(uvs[4][1], uvs[4][3]);
	}
}

void AnimTexture::RotateQuadUV(std::array<QuadUV, 5>& uvs)
{
	auto SwapQuadUV = [](QuadUV& dst, const QuadUV& tgt)
		{
			dst[0] = tgt[2];
			dst[1] = tgt[0];
			dst[2] = tgt[3];
			dst[3] = tgt[1];
		};

	QuadUV tmp = uvs[0];
	SwapQuadUV(uvs[0], uvs[2]);
	SwapQuadUV(uvs[2], uvs[3]);
	SwapQuadUV(uvs[3], uvs[1]);
	SwapQuadUV(uvs[1], tmp);

	Vec2 lowTmp = uvs[4][0];
	uvs[4][0] = uvs[4][2];
	uvs[4][2] = uvs[4][3];
	uvs[4][3] = uvs[4][1];
	uvs[4][1] = lowTmp;
}

void AnimTexture::MirrorFrames(bool horizontal)
{
	for (AnimTextureFrame& frame : m_frames)
	{
		MirrorQuadUV(horizontal, frame.uvs);
	}
}

void AnimTexture::RotateFrames(int targetRotation)
{
	if (targetRotation == 0) { return; }

	int rotTimes = targetRotation / 90;
	if (rotTimes < 0) { rotTimes = 4 - (rotTimes * -1); }
	for (AnimTextureFrame& frame : m_frames)
	{
		for (int i = 0; i < rotTimes; i++) { RotateQuadUV(frame.uvs); }
	}
}
