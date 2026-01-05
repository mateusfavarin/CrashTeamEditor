#define _CRT_SECURE_NO_WARNINGS
#include "levdataextractor.h"
#include "psx_types.h"
#include <fstream>
#include <iterator>
#include <cstring>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

void LevDataExtractor::Log(const char* format, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	m_log += buffer;
}

// PSX color conversion (Duckstation formula)
uint8_t LevDataExtractor::Convert5To8(uint8_t value)
{
	return static_cast<uint8_t>(((value * 527u) + 23u) >> 6);
}

LevDataExtractor::RGBA8 LevDataExtractor::Convert16ToRGBA(uint16_t color)
{
	RGBA8 result;
	result.r = Convert5To8((color >> 0) & 0x1F);
	result.g = Convert5To8((color >> 5) & 0x1F);
	result.b = Convert5To8((color >> 10) & 0x1F);

	// Alpha handling: color 0 is transparent, STP bit affects alpha
	if (color == 0)
	{
		result.a = 0;
	}
	else
	{
		uint8_t stp = (color >> 15) & 0x1;
		result.a = stp ? 127 : 255;
	}

	return result;
}

void LevDataExtractor::ParseVrmIntoVram(VramBuffer& vram)
{
	if (m_vrmData.empty())
	{
		Log("%sWARNING%s: VRM data is empty, cannot parse textures\n", ANSI_YELLOW, ANSI_RESET);
		return;
	}

	size_t offset = 0;

	// Check for multi-TIM format (starts with 0x20 magic)
	uint32_t magic = *reinterpret_cast<const uint32_t*>(m_vrmData.data());
	if (magic == 0x20)
	{
		offset = 4; // Skip magic

		// CTR VRM files can have up to 2 TIM pages
		for (int page = 0; page < 2 && offset < m_vrmData.size(); page++)
		{
			if (offset + 4 > m_vrmData.size()) break;

			uint32_t timSize = *reinterpret_cast<const uint32_t*>(m_vrmData.data() + offset);
			offset += 4;

			if (offset + timSize > m_vrmData.size())
			{
				Log("%sWARNING%s: TIM size exceeds VRM file bounds\n", ANSI_YELLOW, ANSI_RESET);
				break;
			}

			// Parse TIM header
			if (offset + 8 > m_vrmData.size()) break;
			uint32_t timMagic = *reinterpret_cast<const uint32_t*>(m_vrmData.data() + offset);
			if (timMagic != 0x10)
			{
				Log("%sWARNING%s: Invalid TIM magic: 0x%08X\n", ANSI_YELLOW, ANSI_RESET, timMagic);
				offset += timSize - 4;
				continue;
			}

			offset += 4; // Skip TIM magic
			uint32_t flags = *reinterpret_cast<const uint32_t*>(m_vrmData.data() + offset);
			offset += 4;

			// Skip CLUT if present (bit 3 of flags)
			if (flags & 0x8)
			{
				if (offset + 12 > m_vrmData.size()) break;
				uint32_t clutSize = *reinterpret_cast<const uint32_t*>(m_vrmData.data() + offset);
				offset += clutSize;
			}

			// Read image data
			if (offset + 12 > m_vrmData.size()) break;
			uint32_t imageSize = *reinterpret_cast<const uint32_t*>(m_vrmData.data() + offset);
			offset += 4;

			uint16_t regionX = *reinterpret_cast<const uint16_t*>(m_vrmData.data() + offset);
			offset += 2;
			uint16_t regionY = *reinterpret_cast<const uint16_t*>(m_vrmData.data() + offset);
			offset += 2;
			uint16_t regionW = *reinterpret_cast<const uint16_t*>(m_vrmData.data() + offset);
			offset += 2;
			uint16_t regionH = *reinterpret_cast<const uint16_t*>(m_vrmData.data() + offset);
			offset += 2;

			uint32_t pixelDataSize = imageSize - 12;
			if (offset + pixelDataSize > m_vrmData.size()) break;

			// Copy pixel data into VRAM at specified region
			const uint16_t* pixelData = reinterpret_cast<const uint16_t*>(m_vrmData.data() + offset);
			size_t pixelCount = pixelDataSize / 2;

			for (size_t i = 0; i < pixelCount && i < regionW * regionH; i++)
			{
				size_t x = regionX + (i % regionW);
				size_t y = regionY + (i / regionW);

				if (x < VRAM_WIDTH && y < VRAM_HEIGHT)
				{
					vram.data[y * VRAM_WIDTH + x] = pixelData[i];
				}
			}

			offset += pixelDataSize;
		}

		Log("  Parsed VRM file with multi-TIM format\n");
	}
	else
	{
		Log("%sWARNING%s: VRM format not recognized (magic: 0x%08X)\n", ANSI_YELLOW, ANSI_RESET, magic);
	}
}

