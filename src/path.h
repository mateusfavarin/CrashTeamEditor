#pragma once

#include "quadblock.h"
#include "checkpoint.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class Path
{
public:
	Path();
	Path(size_t index);
	Path(const Path& path);
	~Path();
	size_t GetIndex() const;
	size_t GetStart() const;
	size_t GetEnd() const;
	std::vector<size_t>& GetStartIndexes();
	std::vector<size_t>& GetEndIndexes();
	std::vector<size_t>& GetIgnoreIndexes();
	bool IsReady() const;
	void SetIndex(size_t index);
	void UpdateDist(float dist, const Vec3& refPoint, std::vector<Checkpoint>& checkpoints);
	std::vector<Checkpoint> GeneratePath(size_t pathStartIndex, std::vector<Quadblock>& quadblocks);
	void RenderUI(const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, bool drawPathBtn, bool& insertAbove, bool& removePath);
	void ToJson(nlohmann::json& json, const std::vector<Quadblock>& quadblocks) const;
	void FromJson(const nlohmann::json& json, const std::vector<Quadblock>& quadblocks);

	Path& operator=(const Path& path);

private:
	void GetStartEndIndexes(std::vector<size_t>& out) const;

private:
	size_t m_index;
	size_t m_start;
	size_t m_end;
	Path* m_left;
	Path* m_right;

	size_t m_previewValueStart;
	std::string m_previewLabelStart;
	std::vector<size_t> m_quadIndexesStart;

	size_t m_previewValueIgnore;
	std::string m_previewLabelIgnore;
	std::vector<size_t> m_quadIndexesIgnore;

	size_t m_previewValueEnd;
	std::string m_previewLabelEnd;
	std::vector<size_t> m_quadIndexesEnd;
};
