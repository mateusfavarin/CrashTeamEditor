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

		std::vector<size_t> modelHeaderOffsets{};
    std::vector<PSX::ModelHeader*> output_ModelHeaders{};
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

			const size_t modelHeaderOffset = currentOffset;
			modelHeaderOffsets.push_back(modelHeaderOffset);
			//consider pre-allocating the output modelheaders outside of this for loop, that way we can have the data 
      //belonging to the headers appended afterwards, avoiding the, "modelHeaders are expected to be stored contiguously" issue.
			PSX::ModelHeader* output_ModelHeader = reinterpret_cast<PSX::ModelHeader*>(malloc(sizeof(PSX::ModelHeader)));
			currentOffset += sizeof(*output_ModelHeader);
      output_ModelHeaders.push_back(output_ModelHeader);
			modelDataChunks.push_back({ sizeof(*output_ModelHeader), output_ModelHeader });

			//Reminder: modelHeaders are expected to be stored contiguously, so we can't have any of this other data intermixed.
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

				referencedTextures.insert({ command.texCoordIndex, textureIndexCounter });
				textureIndexCounter++;
				referencedColorCoords.insert({ command.colorCoordIndex, colorCoordIndexCounter });
				colorCoordIndexCounter++;

				if (command.readNextVertFromStackIndexFlag == 0) { numberOfStoredVerts++; }
			}
			printf("    ModelHeader references %lld unique textures\n", referencedTextures.size());
			printf("    ModelHeader references %lld unique color coords\n", referencedColorCoords.size());
			printf("    ModelHeader stores %lld vertices\n", numberOfStoredVerts);

			if (modelHeader.offFrameData != (uint32_t)nullptr)
			{ //not animated
				const PSX::ModelFrame& modelFrame = *reinterpret_cast<const PSX::ModelFrame*>(Deref(modelHeader.offFrameData));
				const uint8_t* vertData = Deref(((uint32_t)&modelFrame) + modelFrame.vertexOffset); //capture "numberOfStoredVerts * 3" starting at this location

				//although these considerations are valid, concerns about padding are more applicable once this model is
				//embedded in a level. For "serializing" to it's own discrete "model file", we can just ignore this.
				//size_t totalVertDataLength = numberOfStoredVerts * 3;
				//size_t vertDataStartAddr = (uint32_t)vertData;
				//size_t vertDataEndAddr = vertDataStartAddr + totalVertDataLength;
				//size_t totalVertDataLengthIncludingPadding;
				//if (vertDataEndAddr % 4 == 0) { totalVertDataLengthIncludingPadding = totalVertDataLength; }
				//else { totalVertDataLengthIncludingPadding = totalVertDataLength + (4 - (totalVertDataLength % 4)); }
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

		output_Model->offHeaders = modelHeaderOffsets[0];

		for (const SH::WriteableObject& chunk : modelDataChunks)
		{
			outputModelFile.write((char*)chunk.data, chunk.size);
			free(chunk.data);
		}
	}
}