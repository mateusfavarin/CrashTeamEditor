#pragma once

#include "psx_types.h"
#include "quadblock.h"

#include <cstdint>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include <functional>

typedef std::unordered_set<size_t> Shape;

class Texture
{
public:
	enum class BPP
	{
		BPP_4, BPP_8, BPP_16
	};
	Texture() : m_width(0), m_height(0), m_imageX(0), m_imageY(0), m_clutX(0), m_clutY(0), m_blendMode(0) {};
	Texture(const std::filesystem::path& path);
	void UpdateTexture(const std::filesystem::path& path);
	// Create texture from raw RGBA pixel data (for minimap processing)
	static Texture CreateFromPixelData(const unsigned char* pixels, int width, int height);
	Texture::BPP GetBPP() const;
	int GetWidth() const;
	int GetVRAMWidth() const;
	int GetHeight() const;
	uint16_t GetBlendMode() const;
	const std::filesystem::path& GetPath() const;
	bool IsEmpty() const;
	const std::vector<uint16_t>& GetImage() const;
	const std::vector<uint16_t>& GetClut() const;
	size_t GetImageX() const;
	size_t GetImageY() const;
	size_t GetCLUTX() const;
	size_t GetCLUTY() const;
	void SetImageCoords(size_t x, size_t y);
	void SetCLUTCoords(size_t x, size_t y);
	void SetBlendMode(uint16_t mode);
	PSX::TextureLayout Serialize(const QuadUV& uvs) const;
	bool CompareEquivalency(const Texture& tex);
	void CopyVRAMAttributes(const Texture& tex);
	bool operator==(const Texture& tex) const;
	bool operator!=(const Texture& tex) const;
	void RenderUI(const std::vector<size_t>& quadblockIndexes, std::vector<Quadblock>& quadblocks, std::function<void(void)> refreshTextureStores);
	void RenderUI();

private:
	void FillShapes(const std::vector<size_t>& colorIndexes);
	void ClearTexture();
	bool CreateTexture();
	uint16_t ConvertColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	void ConvertPixels(const std::vector<size_t>& colorIndexes, unsigned indexesPerPixel);

private:
	int m_width, m_height;
	uint16_t m_blendMode;
	size_t m_imageX, m_imageY;
	size_t m_clutX, m_clutY;
	std::vector<uint16_t> m_image;
	std::vector<uint16_t> m_clut;
	std::vector<Shape> m_shapes;
	std::filesystem::path m_path;
};

std::vector<uint8_t> PackVRM(std::vector<Texture*>& textures);