void LevDataExtractor::ExtractTexture(const VramBuffer& vram, const PSX::TextureLayout& layout,
                                      int width, int height, std::vector<RGBA8>& outPixels)
{
	// Calculate texture position in VRAM
	int pageX = layout.texPage.x;
	int pageY = layout.texPage.y;
	int bpp = layout.texPage.texpageColors;
					int blendMode = layout.texPage.blendMode; // 0=4bit, 1=8bit, 2=16bit

	// Determine stretch factor based on bit depth
	int stretch = (bpp == 0) ? 4 : (bpp == 1) ? 2 : 1;

	// Get UV bounds
	int minU = std::min({layout.u0, layout.u1, layout.u2, layout.u3});
	int minV = std::min({layout.v0, layout.v1, layout.v2, layout.v3});

	// Calculate real position in VRAM
	int realX = pageX * 64 + minU / stretch;
	int realY = pageY * 256 + minV;

	outPixels.resize(width * height);

	if (bpp == 2) // 16-bit direct color
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int vramX = realX + x;
				int vramY = realY + y;

				if (vramX < VRAM_WIDTH && vramY < VRAM_HEIGHT)
				{
					uint16_t color = vram.data[vramY * VRAM_WIDTH + vramX];
					outPixels[y * width + x] = Convert16ToRGBA(color);
				}
			}
		}
	}
	else // Indexed color (4-bit or 8-bit)
	{
		// Get palette position
		int palX = layout.clut.x * 16;
		int palY = layout.clut.y;
		int palSize = (bpp == 0) ? 16 : 256;

		// Extract palette
		std::vector<uint16_t> palette(palSize);
		for (int i = 0; i < palSize; i++)
		{
			int px = palX + i;
			if (px < VRAM_WIDTH && palY < VRAM_HEIGHT)
			{
				palette[i] = vram.data[palY * VRAM_WIDTH + px];
			}
		}

		// Extract and decode pixels
		if (bpp == 0) // 4-bit
		{
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int vramX = realX + x / 4;
					int vramY = realY + y;

					if (vramX < VRAM_WIDTH && vramY < VRAM_HEIGHT)
					{
						uint16_t packed = vram.data[vramY * VRAM_WIDTH + vramX];
						int shift = (x % 4) * 4;
						int index = (packed >> shift) & 0xF;
						outPixels[y * width + x] = Convert16ToRGBA(palette[index]);
					}
				}
			}
		}
		else if (bpp == 1) // 8-bit
		{
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int vramX = realX + x / 2;
					int vramY = realY + y;

					if (vramX < VRAM_WIDTH && vramY < VRAM_HEIGHT)
					{
						uint16_t packed = vram.data[vramY * VRAM_WIDTH + vramX];
						int shift = (x % 2) * 8;
						int index = (packed >> shift) & 0xFF;
						outPixels[y * width + x] = Convert16ToRGBA(palette[index]);
					}
				}
			}
		}
	}
}

void LevDataExtractor::ExtractTextureFromOrigin(const VramBuffer& vram, const PSX::TextureLayout& layout,
                                                int width, int height, std::vector<RGBA8>& outPixels)
{
	// Calculate texture position in VRAM (from origin 0,0 of the texpage)
	int pageX = layout.texPage.x;
	int pageY = layout.texPage.y;
	int bpp = layout.texPage.texpageColors;
					int blendMode = layout.texPage.blendMode; // 0=4bit, 1=8bit, 2=16bit

	// Real position is just the texpage origin
	int realX = pageX * 64;
	int realY = pageY * 256;

	outPixels.resize(width * height);

	if (bpp == 2) // 16-bit direct color
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int vramX = realX + x;
				int vramY = realY + y;

				if (vramX >= 0 && vramX < (int)VRAM_WIDTH && vramY >= 0 && vramY < (int)VRAM_HEIGHT)
				{
					uint16_t color = vram.data[vramY * VRAM_WIDTH + vramX];
					outPixels[y * width + x] = Convert16ToRGBA(color);
				}
			}
		}
	}
	else // Indexed color (4-bit or 8-bit)
	{
		// Get palette position
		int palX = layout.clut.x * 16;
		int palY = layout.clut.y;
		int palSize = (bpp == 0) ? 16 : 256;

		// Extract palette
		std::vector<uint16_t> palette(palSize);
		for (int i = 0; i < palSize; i++)
		{
			int px = palX + i;
			if (px >= 0 && px < (int)VRAM_WIDTH && palY >= 0 && palY < (int)VRAM_HEIGHT)
			{
				palette[i] = vram.data[palY * VRAM_WIDTH + px];
			}
		}

		// Extract and decode pixels
		if (bpp == 0) // 4-bit
		{
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int vramX = realX + x / 4;
					int vramY = realY + y;

					if (vramX >= 0 && vramX < (int)VRAM_WIDTH && vramY >= 0 && vramY < (int)VRAM_HEIGHT)
					{
						uint16_t packed = vram.data[vramY * VRAM_WIDTH + vramX];
						int shift = (x % 4) * 4;
						int index = (packed >> shift) & 0xF;
						outPixels[y * width + x] = Convert16ToRGBA(palette[index]);
					}
				}
			}
		}
		else if (bpp == 1) // 8-bit
		{
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int vramX = realX + x / 2;
					int vramY = realY + y;

					if (vramX >= 0 && vramX < (int)VRAM_WIDTH && vramY >= 0 && vramY < (int)VRAM_HEIGHT)
					{
						uint16_t packed = vram.data[vramY * VRAM_WIDTH + vramX];
						int shift = (x % 2) * 8;
						int index = (packed >> shift) & 0xFF;
						outPixels[y * width + x] = Convert16ToRGBA(palette[index]);
					}
				}
			}
		}
	}
}

// Helper struct for collecting raw texture data
struct RawTextureData {
	std::vector<uint8_t> pixelData;   // Raw PSX format pixel indices
	std::vector<uint16_t> palette;    // Raw PSX 16-bit palette (empty for 16-bit textures)
	uint16_t width;
	uint16_t height;
	uint8_t bpp;        // 0=4bit, 1=8bit, 2=16bit
	uint8_t blendMode;
	uint8_t originU;    // minU of combined texture
	uint8_t originV;    // minV of combined texture
	// Original VRAM coordinates for matching at import
	uint8_t origPageX;
	uint8_t origPageY;
	uint8_t origPalX;
	uint16_t origPalY;
	std::string key;    // Grouping key for mapping TextureLayouts to this texture
};

