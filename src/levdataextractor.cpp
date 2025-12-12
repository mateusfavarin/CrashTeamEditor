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
	printf("Extracting models from LEV: %s\n", m_levPath.string().c_str());

	auto Deref = [this](uint32_t offset) -> const uint8_t* {
    return m_levData.data() + offset + 4; //offsets are from start of file + 4
	};

	const PSX::LevHeader& levHeader = *reinterpret_cast<const PSX::LevHeader*>(m_levData.data() + 4);

	const uint32_t* modelArray = reinterpret_cast<const uint32_t*>(Deref(levHeader.offModels));
	for (size_t modelIndex = 0; modelIndex < levHeader.numModels; modelIndex++)
	{
		const PSX::Model& model = *reinterpret_cast<const PSX::Model*>(Deref(modelArray[modelIndex]));
		printf("Model name: %s\n", model.name);

		if (model.numHeaders == 0 || model.offHeaders == (uint32_t)nullptr)
		{
			printf("  Model has no headers, skipping extraction...\n");
      continue;
		}

    std::vector<SH::WriteableObject> modelDataChunks{};

		std::ofstream outputModelFile{ std::string(model.name, ".ctrmodel").c_str() }; //custom format, perhaps we should talk to DCxDemo about making formats intercompatible/standardized (I think they already have a standard, but it probably has differences from this one)
		size_t currentOffset = 0;
    bool isSupportedByCurrentTechnology = true; //we don't currently support anything with animations.

    const size_t ctrModelOffset = currentOffset;
		SH::CtrModel* output_CtrModel = reinterpret_cast<SH::CtrModel*>(malloc(sizeof(SH::CtrModel)));
		currentOffset += sizeof(*output_CtrModel);
    modelDataChunks.push_back({ sizeof(*output_CtrModel), output_CtrModel });

		const size_t modelOffset = currentOffset;
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
		PSX::ModelHeader* output_AllModelHeaders =
		    reinterpret_cast<PSX::ModelHeader*>(malloc(allHeadersSize));
		currentOffset += allHeadersSize;
		modelDataChunks.push_back({ allHeadersSize, output_AllModelHeaders });

		for (size_t modelHeaderIndex = 0; (modelHeaderIndex < model.numHeaders) && isSupportedByCurrentTechnology; modelHeaderIndex++)
		{
			//modelHeaders are expected to be stored contiguously
			const PSX::ModelHeader& modelHeader = *(reinterpret_cast<const PSX::ModelHeader*>(Deref(model.offHeaders + modelHeaderIndex)));
      printf("  Model header name: %s\n", modelHeader.name);

			if (modelHeader.offFrameData == (uint32_t)nullptr ||
				  modelHeader.numAnimations != 0 ||
					modelHeader.offAnimations != (uint32_t)nullptr ||
					modelHeader.offAnimtex != (uint32_t)nullptr
				 )
			{
				printf("    Model has unsupported features, skipping extraction...\n");
				isSupportedByCurrentTechnology = false;
				break; //since this is a break and not a continue, technechally the boolean check doesn't need to be in the for loop condition.
        //maybe we should consider just skipping *only* the unsupported model header(s) instead of the whole model?
			}

			if (modelHeader.unk1 != 0 ||
				  modelHeader.maybeScaleMaybePadding != 0 ||
				  modelHeader.unk3 != 0)
			{
        //TODO: investigate what these fields actually do, maybe it's safe after all.
				printf("    Model has unexpected data, skipping extraction...\n");
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

      const PSX::InstDrawCommand* commandList = reinterpret_cast<const PSX::InstDrawCommand*>(Deref(modelHeader.offCommandList));

			size_t numberOfCommands = 0;
			size_t numberOfStoredVerts = 0;
			std::unordered_map<uint32_t, uint32_t> referencedTextures{};
			std::unordered_map<uint32_t, uint32_t> referencedColorCoords{};
			size_t textureIndexCounter = 0;
      size_t colorCoordIndexCounter = 0;
			for (size_t commandListEntryIndex = 0; commandList[commandListEntryIndex].command != 0xFFFFFFFF; commandListEntryIndex++)
			{
				numberOfCommands = commandListEntryIndex + 1;
        const PSX::InstDrawCommand& command = commandList[commandListEntryIndex];

				// Only increment counter if we actually inserted a new entry
				auto texResult = referencedTextures.insert({ command.texCoordIndex, textureIndexCounter });
				if (texResult.second) { textureIndexCounter++; }

				auto colorResult = referencedColorCoords.insert({ command.colorCoordIndex, colorCoordIndexCounter });
				if (colorResult.second) { colorCoordIndexCounter++; }

				if (command.readNextVertFromStackIndexFlag == 0) { numberOfStoredVerts++; }
			}
			printf("    ModelHeader references %lld unique textures\n", referencedTextures.size());
			printf("    ModelHeader references %lld unique color coords\n", referencedColorCoords.size());
			printf("    ModelHeader stores %lld vertices\n", numberOfStoredVerts);

			// Extract command list (including 0xFFFFFFFF terminator)
			const size_t commandListOffset = currentOffset;
			const size_t commandListSize = (numberOfCommands + 1) * sizeof(PSX::InstDrawCommand);
			PSX::InstDrawCommand* output_CommandList = reinterpret_cast<PSX::InstDrawCommand*>(malloc(commandListSize));
			memcpy(output_CommandList, commandList, commandListSize);

			// Remap texture and color indices to new sequential indices
			for (size_t i = 0; i < numberOfCommands; i++)
			{
				uint32_t oldTexIndex = output_CommandList[i].texCoordIndex;
				uint32_t oldColorIndex = output_CommandList[i].colorCoordIndex;

				output_CommandList[i].texCoordIndex = referencedTextures[oldTexIndex];
				output_CommandList[i].colorCoordIndex = referencedColorCoords[oldColorIndex];
			}

			currentOffset += commandListSize;
			modelDataChunks.push_back({ commandListSize, output_CommandList });

			if (modelHeader.offFrameData != (uint32_t)nullptr)
			{ //not animated
				const PSX::ModelFrame& modelFrame = *reinterpret_cast<const PSX::ModelFrame*>(Deref(modelHeader.offFrameData));
				const uint8_t* frameDataBase = reinterpret_cast<const uint8_t*>(&modelFrame);
				const uint8_t* vertData = Deref(((uint32_t)&modelFrame) + modelFrame.vertexOffset);

				// Extract frame data: ModelFrame + mystery padding + vertices
				const size_t frameDataOffset = currentOffset;

				// ModelFrame structure
				PSX::ModelFrame* output_ModelFrame = reinterpret_cast<PSX::ModelFrame*>(malloc(sizeof(PSX::ModelFrame)));
				memcpy(output_ModelFrame, &modelFrame, sizeof(PSX::ModelFrame));

				// Force vertexOffset to 0x1C (drop any mystery padding from original)
				output_ModelFrame->vertexOffset = 0x1C;

				// Log warning if original had non-standard offset
				if (modelFrame.vertexOffset > 0x1C)
				{
					size_t droppedBytes = modelFrame.vertexOffset - 0x1C;
					printf("    WARNING: Model has non-standard vertexOffset 0x%X, dropping %lld mystery bytes\n",
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
			}
			else
			{
				//technechally this is caught by the "pre-check" at the start of the for loop, but later on this "else" case 
				//will probably be for supporting animated models.
				printf("    Model has unsupported features, skipping extraction...\n");
				isSupportedByCurrentTechnology = false;
				break;
			}

      const PSX::TextureLayout* textureLayoutArray = reinterpret_cast<const PSX::TextureLayout*>(Deref(modelHeader.offTexLayout));
			const PSX::CLUT* clutArray = reinterpret_cast<const PSX::CLUT*>(Deref(modelHeader.offColors));

			// Extract texture layouts
			const size_t texLayoutOffset = currentOffset;
			const size_t numTexLayouts = referencedTextures.size();
			const size_t texLayoutSize = numTexLayouts * sizeof(PSX::TextureLayout);
			if (numTexLayouts > 0)
			{
				PSX::TextureLayout* output_TextureLayouts = reinterpret_cast<PSX::TextureLayout*>(malloc(texLayoutSize));
				for (const auto& [originalIndex, newIndex] : referencedTextures)
				{
					output_TextureLayouts[newIndex] = textureLayoutArray[originalIndex];
				}
				currentOffset += texLayoutSize;
				modelDataChunks.push_back({ texLayoutSize, output_TextureLayouts });
			}

			// Extract CLUTs
			const size_t clutOffset = currentOffset;
			const size_t numCLUTs = referencedColorCoords.size();
			const size_t clutSize = numCLUTs * sizeof(PSX::CLUT);
			if (numCLUTs > 0)
			{
				PSX::CLUT* output_CLUTs = reinterpret_cast<PSX::CLUT*>(malloc(clutSize));
				for (const auto& [originalIndex, newIndex] : referencedColorCoords)
				{
					output_CLUTs[newIndex] = clutArray[originalIndex];
				}
				currentOffset += clutSize;
				modelDataChunks.push_back({ clutSize, output_CLUTs });
			}

			// Patch ModelHeader offsets
			output_ModelHeader->offCommandList = commandListOffset;
			output_ModelHeader->offFrameData = frameDataOffset;
			output_ModelHeader->offTexLayout = (numTexLayouts > 0) ? texLayoutOffset : 0;
			output_ModelHeader->offColors = (numCLUTs > 0) ? clutOffset : 0;


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

		// Patch CtrModel header
		output_CtrModel->modelOffset = modelOffset;
		output_CtrModel->modelPatchTableOffset = 0; // Not implemented yet
		output_CtrModel->vramDataOffset = 0; // Textures not implemented yet

		for (const SH::WriteableObject& chunk : modelDataChunks)
		{
			outputModelFile.write((char*)chunk.data, chunk.size);
			free(chunk.data);
		}
	}
}