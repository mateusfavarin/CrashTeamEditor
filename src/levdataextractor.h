#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <string>

#include "psx_types.h"

class LevDataExtractor
{
public:
	LevDataExtractor(const std::filesystem::path& levPath, const std::filesystem::path& vrmPath);

	void ExtractModels(void);
	const std::string& GetLog() const { return m_log; }

	// Texture extraction helpers (public for use by free functions)
	static constexpr size_t VRAM_WIDTH = 1024;
	static constexpr size_t VRAM_HEIGHT = 512;

	struct VramBuffer {
		std::vector<uint16_t> data; // 1024x512 ushorts
		VramBuffer() : data(VRAM_WIDTH * VRAM_HEIGHT, 0) {}
	};

private:
	void Log(const char* format, ...);

	// VRAM operations
	void ParseVrmIntoVram(VramBuffer& vram);

	std::filesystem::path m_levPath;
	std::filesystem::path m_vrmPath;
	std::vector<uint8_t> m_levData;
	std::vector<uint8_t> m_vrmData;
	std::string m_log;
};

namespace SH
{
	struct WriteableObject
	{
		size_t size;
		void* data;
	};

	struct CtrModel
	{
		uint32_t modelOffset;
		uint32_t modelPatchTableOffset;
		uint32_t textureDataOffset; // Points to TextureSectionHeader, 0 if no textures
	};

	struct TextureSectionHeader
	{
		uint32_t numTextures;
		// Followed by: uint32_t textureOffsets[numTextures] (offsets relative to file start)
		// Followed by: TextureData for each texture (at the offsets specified above)
	};

	struct TextureDataHeader
	{
		uint16_t width;
		uint16_t height;
		uint8_t bpp;        // 0=4bit, 1=8bit, 2=16bit
		uint8_t blendMode;
		uint8_t originU;    // minU of combined texture in original UV space
		uint8_t originV;    // minV of combined texture in original UV space
		// Original VRAM coordinates (for matching TextureLayouts to textures at import)
		uint8_t origPageX;  // Original texpage X (0-15)
		uint8_t origPageY;  // Original texpage Y (0-1)
		uint8_t origPalX;   // Original CLUT X / 16 (0-63)
		uint8_t origPalY_lo; // Original CLUT Y low byte
		uint8_t origPalY_hi; // Original CLUT Y high byte (Y is 0-511)
		uint8_t padding;    // Align to 16 bytes total
		// Followed by:
		// - Pixel data in raw PSX format:
		//   4-bit: ceil(width/2) * height bytes (2 pixels per byte)
		//   8-bit: width * height bytes
		//   16-bit: width * height * 2 bytes
		// - Palette data (raw PSX 16-bit colors):
		//   4-bit: 16 * sizeof(uint16_t) = 32 bytes
		//   8-bit: 256 * sizeof(uint16_t) = 512 bytes
		//   16-bit: no palette
	};

	// Groups multiple TextureLayouts that share the same texpage+palette into a single combined texture for extraction
	struct TextureExtractionGroup
	{
		std::vector<size_t> layoutIndices;  // Indices of TextureLayouts in this group
		int minU = 255;
		int minV = 255;
		int maxU = 0;
		int maxV = 0;
		int pageX = 0;
		int pageY = 0;
		int palX = 0;
		int palY = 0;
		int bpp = 0;
		int blendMode = 0;
	};
}
