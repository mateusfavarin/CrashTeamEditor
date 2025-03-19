#pragma once

#include "psx_types.h"
#include "quadblock.h"

#include <cstdint>
#include <vector>
#include <filesystem>

class Texture
{
public:
	enum class BPP
	{
		BPP_4, BPP_8, BPP_16
	};
	Texture() : m_width(0), m_height(0), m_imageX(0), m_imageY(0), m_clutX(0), m_clutY(0), m_blendMode(0) {};
	Texture(const std::filesystem::path& path);
	Texture::BPP GetBPP() const;
	int GetWidth() const;
	int GetVRAMWidth() const;
	int GetHeight() const;
	uint16_t GetBlendMode() const;
	const std::filesystem::path& GetPath() const;
	bool Empty() const;
	const std::vector<uint16_t>& GetImage() const;
	const std::vector<uint16_t>& GetClut() const;
	void SetImageCoords(size_t x, size_t y);
	void SetCLUTCoords(size_t x, size_t y);
	void SetBlendMode(size_t mode);
	PSX::TextureLayout Serialize(const QuadUV& uvs, bool lowLOD);

private:
	void ClearTexture();
	void CreateTexture();
	uint16_t ConvertColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	void ConvertPixels(const std::vector<size_t> colorIndexes, unsigned indexesPerPixel);

private:
	int m_width, m_height;
	uint16_t m_blendMode;
	size_t m_imageX, m_imageY;
	size_t m_clutX, m_clutY;
	std::vector<uint16_t> m_image;
	std::vector<uint16_t> m_clut;
	std::filesystem::path m_path;
};

std::vector<uint8_t> PackVRM(std::vector<Texture*>& textures);