// Extract raw PSX texture data (pixel indices + palette) without converting to RGBA
static void ExtractRawPSXTexture(const LevDataExtractor::VramBuffer& vram,
                                  int pageX, int pageY, int palX, int palY,
                                  int bpp, int blendMode, int minU, int minV, int width, int height,
                                  RawTextureData& outData)
{
	outData.width = static_cast<uint16_t>(width);
	outData.height = static_cast<uint16_t>(height);
	outData.bpp = static_cast<uint8_t>(bpp);
	outData.blendMode = static_cast<uint8_t>(blendMode);
	outData.originU = static_cast<uint8_t>(minU);
	outData.originV = static_cast<uint8_t>(minV);
	// Store original VRAM coordinates for matching at import
	outData.origPageX = static_cast<uint8_t>(pageX);
	outData.origPageY = static_cast<uint8_t>(pageY);
	outData.origPalX = static_cast<uint8_t>(palX);
	outData.origPalY = static_cast<uint16_t>(palY);

	// Determine stretch factor based on bit depth
	int stretch = (bpp == 0) ? 4 : (bpp == 1) ? 2 : 1;

	// Calculate real position in VRAM
	int realX = pageX * 64 + minU / stretch;
	int realY = pageY * 256 + minV;

	if (bpp == 2) // 16-bit direct color - store raw uint16 pixels
	{
		outData.pixelData.resize(width * height * 2);
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int vramX = realX + x;
				int vramY = realY + y;
				uint16_t color = 0;
				if (vramX >= 0 && vramX < (int)LevDataExtractor::VRAM_WIDTH &&
				    vramY >= 0 && vramY < (int)LevDataExtractor::VRAM_HEIGHT)
				{
					color = vram.data[vramY * LevDataExtractor::VRAM_WIDTH + vramX];
				}
				// Store as little-endian uint16
				size_t idx = (y * width + x) * 2;
				outData.pixelData[idx] = color & 0xFF;
				outData.pixelData[idx + 1] = (color >> 8) & 0xFF;
			}
		}
		// No palette for 16-bit
		outData.palette.clear();
	}
	else // Indexed color (4-bit or 8-bit)
	{
		// Extract palette
		int clutX = palX * 16;
		int clutY = palY;
		int palSize = (bpp == 0) ? 16 : 256;
		outData.palette.resize(palSize);
		for (int i = 0; i < palSize; i++)
		{
			int px = clutX + i;
			if (px >= 0 && px < (int)LevDataExtractor::VRAM_WIDTH &&
			    clutY >= 0 && clutY < (int)LevDataExtractor::VRAM_HEIGHT)
			{
				outData.palette[i] = vram.data[clutY * LevDataExtractor::VRAM_WIDTH + px];
			}
			else
			{
				outData.palette[i] = 0;
			}
		}

		if (bpp == 0) // 4-bit - pack 2 pixels per byte
		{
			// Each row: ceil(width/2) bytes
			size_t rowBytes = (width + 1) / 2;
			outData.pixelData.resize(rowBytes * height, 0);

			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int vramX = realX + x / 4;
					int vramY = realY + y;
					uint8_t index = 0;
					if (vramX >= 0 && vramX < (int)LevDataExtractor::VRAM_WIDTH &&
					    vramY >= 0 && vramY < (int)LevDataExtractor::VRAM_HEIGHT)
					{
						uint16_t packed = vram.data[vramY * LevDataExtractor::VRAM_WIDTH + vramX];
						int shift = (x % 4) * 4;
						index = (packed >> shift) & 0xF;
					}
					// Pack 2 pixels per byte: low nibble = even pixel, high nibble = odd pixel
					size_t byteIdx = y * rowBytes + (x / 2);
					if (x % 2 == 0)
						outData.pixelData[byteIdx] = (outData.pixelData[byteIdx] & 0xF0) | index;
					else
						outData.pixelData[byteIdx] = (outData.pixelData[byteIdx] & 0x0F) | (index << 4);
				}
			}
		}
		else if (bpp == 1) // 8-bit - 1 byte per pixel
		{
			outData.pixelData.resize(width * height);
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					int vramX = realX + x / 2;
					int vramY = realY + y;
					uint8_t index = 0;
					if (vramX >= 0 && vramX < (int)LevDataExtractor::VRAM_WIDTH &&
					    vramY >= 0 && vramY < (int)LevDataExtractor::VRAM_HEIGHT)
					{
						uint16_t packed = vram.data[vramY * LevDataExtractor::VRAM_WIDTH + vramX];
						int shift = (x % 2) * 8;
						index = (packed >> shift) & 0xFF;
					}
					outData.pixelData[y * width + x] = index;
				}
			}
		}
	}
}

LevDataExtractor::LevDataExtractor(const std::filesystem::path& levPath, const std::filesystem::path& vrmPath)
	: m_levPath(levPath), m_vrmPath(vrmPath)
{
	// Read LEV file into m_levData
	std::ifstream levFile(m_levPath, std::ios::binary);
	if (levFile)
	{
		m_levData = std::vector<uint8_t>(
			std::istreambuf_iterator<char>(levFile),
			std::istreambuf_iterator<char>()
		);
		levFile.close();
	}

	// Read VRM file into m_vrmData
	std::ifstream vrmFile(m_vrmPath, std::ios::binary);
	if (vrmFile)
	{
		m_vrmData = std::vector<uint8_t>(
			std::istreambuf_iterator<char>(vrmFile),
			std::istreambuf_iterator<char>()
		);
		vrmFile.close();
	}
}

