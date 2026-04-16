#pragma once

#include "psx_types.h"
#include "quadblock.h"

#include <cstdint>
#include <vector>
#include <filesystem>
#include <unordered_set>
#include <functional>

typedef std::unordered_set<size_t> Shape;


//Helper structs for reading texture from .lev/.vrm. Maybe move to psx_types.h ?
struct RawUV {
	uint8_t u0, v0, u1, v1, u2, v2, u3, v3;

	RawUV() = default;

	RawUV(const PSX::TextureLayout& layout)
		: u0(layout.u0), v0(layout.v0)
		, u1(layout.u1), v1(layout.v1)
		, u2(layout.u2), v2(layout.v2)
		, u3(layout.u3), v3(layout.v3)
	{
	}

	RawUV(const PSX::TextureLayout& layout, uint32_t drawOrderLow, int f)
		: RawUV(layout)
	{
		auto SwapUV = [](uint8_t& u1, uint8_t& v1, uint8_t& u2, uint8_t& v2) {
			uint8_t tmpU = u1, tmpV = v1;
			u1 = u2; v1 = v2;
			u2 = tmpU; v2 = tmpV;
			};
		auto Rotate90 = [&](RawUV& u) {
			uint8_t tmp_u = u.u0, tmp_v = u.v0;
			u.u0 = u.u2; u.v0 = u.v2;
			u.u2 = u.u3; u.v2 = u.v3;
			u.u3 = u.u1; u.v3 = u.v1;
			u.u1 = tmp_u; u.v1 = tmp_v;
			};
		auto Flip = [&](RawUV& u) {
			SwapUV(u.u0, u.v0, u.u1, u.v1);
			SwapUV(u.u2, u.v2, u.u3, u.v3);
			};

		uint32_t shift = 8 + (f * 5);
		uint32_t rotateFlip = (drawOrderLow >> shift) & 0x7;

		switch (rotateFlip)
		{
		case 1: Rotate90(*this); break;
		case 2: Rotate90(*this); Rotate90(*this); break;
		case 3: Rotate90(*this); Rotate90(*this); Rotate90(*this); break;
		case 4: Flip(*this); Rotate90(*this); Rotate90(*this); Rotate90(*this); break;
		case 5: Flip(*this); Rotate90(*this); Rotate90(*this); break;
		case 6: Flip(*this); Rotate90(*this); break;
		case 7: Flip(*this); break;
		default: break;
		}
	}
};



struct PixelBounds
{
	uint8_t minU = 255, minV = 255;
	uint8_t maxU = 0, maxV = 0;

	void Update(const RawUV& uvs)
	{
		if (uvs.u0 < minU) minU = uvs.u0;
		if (uvs.u1 < minU) minU = uvs.u1;
		if (uvs.u2 < minU) minU = uvs.u2;
		if (uvs.u3 < minU) minU = uvs.u3;

		if (uvs.v0 < minV) minV = uvs.v0;
		if (uvs.v1 < minV) minV = uvs.v1;
		if (uvs.v2 < minV) minV = uvs.v2;
		if (uvs.v3 < minV) minV = uvs.v3;

		if (uvs.u0 > maxU) maxU = uvs.u0;
		if (uvs.u1 > maxU) maxU = uvs.u1;
		if (uvs.u2 > maxU) maxU = uvs.u2;
		if (uvs.u3 > maxU) maxU = uvs.u3;

		if (uvs.v0 > maxV) maxV = uvs.v0;
		if (uvs.v1 > maxV) maxV = uvs.v1;
		if (uvs.v2 > maxV) maxV = uvs.v2;
		if (uvs.v3 > maxV) maxV = uvs.v3;
	}
};

struct LayoutKey // 2 PSX::TextureLayout have the same LayoutKey if they use the same vram page and colors. Roughly correspond to materials
{
	uint16_t pageX;
	uint16_t pageY;
	uint16_t bpp;
	uint16_t clutX;
	uint16_t clutY;
	uint16_t blendMode;

	LayoutKey() = default;

	LayoutKey(const PSX::TextureLayout& layout)
		: pageX(layout.texPage.x)
		, pageY(layout.texPage.y)
		, bpp(layout.texPage.texpageColors)
		, clutX(layout.clut.x)
		, clutY(layout.clut.y)
		, blendMode(layout.texPage.blendMode)
	{
	}

	bool operator==(const LayoutKey& other) const
	{
		return pageX == other.pageX &&
			pageY == other.pageY &&
			bpp == other.bpp &&
			clutX == other.clutX &&
			clutY == other.clutY &&
			blendMode == other.blendMode;
	}
};

namespace std
{
	template<>
	struct hash<LayoutKey>
	{
		size_t operator()(const LayoutKey& key) const
		{
			size_t h1 = std::hash<uint16_t>{}(key.pageX);
			size_t h2 = std::hash<uint16_t>{}(key.pageY);
			size_t h3 = std::hash<uint16_t>{}(key.bpp);
			size_t h4 = std::hash<uint16_t>{}(key.clutX);
			size_t h5 = std::hash<uint16_t>{}(key.clutY);
			size_t h6 = std::hash<uint16_t>{}(key.blendMode);

			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
		}
	};
}





class Texture
{
public:
	enum class BPP
	{
		BPP_4, BPP_8, BPP_16
	};
	Texture() : m_width(0), m_height(0), m_imageX(0), m_imageY(0), m_clutX(0), m_clutY(0), m_blendMode(0), m_semiTransparent(false) {};
	Texture(const std::filesystem::path& path);
	Texture(const LayoutKey& key, const PixelBounds& bounds, const std::vector<uint16_t>& vram, const std::string& newMatName, const std::filesystem::path& tempDir, bool crop = true);
	void UpdateTexture(const std::filesystem::path& path);
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
	bool IsSemiTransparent() const;
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
	bool m_semiTransparent;
	std::vector<uint16_t> m_image;
	std::vector<uint16_t> m_clut;
	std::vector<Shape> m_shapes;
	std::filesystem::path m_path;
};

std::vector<uint8_t> PackVRM(std::vector<Texture*>& textures);
