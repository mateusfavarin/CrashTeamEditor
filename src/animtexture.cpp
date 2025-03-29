#include "animtexture.h"
#include "level.h"

#include <unordered_map>

AnimTexture::AnimTexture(const std::filesystem::path& path, const std::vector<std::string>& usedNames)
{
	Level dummy;
	if (!dummy.Load(path)) { return; }

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
	m_previewQuadIndex = 0;
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

const std::vector<size_t>& AnimTexture::GetQuadblockIndexes() const
{
	return m_quadblockIndexes;
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
