#pragma once

#include "quadblock.h"
#include "texture.h"
#include "psx_types.h"

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

struct AnimTextureFrame
{
	size_t textureIndex;
	std::array<QuadUV, 5> uvs;
};

class AnimTexture
{
public:
	AnimTexture() {};
	AnimTexture(const std::filesystem::path& path, const std::vector<std::string>& usedNames);
	bool IsEmpty() const;
	bool IsTriblock() const;
	const std::vector<AnimTextureFrame>& GetFrames() const;
	const std::vector<Texture>& GetTextures() const;
	const std::vector<size_t>& GetQuadblockIndexes() const;
	bool AdvanceRender(float deltaTime);
	const AnimTextureFrame& GetRenderFrame() const;
	std::vector<uint8_t> Serialize(size_t offsetFirstFrame, size_t offTextures) const;
	const std::string& GetName() const;
	bool IsPopulated() const;
	void AddQuadblockIndex(size_t index);
	void CopyParameters(const AnimTexture& animTex);
	void FromJson(const nlohmann::json& json, std::vector<Quadblock>& quadblocks, const std::filesystem::path& parentPath);
	void ToJson(nlohmann::json& json, const std::vector<Quadblock>& quadblocks) const;
	bool IsEquivalent(const AnimTexture& animTex) const;
	bool RenderUI(std::vector<std::string>& animTexNames, std::vector<Quadblock>& quadblocks, const std::unordered_map<std::string, std::vector<size_t>>& materialMap, const std::string& query, std::vector<AnimTexture>& newTextures);

private:
	bool ReadAnimation(const std::filesystem::path& path);
	void ClearAnimation();
	void SetDefaultParams();
	void MirrorQuadUV(bool horizontal, std::array<QuadUV, 5>& uvs);
	void RotateQuadUV(std::array<QuadUV, 5>& uvs);
	void MirrorFrames(bool horizontal);
	void RotateFrames(int targetRotation);
	void ResetRenderState();

private:
	bool m_triblock;
	bool m_manualOrientation;
	std::string m_name;
	std::filesystem::path m_path;
	std::vector<AnimTextureFrame> m_frames;
	std::vector<Texture> m_textures;
	std::vector<size_t> m_quadblockIndexes;

	int m_startAtFrame;
	int m_duration;
	int m_rotation;
	bool m_horMirror;
	bool m_verMirror;

	bool m_renderDirty;
	size_t m_renderFrameIndex;
	float m_renderFrameTimer;

	std::string m_previewQuadName;
	size_t m_previewQuadIndex;
	std::string m_previewMaterialName;
	std::string m_lastAppliedMaterialName;
};
