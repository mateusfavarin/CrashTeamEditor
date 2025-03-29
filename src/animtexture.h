#pragma once

#include "quadblock.h"
#include "texture.h"
#include "psx_types.h"

#include <string>
#include <filesystem>

struct AnimTextureFrame
{
	size_t textureIndex;
	std::array<QuadUV, 5> uvs;
};

class AnimTexture
{
public:
	AnimTexture(const std::filesystem::path& path, const std::vector<std::string>& usedNames);
	bool Empty() const;
	const std::vector<AnimTextureFrame>& GetFrames() const;
	const std::vector<Texture>& GetTextures() const;
	const std::vector<size_t>& GetQuadblockIndexes() const;
	std::vector<uint8_t> Serialize(size_t offsetFirstFrame, size_t offTextures) const;
	const std::string& GetName() const;
	bool RenderUI(const std::vector<Quadblock>& quadblocks, const std::unordered_map<std::string, std::vector<size_t>>& materialMap, const std::string& query);

private:
	void RotateQuadUV(std::array<QuadUV, 5>& uvs);
	void RotateFrames(int targetRotation);

private:
	std::string m_name;
	std::vector<AnimTextureFrame> m_frames;
	std::vector<Texture> m_textures;
	std::vector<size_t> m_quadblockIndexes;

	int m_startAtFrame;
	int m_duration;
	int m_rotation;

	std::string m_previewQuadName;
	size_t m_previewQuadIndex;
	std::string m_previewMaterialName;
	std::string m_lastAppliedMaterialName;
};