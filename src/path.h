#pragma once

#include "quadblock.h"
#include "checkpoint.h"

#include <string>
#include <vector>

class Path
{
public:
	Path(size_t index);
	bool Ready() const;
	std::vector<Checkpoint> GeneratePath(size_t pathStartIndex, std::vector<Quadblock>& quadblocks) const;
	void RenderUI(const std::vector<Quadblock>& quadblocks);

private:
	size_t m_index;

	size_t m_previewValueStart;
	std::string m_previewLabelStart;
	std::vector<size_t> m_quadIndexesStart;

	size_t m_previewValueEnd;
	std::string m_previewLabelEnd;
	std::vector<size_t> m_quadIndexesEnd;
};