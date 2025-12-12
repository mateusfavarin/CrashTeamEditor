#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>

class LevDataExtractor
{
public:
	LevDataExtractor(const std::filesystem::path& levPath, const std::filesystem::path& vrmPath);

	void ExtractModels(void);
private:
	std::filesystem::path m_levPath;
	std::filesystem::path m_vrmPath;
	std::vector<uint8_t> m_levData;
	std::vector<uint8_t> m_vrmData;
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
		uint32_t vramDataOffset; //TODO, investigate if this needs to be more nuanced
	};
}