void LevDataExtractor::ExtractModels(void)
{
	#define nameof(x) #x

	Log("Extracting models from LEV: %s\n", m_levPath.string().c_str());

	// Create output directory for extracted models
	std::filesystem::path outputDir = m_levPath.parent_path() / "extracted_models";
	std::filesystem::create_directories(outputDir);
	Log("Output directory: %s\n", outputDir.string().c_str());

	auto Deref = [this](uint32_t offset) -> const uint8_t* {
    return m_levData.data() + offset + 4; //offsets are from start of file + 4
	};

	const PSX::LevHeader& levHeader = *reinterpret_cast<const PSX::LevHeader*>(m_levData.data() + 4);

	// Parse VRM file into VRAM buffer for texture extraction
	VramBuffer vram;
	Log("\nParsing VRM file for textures...\n");
	ParseVrmIntoVram(vram);

	const uint32_t* modelArray = reinterpret_cast<const uint32_t*>(Deref(levHeader.offModels));
	Log("\nFound %u models in LEV file\n\n", levHeader.numModels);

	for (size_t modelIndex = 0; modelIndex < levHeader.numModels; modelIndex++)
	{
		const PSX::Model& model = *reinterpret_cast<const PSX::Model*>(Deref(modelArray[modelIndex]));
		//if (memcmp(model.name, "cactus_saguro", 13) != 0)
		//	continue;
		Log("========================================\n");
		Log("Model %zu/%u: %s\n", modelIndex + 1, levHeader.numModels, model.name);
		Log("  ID: %d\n", model.id);
		Log("  Headers: %u\n", model.numHeaders);

		if (model.numHeaders == 0 || model.offHeaders == (uint32_t)nullptr)
		{
			Log("  Model has no headers, skipping extraction...\n");
      continue;
		}

    std::vector<SH::WriteableObject> modelDataChunks{};
		size_t currentOffset = 0;

		std::filesystem::path outputFilePath = outputDir / (std::string(model.name) + ".ctrmodel");
		std::ofstream outputModelFile(outputFilePath, std::ios::binary);
		if (!outputModelFile)
		{
			Log("  %sERROR%s: Failed to create output file: %s\n", ANSI_RED, ANSI_RESET, outputFilePath.string().c_str());
			continue;
		}

    bool isSupportedByCurrentTechnology = true; //we don't currently support anything with animations.

		// Model-level texture collection (shared across all ModelHeaders)
		std::vector<RawTextureData> modelTextures;
		std::unordered_map<std::string, size_t> keyToTextureIndex;
		// Per-ModelHeader: maps TextureLayout index -> texture offset in file (filled after texture section is built)
		std::vector<std::vector<uint32_t>> perHeaderTexLayoutToTextureOffset;

    const size_t ctrModelOffset = currentOffset;
		Log("  " nameof(ctrModelOffset) " = 0x%zx\n", ctrModelOffset);
		SH::CtrModel* output_CtrModel = reinterpret_cast<SH::CtrModel*>(malloc(sizeof(SH::CtrModel)));
		currentOffset += sizeof(*output_CtrModel);
    modelDataChunks.push_back({ sizeof(*output_CtrModel), output_CtrModel });

		const size_t modelOffset = currentOffset;
		Log("  " nameof(modelOffset) " = 0x%zx\n", modelOffset);
		PSX::Model* output_Model = reinterpret_cast<PSX::Model*>(malloc(sizeof(PSX::Model)));
    currentOffset += sizeof(*output_Model);
		modelDataChunks.push_back({ sizeof(*output_Model), output_Model });

		memcpy(output_Model, &model, sizeof(model));
		output_Model->offHeaders = 0; //patch later

		//model.name --- done
		//model.id --- done
    //model.numHeaders --- done
    //model.offHeaders --- done

		// Pre-allocate contiguous block for ALL ModelHeaders
		const size_t allHeadersSize = model.numHeaders * sizeof(PSX::ModelHeader);
		const size_t allHeadersOffset = currentOffset;
		Log("  " nameof(allHeadersOffset) " = 0x%zx (size = 0x%zx, %zu headers)\n",
		       allHeadersOffset, allHeadersSize, model.numHeaders);
		PSX::ModelHeader* output_AllModelHeaders =
		    reinterpret_cast<PSX::ModelHeader*>(malloc(allHeadersSize));
		currentOffset += allHeadersSize;
		modelDataChunks.push_back({ allHeadersSize, output_AllModelHeaders });

		// Generate patch table - track all pointer offsets for SaveLEV to patch when embedding
		#define CALCULATE_OFFSET(s, m, b) static_cast<uint32_t>(offsetof(s, m) + b)

		std::vector<uint32_t> patchTable;

		// Add offset of Model.offHeaders field
		patchTable.push_back(CALCULATE_OFFSET(PSX::Model, offHeaders, modelOffset));

		for (size_t modelHeaderIndex = 0; (modelHeaderIndex < model.numHeaders) && isSupportedByCurrentTechnology; modelHeaderIndex++)
		{
			//modelHeaders are expected to be stored contiguously
			const PSX::ModelHeader& modelHeader = *(reinterpret_cast<const PSX::ModelHeader*>(Deref(model.offHeaders + (modelHeaderIndex * sizeof(PSX::ModelHeader)))));
			Log("\n  LOD %zu: %s\n", modelHeaderIndex, modelHeader.name);
			Log("    Max Distance: 0x%04X\n", modelHeader.maxDistanceLOD);
			Log("    Flags: 0x%04X\n", modelHeader.flags);
			Log("    Scale: (%d, %d, %d)\n", modelHeader.scale.x, modelHeader.scale.y, modelHeader.scale.z);
			Log("    offTexLayout %d\n", modelHeader.offTexLayout);

			if (modelHeader.offFrameData == (uint32_t)nullptr ||
				  modelHeader.numAnimations != 0 ||
					modelHeader.offAnimations != (uint32_t)nullptr ||
					modelHeader.offAnimtex != (uint32_t)nullptr
				 )
			{
				Log("    %s[!] SKIPPING:%s Unsupported features detected\n", ANSI_YELLOW, ANSI_RESET);
				if (modelHeader.offFrameData == (uint32_t)nullptr) Log("      - No frame data\n");
				if (modelHeader.numAnimations != 0) Log("      - Has animations (%u)\n", modelHeader.numAnimations);
				if (modelHeader.offAnimations != (uint32_t)nullptr) Log("      - Has animation data\n");
				if (modelHeader.offAnimtex != (uint32_t)nullptr) Log("      - Has animated textures\n");
				isSupportedByCurrentTechnology = false;
				break;
			}

			if (modelHeader.unk1 != 0 ||
				  modelHeader.maybeScaleMaybePadding != 0 ||
				  modelHeader.unk3 != 0)
			{
				Log("    %s[!] SKIPPING:%s Unexpected non-zero fields\n", ANSI_YELLOW, ANSI_RESET);
				if (modelHeader.unk1 != 0) Log("      - unk1 = 0x%08X\n", modelHeader.unk1);
				if (modelHeader.maybeScaleMaybePadding != 0) Log("      - maybeScaleMaybePadding = 0x%04X\n", modelHeader.maybeScaleMaybePadding);
				if (modelHeader.unk3 != 0) Log("      - unk3 = 0x%08X\n", modelHeader.unk3);
				Log("      TODO: Investigate if these fields are safe to ignore\n");
				isSupportedByCurrentTechnology = false;
				break;
			}

			// Get pointer to THIS header within the pre-allocated block
			PSX::ModelHeader* output_ModelHeader = &output_AllModelHeaders[modelHeaderIndex];

			// Headers are now contiguously stored in the pre-allocated block
			memcpy(output_ModelHeader, &modelHeader, sizeof(modelHeader));
      output_ModelHeader->offCommandList = 0; //patch later
      output_ModelHeader->offFrameData = 0; //patch later
      output_ModelHeader->offTexLayout = 0; //patch later
      output_ModelHeader->offColors = 0; //patch later

			// Declare offset variables at loop scope so they're accessible when patching
			size_t unkNumOffset = 0;
			size_t commandListOffset = 0;
			size_t frameDataOffset = 0;
			size_t texLayoutOffset = 0;
			size_t clutOffset = 0;

			// Command list structure: [unkNum:u32] [commands...] [0xFFFFFFFF terminator]
			const uint8_t* commandListPtr = Deref(modelHeader.offCommandList);
			const uint32_t unkNum = *reinterpret_cast<const uint32_t*>(commandListPtr);
			const PSX::InstDrawCommand* commandList = reinterpret_cast<const PSX::InstDrawCommand*>(commandListPtr + 4);

			Log("    Command list unkNum: %u (0x%08X)\n", unkNum, unkNum);

			size_t numberOfCommands = 0;
			size_t numberOfStoredVerts = 0;
			uint32_t maxTexCoordIndex = 0;
			uint32_t maxColorCoordIndex = 0;
			for (size_t commandListEntryIndex = 0; commandList[commandListEntryIndex].command != 0xFFFFFFFF; commandListEntryIndex++)
			{
				numberOfCommands = commandListEntryIndex + 1;
        const PSX::InstDrawCommand& command = commandList[commandListEntryIndex];

				// Track maximum indices to determine array sizes
				if (command.texCoordIndex > maxTexCoordIndex)
					maxTexCoordIndex = command.texCoordIndex;
				if (command.colorCoordIndex > maxColorCoordIndex)
					maxColorCoordIndex = command.colorCoordIndex;

				if (command.readNextVertFromStackIndexFlag == 0) { numberOfStoredVerts++; }
			}

			Log("    Commands: %zu (0x%zx bytes)\n", numberOfCommands, (numberOfCommands + 1) * sizeof(PSX::InstDrawCommand));
			Log("    Vertices: %zu (%zu bytes)\n", numberOfStoredVerts, numberOfStoredVerts * 3);
			Log("    Max texture index: %u\n", maxTexCoordIndex);
			Log("    Max color index: %u\n", maxColorCoordIndex);

			// Write unkNum (precedes command list)
			unkNumOffset = currentOffset;
			uint32_t* output_UnkNum = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t)));
			*output_UnkNum = unkNum;
			modelDataChunks.push_back({ sizeof(uint32_t), output_UnkNum });
			currentOffset += sizeof(uint32_t);
			Log("    " nameof(unkNumOffset) " = 0x%zx\n", unkNumOffset);

			// Write command list (including 0xFFFFFFFF terminator)
			commandListOffset = currentOffset;
			const size_t commandListSize = (numberOfCommands + 1) * sizeof(PSX::InstDrawCommand);
			PSX::InstDrawCommand* output_CommandList = reinterpret_cast<PSX::InstDrawCommand*>(malloc(commandListSize));
			memcpy(output_CommandList, commandList, commandListSize);
			modelDataChunks.push_back({ commandListSize, output_CommandList });
			currentOffset += commandListSize;
			Log("    " nameof(commandListOffset) " = 0x%zx (%zu commands, size = 0x%zx)\n",
			       commandListOffset, numberOfCommands, commandListSize);

			if (modelHeader.offFrameData != (uint32_t)nullptr)
			{ //not animated
				const PSX::ModelFrame& modelFrame = *reinterpret_cast<const PSX::ModelFrame*>(Deref(modelHeader.offFrameData));
				const uint8_t* frameDataBase = reinterpret_cast<const uint8_t*>(&modelFrame);
				const uint8_t* vertData = (uint8_t*)(((uint64_t)&modelFrame) + modelFrame.vertexOffset);

				// Extract frame data: ModelFrame + mystery padding + vertices
				frameDataOffset = currentOffset;
				Log("    " nameof(frameDataOffset) " = 0x%zx\n", frameDataOffset);

				// ModelFrame structure
				PSX::ModelFrame* output_ModelFrame = reinterpret_cast<PSX::ModelFrame*>(malloc(sizeof(PSX::ModelFrame)));
				memcpy(output_ModelFrame, &modelFrame, sizeof(PSX::ModelFrame));

				// Force vertexOffset to 0x1C (drop any mystery padding from original)
				output_ModelFrame->vertexOffset = 0x1C;

				// Log warning if original had non-standard offset
				if (modelFrame.vertexOffset > 0x1C)
				{
					size_t droppedBytes = modelFrame.vertexOffset - 0x1C;
					Log("    WARNING: Model has non-standard vertexOffset 0x%X, dropping %lld mystery bytes\n",
					       modelFrame.vertexOffset, droppedBytes);
					// NOTE: If these bytes become significant, we'll need to revisit this decision
				}

				currentOffset += sizeof(PSX::ModelFrame);
				modelDataChunks.push_back({ sizeof(PSX::ModelFrame), output_ModelFrame });


				// Vertex data
				const size_t vertexDataSize = numberOfStoredVerts * 3;
				uint8_t* output_VertexData = reinterpret_cast<uint8_t*>(malloc(vertexDataSize));
				memcpy(output_VertexData, vertData, vertexDataSize);
				currentOffset += vertexDataSize;
				modelDataChunks.push_back({ vertexDataSize, output_VertexData });

				// Add 4-byte alignment padding after vertices
				const size_t paddingSize = (vertexDataSize % 4 == 0) ? 0 : (4 - (vertexDataSize % 4));
				if (paddingSize > 0)
				{
					uint8_t* padding = reinterpret_cast<uint8_t*>(calloc(paddingSize, 1));
					currentOffset += paddingSize;
					modelDataChunks.push_back({ paddingSize, padding });
					Log("    Added %zu bytes of padding after vertices\n", paddingSize);
				}
			}
			else
			{
				//technechally this is caught by the "pre-check" at the start of the for loop, but later on this "else" case 
				//will probably be for supporting animated models.
				Log("    Model has unsupported features, skipping extraction...\n");
				isSupportedByCurrentTechnology = false;
				break;
			}

			// offTexLayout points to an array of POINTERS to TextureLayouts
			// Note: Texture indices are 1-based (0 = no texture, 1 = first texture at tl[0], etc.)
			const uint32_t* textureLayoutPtrArray = reinterpret_cast<const uint32_t*>(Deref(modelHeader.offTexLayout));

			// offColors points directly to 4-byte color array (not pointers)
			// Color indices are 0-based
			const uint32_t* colorArray = reinterpret_cast<const uint32_t*>(Deref(modelHeader.offColors));

			// Texture array size = max texture index (since indices are 1-based, index N means we need N textures)
			const size_t numTexLayouts = maxTexCoordIndex;

			// First, write the TextureLayout structures themselves
			const size_t textureLayoutsOffset = currentOffset;
			const size_t texLayoutsSize = numTexLayouts * sizeof(PSX::TextureLayout);
			Log("    Texture layouts offset = 0x%zx (count = %zu, size = 0x%zx)\n",
			       textureLayoutsOffset, numTexLayouts, texLayoutsSize);

			if (numTexLayouts > 0)
			{
				PSX::TextureLayout* output_TextureLayouts = reinterpret_cast<PSX::TextureLayout*>(malloc(texLayoutsSize));
				// Copy textures (indices are 1-based, so texture N is at array position N-1)
				for (size_t i = 0; i < numTexLayouts; i++)
				{
					const PSX::TextureLayout* layout = reinterpret_cast<const PSX::TextureLayout*>(Deref(textureLayoutPtrArray[i]));
					output_TextureLayouts[i] = *layout;
				}
				currentOffset += texLayoutsSize;
				modelDataChunks.push_back({ texLayoutsSize, output_TextureLayouts });

				// Extract textures as PNG files (deduplicated by CLUT)
				std::filesystem::path texOutputDir = outputDir / (std::string(model.name) + "_textures");
				std::filesystem::create_directories(texOutputDir);

				// Group TextureLayouts using ctr-tools GroupByPalette approach
				// Groups by PageX_PageY_PalX_PalY, then finds max bounding rect
				std::unordered_map<std::string, SH::TextureGroup> textureGroups;

				for (size_t i = 0; i < numTexLayouts; i++)
				{
					const PSX::TextureLayout& layout = output_TextureLayouts[i];

					int pageX = layout.texPage.x;
					int pageY = layout.texPage.y;
					int palX = layout.clut.x;
					int palY = layout.clut.y;
					int bpp = layout.texPage.texpageColors;
					int blendMode = layout.texPage.blendMode;

					// Calculate UV bounds for this layout
					int layoutMinU = std::min({layout.u0, layout.u1, layout.u2, layout.u3});
					int layoutMinV = std::min({layout.v0, layout.v1, layout.v2, layout.v3});
					int layoutMaxU = std::max({layout.u0, layout.u1, layout.u2, layout.u3});
					int layoutMaxV = std::max({layout.v0, layout.v1, layout.v2, layout.v3});

					// Group by palette + texpage (ctr-tools GroupByPalette style)
					std::string key = std::to_string(pageX) + "_" + std::to_string(pageY) + "_" +
					                  std::to_string(palX) + "_" + std::to_string(palY);

					if (textureGroups.find(key) == textureGroups.end())
					{
						textureGroups[key].representativeLayout = layout;
						textureGroups[key].pageX = pageX;
						textureGroups[key].pageY = pageY;
						textureGroups[key].palX = palX;
						textureGroups[key].palY = palY;
						textureGroups[key].bpp = bpp;
						textureGroups[key].blendMode = blendMode;
					}

					// Expand bounding box to include this layout
					textureGroups[key].minU = std::min(textureGroups[key].minU, layoutMinU);
					textureGroups[key].minV = std::min(textureGroups[key].minV, layoutMinV);
					textureGroups[key].maxU = std::max(textureGroups[key].maxU, layoutMaxU);
					textureGroups[key].maxV = std::max(textureGroups[key].maxV, layoutMaxV);
					textureGroups[key].layoutIndices.push_back(i);
				}

				Log("      Grouped %zu TextureLayouts into %zu unique textures (max bounding rect)\n",
				    numTexLayouts, textureGroups.size());

				// Extract one texture per unique group using the combined bounding box
				// Also create mapping from TextureLayout index to texture index
				std::vector<uint32_t> thisHeaderTexLayoutMapping(numTexLayouts, 0);
				
				for (auto& [key, group] : textureGroups)
				{
					// Calculate dimensions from the combined bounding box
					int width = group.maxU - group.minU;
					int height = group.maxV - group.minV;

					int bpp = group.bpp;
					const char* bppStr = (bpp == 0) ? "4-bit" : (bpp == 1) ? "8-bit" : "16-bit";
					int stretch = (bpp == 0) ? 4 : (bpp == 1) ? 2 : 1;
					int realX = group.pageX * 64 + group.minU / stretch;
					int realY = group.pageY * 256 + group.minV;

					size_t textureIndex;
					if (keyToTextureIndex.find(key) != keyToTextureIndex.end())
					{
						// Texture already exists in model collection
						textureIndex = keyToTextureIndex[key];
						Log("      Texture (reused): %dx%d at VRAM(%d,%d), %s (from %zu layouts)\n",
						    width, height, realX, realY, bppStr, group.layoutIndices.size());
					}
					else if (width > 0 && height > 0 && width <= 256 && height <= 256)
					{
						// Extract raw PSX texture data
						RawTextureData rawTex;
						ExtractRawPSXTexture(vram, group.pageX, group.pageY, group.palX, group.palY,
						                     bpp, group.blendMode, group.minU, group.minV, width, height, rawTex);
						rawTex.key = key;

						textureIndex = modelTextures.size();
						modelTextures.push_back(std::move(rawTex));
						keyToTextureIndex[key] = textureIndex;

						Log("      Texture %zu: %dx%d at VRAM(%d,%d), CLUT(%d,%d), %s (from %zu layouts)\n",
						    textureIndex, width, height, realX, realY, group.palX * 16, group.palY, bppStr, group.layoutIndices.size());

						// Also save as PNG for debugging
						std::vector<RGBA8> pixels;
						PSX::TextureLayout combinedLayout = group.representativeLayout;
						combinedLayout.u0 = group.minU; combinedLayout.v0 = group.minV;
						combinedLayout.u1 = group.maxU; combinedLayout.v1 = group.minV;
						combinedLayout.u2 = group.minU; combinedLayout.v2 = group.maxV;
						combinedLayout.u3 = group.maxU; combinedLayout.v3 = group.maxV;
						ExtractTexture(vram, combinedLayout, width, height, pixels);
						std::string texFilename = "tex_" + key + ".png";
						std::filesystem::path texPath = texOutputDir / texFilename;
						stbi_write_png(texPath.string().c_str(), width, height, 4, pixels.data(), width * 4);
					}
					else
					{
						Log("      %sWARNING%s: Invalid texture dimensions %dx%d, skipping\n", ANSI_YELLOW, ANSI_RESET, width, height);
						continue;
					}

					// Map all TextureLayout indices in this group to the texture index
					// Note: textureIndex will be converted to file offset later
					for (size_t layoutIdx : group.layoutIndices)
					{
						thisHeaderTexLayoutMapping[layoutIdx] = static_cast<uint32_t>(textureIndex);
					}
				}
				perHeaderTexLayoutToTextureOffset.push_back(std::move(thisHeaderTexLayoutMapping));

				// Now write the pointer array that points to each TextureLayout
				texLayoutOffset = currentOffset;
				const size_t ptrArraySize = numTexLayouts * sizeof(uint32_t);
				Log("    " nameof(texLayoutOffset) " = 0x%zx (pointer array, %zu pointers)\n",
				       texLayoutOffset, numTexLayouts);

				uint32_t* output_TextureLayoutPtrArray = reinterpret_cast<uint32_t*>(malloc(ptrArraySize));
				for (size_t i = 0; i < numTexLayouts; i++)
				{
					output_TextureLayoutPtrArray[i] = static_cast<uint32_t>(textureLayoutsOffset + (i * sizeof(PSX::TextureLayout)));
					// Add this pointer to the patch table
					patchTable.push_back(static_cast<uint32_t>(texLayoutOffset + (i * sizeof(uint32_t))));
				}
				currentOffset += ptrArraySize;
				modelDataChunks.push_back({ ptrArraySize, output_TextureLayoutPtrArray });
			}

			// Extract colors (4-byte values, following CTR-tools)
			// Color array size = max color index + 1 (since indices are 0-based)
			clutOffset = currentOffset;
			const size_t numColors = maxColorCoordIndex + 1;
			const size_t colorSize = numColors * sizeof(uint32_t);
			Log("    " nameof(clutOffset) " = 0x%zx (count = %zu, size = 0x%zx)\n",
			       clutOffset, numColors, colorSize);
			if (numColors > 0)
			{
				uint32_t* output_Colors = reinterpret_cast<uint32_t*>(malloc(colorSize));
				// Copy colors directly (no remapping)
				for (size_t i = 0; i < numColors; i++)
				{
					output_Colors[i] = colorArray[i];
				}
				currentOffset += colorSize;
				modelDataChunks.push_back({ colorSize, output_Colors });
			}

			// Patch ModelHeader offsets
			output_ModelHeader->offCommandList = unkNumOffset;  // Points to unkNum (start of command list structure)
			output_ModelHeader->offFrameData = frameDataOffset;
			output_ModelHeader->offTexLayout = (numTexLayouts > 0) ? texLayoutOffset : 0;
			output_ModelHeader->offColors = (numColors > 0) ? clutOffset : 0;

			// Add ModelHeader pointer fields to patch table
			size_t headerBaseOffset = allHeadersOffset + (modelHeaderIndex * sizeof(PSX::ModelHeader));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offCommandList, headerBaseOffset));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offFrameData, headerBaseOffset));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offTexLayout, headerBaseOffset));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offColors, headerBaseOffset));

      //modelHeader.name --- done
      //modelHeader.unk1 --- NOT DONE: could be a pointer, if just a value then should be done
      //modelHeader.maxDistanceLOD --- done
			//modelHeader.flags --- done
      //modelHeader.scale --- done
			//modelHeader.maybeScaleMaybePadding --- done
			//modelHeader.offCommandList --- NOT DONE: is a pointer, need to capture this data, is terminated by 0xFFFFFFFF
      //modelHeader.offFrameData --- NOT DONE: is a pointer, need to capture this data, is a ModelFrame followed by variable length data, need to dry-parse command list to know how long this is.
			//modelHeader.offTexLayout --- NOT DONE: is a pointer to an array of pointers (it isn't clear how long this array is, may need to dry-parse command list to know how long this is).
			//modelHeader.offColors --- NOT DONE: is a pointer to the same location as offTexLayout??????? This however DOESN'T point to the same location for the startbanner model, needs investigation.
      //modelHeader.unk3 --- NOT DONE: could be a pointer, if just a value then should be done
      //modelHeader.numAnimations --- done
      //modelHeader.offAnimations --- NOT DONE: is a pointer,
      //modelHeader.offAnimtex --- NOT DONE: is a pointer,


			//TODO: fix bug where a single quadblock breaks the CTE
			//TODO: fix bug where spam clicking around causes turbo pads suddenly delete themselves
		}

		output_Model->offHeaders = allHeadersOffset;

		#undef CALCULATE_OFFSET

		// Write patch table: count followed by array
		const size_t patchTableOffset = currentOffset;
		uint32_t patchCount = static_cast<uint32_t>(patchTable.size());
		Log("  " nameof(patchTableOffset) " = 0x%zx (count = %u)\n", patchTableOffset, patchCount);

		uint32_t* output_PatchCount = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t)));
		*output_PatchCount = patchCount;
		currentOffset += sizeof(uint32_t);
		modelDataChunks.push_back({ sizeof(uint32_t), output_PatchCount });

		size_t patchArraySize = patchCount * sizeof(uint32_t);
		uint32_t* output_PatchArray = reinterpret_cast<uint32_t*>(malloc(patchArraySize));
		memcpy(output_PatchArray, patchTable.data(), patchArraySize);
		currentOffset += patchArraySize;
		modelDataChunks.push_back({ patchArraySize, output_PatchArray });

		Log("  Generated patch table with %u entries\n", patchCount);


		// Write texture section if there are any textures
		size_t textureSectionOffset = 0;
		if (!modelTextures.empty())
		{
			textureSectionOffset = currentOffset;
			Log("  Writing texture section at offset 0x%zx (%zu textures)\n", textureSectionOffset, modelTextures.size());

			// First, write TextureSectionHeader (just numTextures for now, offsets come after)
			SH::TextureSectionHeader* output_TexSectionHeader = reinterpret_cast<SH::TextureSectionHeader*>(malloc(sizeof(SH::TextureSectionHeader)));
			output_TexSectionHeader->numTextures = static_cast<uint32_t>(modelTextures.size());
			currentOffset += sizeof(SH::TextureSectionHeader);
			modelDataChunks.push_back({ sizeof(SH::TextureSectionHeader), output_TexSectionHeader });

			// Reserve space for texture offset array
			const size_t texOffsetArraySize = modelTextures.size() * sizeof(uint32_t);
			uint32_t* output_TexOffsetArray = reinterpret_cast<uint32_t*>(malloc(texOffsetArraySize));
			size_t texOffsetArrayOffset = currentOffset;
			currentOffset += texOffsetArraySize;
			modelDataChunks.push_back({ texOffsetArraySize, output_TexOffsetArray });

			// Write each texture and record its offset
			for (size_t i = 0; i < modelTextures.size(); i++)
			{
				const RawTextureData& tex = modelTextures[i];

				// Record the offset for this texture
				output_TexOffsetArray[i] = static_cast<uint32_t>(currentOffset);

				// Calculate sizes
				size_t pixelDataSize = tex.pixelData.size();
				size_t paletteDataSize = tex.palette.size() * sizeof(uint16_t);

				// Allocate and fill TextureDataHeader + pixel data + palette
				size_t totalSize = sizeof(SH::TextureDataHeader) + pixelDataSize + paletteDataSize;
				uint8_t* output_TexData = reinterpret_cast<uint8_t*>(malloc(totalSize));

				// Fill header
				SH::TextureDataHeader* header = reinterpret_cast<SH::TextureDataHeader*>(output_TexData);
				header->width = tex.width;
				header->height = tex.height;
				header->bpp = tex.bpp;
				header->blendMode = tex.blendMode;
				header->originU = tex.originU;
				header->originV = tex.originV;
				header->origPageX = tex.origPageX;
				header->origPageY = tex.origPageY;
				header->origPalX = tex.origPalX;
				header->origPalY_lo = static_cast<uint8_t>(tex.origPalY & 0xFF);
				header->origPalY_hi = static_cast<uint8_t>((tex.origPalY >> 8) & 0xFF);
				header->padding = 0;

				// Copy pixel data
				memcpy(output_TexData + sizeof(SH::TextureDataHeader), tex.pixelData.data(), pixelDataSize);

				// Copy palette data
				if (paletteDataSize > 0)
				{
					memcpy(output_TexData + sizeof(SH::TextureDataHeader) + pixelDataSize, tex.palette.data(), paletteDataSize);
				}

				currentOffset += totalSize;
				modelDataChunks.push_back({ totalSize, output_TexData });

				Log("    Texture %zu: offset=0x%x, %dx%d, %d-bit, %zu pixel bytes, %zu palette bytes\n",
				    i, output_TexOffsetArray[i], tex.width, tex.height, 
				    (tex.bpp == 0) ? 4 : (tex.bpp == 1) ? 8 : 16,
				    pixelDataSize, paletteDataSize);
			}

			// Add 4-byte alignment padding if needed
			size_t paddingNeeded = (currentOffset % 4 == 0) ? 0 : (4 - (currentOffset % 4));
			if (paddingNeeded > 0)
			{
				uint8_t* padding = reinterpret_cast<uint8_t*>(calloc(paddingNeeded, 1));
				currentOffset += paddingNeeded;
				modelDataChunks.push_back({ paddingNeeded, padding });
			}

			Log("  Texture section complete, total size: 0x%zx bytes\n", currentOffset - textureSectionOffset);
		}

		// Patch CtrModel header
		output_CtrModel->modelOffset = modelOffset;
		output_CtrModel->modelPatchTableOffset = patchTableOffset;
		output_CtrModel->textureDataOffset = static_cast<uint32_t>(textureSectionOffset);

		// Check if any model headers were successfully extracted
		if (!isSupportedByCurrentTechnology)
		{
			// All model headers were unsupported, clean up and skip file creation
			Log("\n  %s[SKIPPED]%s All model headers are unsupported, file not created\n", ANSI_YELLOW, ANSI_RESET);

			// Free all allocated memory
			for (const SH::WriteableObject& chunk : modelDataChunks)
			{
				free(chunk.data);
			}

			// Close and delete the file
			outputModelFile.close();
			std::filesystem::remove(outputFilePath);

			Log("========================================\n\n");
			continue;
		}

		Log("\n  %s[OK]%s Extraction complete\n", ANSI_GREEN, ANSI_RESET);
		Log("  Final file size: 0x%zx (%zu bytes)\n", currentOffset, currentOffset);
		if (currentOffset % 4 == 0)
		{
			Log("  Alignment: %s[OK]%s 4-byte aligned\n", ANSI_GREEN, ANSI_RESET);
		}
		else
		{
			Log("  Alignment: %s[X]%s NOT 4-byte aligned\n", ANSI_RED, ANSI_RESET);
		}

		for (const SH::WriteableObject& chunk : modelDataChunks)
		{
			outputModelFile.write((char*)chunk.data, chunk.size);
			free(chunk.data);
		}

		Log("  %s[OK]%s Successfully wrote: %s\n", ANSI_GREEN, ANSI_RESET, outputFilePath.string().c_str());
		Log("========================================\n\n");
	}

	#undef nameof
}