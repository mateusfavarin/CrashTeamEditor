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

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_GRAY    "\033[90m"

// Color printing helpers
static void PrintSuccess(const char* format, ...)
{
	printf(ANSI_GREEN);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf(ANSI_RESET);
}

static void PrintWarning(const char* format, ...)
{
	printf(ANSI_YELLOW);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf(ANSI_RESET);
}

static void PrintError(const char* format, ...)
{
	printf(ANSI_RED);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf(ANSI_RESET);
}

static void PrintInfo(const char* format, ...)
{
	printf(ANSI_CYAN);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf(ANSI_RESET);
}

static void PrintDebug(const char* format, ...)
{
	printf(ANSI_GRAY);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf(ANSI_RESET);
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

	PrintInfo("Extracting models from LEV: %s\n", m_levPath.string().c_str());

	// Create output directory for extracted models
	std::filesystem::path outputDir = m_levPath.parent_path() / "extracted_models";
	std::filesystem::create_directories(outputDir);
	PrintInfo("Output directory: %s\n", outputDir.string().c_str());

	auto Deref = [this](uint32_t offset) -> const uint8_t* {
    return m_levData.data() + offset + 4; //offsets are from start of file + 4
	};

	const PSX::LevHeader& levHeader = *reinterpret_cast<const PSX::LevHeader*>(m_levData.data() + 4);

	const uint32_t* modelArray = reinterpret_cast<const uint32_t*>(Deref(levHeader.offModels));
	PrintInfo("\nFound %u models in LEV file\n\n", levHeader.numModels);

	for (size_t modelIndex = 0; modelIndex < levHeader.numModels; modelIndex++)
	{
		const PSX::Model& model = *reinterpret_cast<const PSX::Model*>(Deref(modelArray[modelIndex]));
		//if (memcmp(model.name, "cactus_saguro", 13) != 0)
		//	continue;
		printf("========================================\n");
		PrintInfo("Model %zu/%u: %s\n", modelIndex + 1, levHeader.numModels, model.name);
		PrintInfo("  ID: %d\n", model.id);
		PrintInfo("  Headers: %u\n", model.numHeaders);

		if (model.numHeaders == 0 || model.offHeaders == (uint32_t)nullptr)
		{
			printf("  Model has no headers, skipping extraction...\n");
      continue;
		}

    std::vector<SH::WriteableObject> modelDataChunks{};
		size_t currentOffset = 0;

		std::filesystem::path outputFilePath = outputDir / (std::string(model.name) + ".ctrmodel");
		std::ofstream outputModelFile(outputFilePath, std::ios::binary);
		if (!outputModelFile)
		{
			PrintError("  ERROR: Failed to create output file: %s\n", outputFilePath.string().c_str());
			continue;
		}

    bool isSupportedByCurrentTechnology = true; //we don't currently support anything with animations.

    const size_t ctrModelOffset = currentOffset;
		PrintDebug("  " nameof(ctrModelOffset) " = 0x%zx\n", ctrModelOffset);
		SH::CtrModel* output_CtrModel = reinterpret_cast<SH::CtrModel*>(malloc(sizeof(SH::CtrModel)));
		currentOffset += sizeof(*output_CtrModel);
    modelDataChunks.push_back({ sizeof(*output_CtrModel), output_CtrModel });

		const size_t modelOffset = currentOffset;
		PrintDebug("  " nameof(modelOffset) " = 0x%zx\n", modelOffset);
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
		PrintDebug("  " nameof(allHeadersOffset) " = 0x%zx (size = 0x%zx, %zu headers)\n",
		       allHeadersOffset, allHeadersSize, model.numHeaders);
		PSX::ModelHeader* output_AllModelHeaders =
		    reinterpret_cast<PSX::ModelHeader*>(malloc(allHeadersSize));
		currentOffset += allHeadersSize;
		modelDataChunks.push_back({ allHeadersSize, output_AllModelHeaders });

		for (size_t modelHeaderIndex = 0; (modelHeaderIndex < model.numHeaders) && isSupportedByCurrentTechnology; modelHeaderIndex++)
		{
			//modelHeaders are expected to be stored contiguously
			const PSX::ModelHeader& modelHeader = *(reinterpret_cast<const PSX::ModelHeader*>(Deref(model.offHeaders + (modelHeaderIndex * sizeof(PSX::ModelHeader)))));
			PrintInfo("\n  LOD %zu: %s\n", modelHeaderIndex, modelHeader.name);
			PrintInfo("    Max Distance: 0x%04X\n", modelHeader.maxDistanceLOD);
			PrintInfo("    Flags: 0x%04X\n", modelHeader.flags);
			PrintInfo("    Scale: (%d, %d, %d)\n", modelHeader.scale.x, modelHeader.scale.y, modelHeader.scale.z);
			PrintInfo("    offTexLayout %d\n", modelHeader.offTexLayout);

			if (modelHeader.offFrameData == (uint32_t)nullptr ||
				  modelHeader.numAnimations != 0 ||
					modelHeader.offAnimations != (uint32_t)nullptr ||
					modelHeader.offAnimtex != (uint32_t)nullptr
				 )
			{
				PrintWarning("    [!] SKIPPING: Unsupported features detected\n");
				if (modelHeader.offFrameData == (uint32_t)nullptr) printf("      - No frame data\n");
				if (modelHeader.numAnimations != 0) printf("      - Has animations (%u)\n", modelHeader.numAnimations);
				if (modelHeader.offAnimations != (uint32_t)nullptr) printf("      - Has animation data\n");
				if (modelHeader.offAnimtex != (uint32_t)nullptr) printf("      - Has animated textures\n");
				isSupportedByCurrentTechnology = false;
				break;
			}

			if (modelHeader.unk1 != 0 ||
				  modelHeader.maybeScaleMaybePadding != 0 ||
				  modelHeader.unk3 != 0)
			{
				PrintWarning("    [!] SKIPPING: Unexpected non-zero fields\n");
				if (modelHeader.unk1 != 0) printf("      - unk1 = 0x%08X\n", modelHeader.unk1);
				if (modelHeader.maybeScaleMaybePadding != 0) printf("      - maybeScaleMaybePadding = 0x%04X\n", modelHeader.maybeScaleMaybePadding);
				if (modelHeader.unk3 != 0) printf("      - unk3 = 0x%08X\n", modelHeader.unk3);
				printf("      TODO: Investigate if these fields are safe to ignore\n");
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

			PrintDebug("    Command list unkNum: %u (0x%08X)\n", unkNum, unkNum);

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

			PrintInfo("    Commands: %zu (0x%zx bytes)\n", numberOfCommands, (numberOfCommands + 1) * sizeof(PSX::InstDrawCommand));
			PrintInfo("    Vertices: %zu (%zu bytes)\n", numberOfStoredVerts, numberOfStoredVerts * 3);
			PrintInfo("    Max texture index: %u\n", maxTexCoordIndex);
			PrintInfo("    Max color index: %u\n", maxColorCoordIndex);

			// Write unkNum (precedes command list)
			unkNumOffset = currentOffset;
			uint32_t* output_UnkNum = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t)));
			*output_UnkNum = unkNum;
			modelDataChunks.push_back({ sizeof(uint32_t), output_UnkNum });
			currentOffset += sizeof(uint32_t);
			PrintDebug("    " nameof(unkNumOffset) " = 0x%zx\n", unkNumOffset);

			// Write command list (including 0xFFFFFFFF terminator)
			commandListOffset = currentOffset;
			const size_t commandListSize = (numberOfCommands + 1) * sizeof(PSX::InstDrawCommand);
			PSX::InstDrawCommand* output_CommandList = reinterpret_cast<PSX::InstDrawCommand*>(malloc(commandListSize));
			memcpy(output_CommandList, commandList, commandListSize);
			modelDataChunks.push_back({ commandListSize, output_CommandList });
			currentOffset += commandListSize;
			PrintDebug("    " nameof(commandListOffset) " = 0x%zx (%zu commands, size = 0x%zx)\n",
			       commandListOffset, numberOfCommands, commandListSize);

			if (modelHeader.offFrameData != (uint32_t)nullptr)
			{ //not animated
				const PSX::ModelFrame& modelFrame = *reinterpret_cast<const PSX::ModelFrame*>(Deref(modelHeader.offFrameData));
				const uint8_t* frameDataBase = reinterpret_cast<const uint8_t*>(&modelFrame);
				const uint8_t* vertData = (uint8_t*)(((uint64_t)&modelFrame) + modelFrame.vertexOffset);

				// Extract frame data: ModelFrame + mystery padding + vertices
				frameDataOffset = currentOffset;
				PrintDebug("    " nameof(frameDataOffset) " = 0x%zx\n", frameDataOffset);

				// ModelFrame structure
				PSX::ModelFrame* output_ModelFrame = reinterpret_cast<PSX::ModelFrame*>(malloc(sizeof(PSX::ModelFrame)));
				memcpy(output_ModelFrame, &modelFrame, sizeof(PSX::ModelFrame));

				// Force vertexOffset to 0x1C (drop any mystery padding from original)
				output_ModelFrame->vertexOffset = 0x1C;

				// Log warning if original had non-standard offset
				if (modelFrame.vertexOffset > 0x1C)
				{
					size_t droppedBytes = modelFrame.vertexOffset - 0x1C;
					PrintWarning("    WARNING: Model has non-standard vertexOffset 0x%X, dropping %lld mystery bytes\n",
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
					PrintInfo("    Added %zu bytes of padding after vertices\n", paddingSize);
				}
			}
			else
			{
				//technechally this is caught by the "pre-check" at the start of the for loop, but later on this "else" case 
				//will probably be for supporting animated models.
				printf("    Model has unsupported features, skipping extraction...\n");
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

			// Extract texture layouts (copy directly, no remapping)
			texLayoutOffset = currentOffset;
			const size_t texLayoutSize = numTexLayouts * sizeof(PSX::TextureLayout);
			PrintDebug("    " nameof(texLayoutOffset) " = 0x%zx (count = %zu, size = 0x%zx)\n",
			       texLayoutOffset, numTexLayouts, texLayoutSize);

			if (numTexLayouts > 0)
			{
				PSX::TextureLayout* output_TextureLayouts = reinterpret_cast<PSX::TextureLayout*>(malloc(texLayoutSize));
				// Copy textures (indices are 1-based, so texture N is at array position N-1)
				for (size_t i = 0; i < numTexLayouts; i++)
				{
					const PSX::TextureLayout* layout = reinterpret_cast<const PSX::TextureLayout*>(Deref(textureLayoutPtrArray[i]));
					output_TextureLayouts[i] = *layout;
				}
				currentOffset += texLayoutSize;
				modelDataChunks.push_back({ texLayoutSize, output_TextureLayouts });
			}

			// Extract colors (4-byte values, following CTR-tools)
			// Color array size = max color index + 1 (since indices are 0-based)
			clutOffset = currentOffset;
			const size_t numColors = maxColorCoordIndex + 1;
			const size_t colorSize = numColors * sizeof(uint32_t);
			PrintDebug("    " nameof(clutOffset) " = 0x%zx (count = %zu, size = 0x%zx)\n",
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

		// Generate patch table - track all pointer offsets for SaveLEV to patch when embedding
		#define CALCULATE_OFFSET(s, m, b) static_cast<uint32_t>(offsetof(s, m) + b)

		std::vector<uint32_t> patchTable;

		// Add offset of Model.offHeaders field
		patchTable.push_back(CALCULATE_OFFSET(PSX::Model, offHeaders, modelOffset));

		// Add offsets for each ModelHeader's pointer fields
		for (size_t i = 0; i < model.numHeaders; i++)
		{
			size_t headerBaseOffset = allHeadersOffset + (i * sizeof(PSX::ModelHeader));

			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offCommandList, headerBaseOffset));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offFrameData, headerBaseOffset));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offTexLayout, headerBaseOffset));
			patchTable.push_back(CALCULATE_OFFSET(PSX::ModelHeader, offColors, headerBaseOffset));
		}

		#undef CALCULATE_OFFSET

		// Write patch table: count followed by array
		const size_t patchTableOffset = currentOffset;
		uint32_t patchCount = static_cast<uint32_t>(patchTable.size());
		PrintDebug("  " nameof(patchTableOffset) " = 0x%zx (count = %u)\n", patchTableOffset, patchCount);

		uint32_t* output_PatchCount = reinterpret_cast<uint32_t*>(malloc(sizeof(uint32_t)));
		*output_PatchCount = patchCount;
		currentOffset += sizeof(uint32_t);
		modelDataChunks.push_back({ sizeof(uint32_t), output_PatchCount });

		size_t patchArraySize = patchCount * sizeof(uint32_t);
		uint32_t* output_PatchArray = reinterpret_cast<uint32_t*>(malloc(patchArraySize));
		memcpy(output_PatchArray, patchTable.data(), patchArraySize);
		currentOffset += patchArraySize;
		modelDataChunks.push_back({ patchArraySize, output_PatchArray });

		PrintInfo("  Generated patch table with %u entries\n", patchCount);

		// Patch CtrModel header
		output_CtrModel->modelOffset = modelOffset;
		output_CtrModel->modelPatchTableOffset = patchTableOffset;
		output_CtrModel->vramDataOffset = 0; // Textures not implemented yet

		// Check if any model headers were successfully extracted
		if (!isSupportedByCurrentTechnology)
		{
			// All model headers were unsupported, clean up and skip file creation
			PrintWarning("\n  [SKIPPED] All model headers are unsupported, file not created\n");

			// Free all allocated memory
			for (const SH::WriteableObject& chunk : modelDataChunks)
			{
				free(chunk.data);
			}

			// Close and delete the file
			outputModelFile.close();
			std::filesystem::remove(outputFilePath);

			printf("========================================\n\n");
			continue;
		}

		PrintSuccess("\n  [OK] Extraction complete\n");
		PrintInfo("  Final file size: 0x%zx (%zu bytes)\n", currentOffset, currentOffset);
		if (currentOffset % 4 == 0)
		{
			PrintSuccess("  Alignment: [OK] 4-byte aligned\n");
		}
		else
		{
			PrintError("  Alignment: [X] NOT 4-byte aligned\n");
		}

		for (const SH::WriteableObject& chunk : modelDataChunks)
		{
			outputModelFile.write((char*)chunk.data, chunk.size);
			free(chunk.data);
		}

		PrintSuccess("  [OK] Successfully wrote: %s\n", outputFilePath.string().c_str());
		printf("========================================\n\n");
	}

	#undef nameof
}