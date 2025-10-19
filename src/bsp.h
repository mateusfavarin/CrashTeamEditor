#pragma once

#include "geo.h"
#include "quadblock.h"

#include <vector>

enum class BSPNode
{
	BRANCH,
	LEAF
};

enum class AxisSplit
{
	NONE, X, Y, Z
};

struct BSPFlags
{
	static constexpr uint16_t NONE = 0;
	static constexpr uint16_t LEAF = 1 << 0;
	static constexpr uint16_t WATER = 1 << 1;
	static constexpr uint16_t SUBDIV_4_1 = 1 << 3;
	static constexpr uint16_t SUBDIV_4_2 = 1 << 4;
	static constexpr uint16_t INVISIBLE = 1 << 6;
	static constexpr uint16_t NO_COLLISION = 1 << 7;
};

struct BSPID
{
	static constexpr uint16_t LEAF = 0x4000;
	static constexpr uint16_t EMPTY = 0xFFFF;
};

static constexpr size_t MAX_QUADBLOCKS_LEAF = 32;

class BSP
{
public:
	BSP();
	BSP(BSPNode type, const std::vector<size_t>& quadblockIndexes, BSP* parent, const std::vector<Quadblock>& quadblocks);
	size_t Id() const;
	bool Empty() const;
	bool Valid() const;
	bool IsBranch() const;
	uint16_t GetFlags() const;
	const std::string& GetType() const;
	const std::string& GetAxis() const;
	const BoundingBox& GetBoundingBox() const;
	const std::vector<size_t>& GetQuadblockIndexes() const;
	const BSP* GetLeftChildren() const;
	const BSP* GetRightChildren() const;
	const BSP* GetParent() const;
	const std::vector<const BSP*> GetTree() const;
	std::vector<const BSP*> GetLeaves() const;
	void SetQuadblockIndexes(const std::vector<size_t>& quadblockIndexes);
	void Clear();
	void Generate(const std::vector<Quadblock>& quadblocks, const size_t maxQuadsPerLeaf, const float maxAxisLength);
	std::vector<uint8_t> Serialize(size_t offQuads) const;
	void RenderUI(const std::vector<Quadblock>& quadblocks);

private:
	float GetAxisMidpoint(const AxisSplit axis) const;
	BoundingBox ComputeBoundingBox(const std::vector<Quadblock>& quadblocks, const std::vector<size_t>& quadblockIndexes) const;
	float Split(std::vector<size_t>& left, std::vector<size_t>& right, const AxisSplit axis, const std::vector<Quadblock>& quadblocks) const;
	void GenerateOffspring(std::vector<size_t>& left, std::vector<size_t>& right, const std::vector<Quadblock>& quadblocks, const size_t maxQuadsPerLeaf, const float maxAxisLength);
	std::vector<uint8_t> SerializeBranch() const;
	std::vector<uint8_t> SerializeLeaf(size_t offQuads) const;

private:
	size_t m_id;
	BSPNode m_node;
	AxisSplit m_axis;
	uint16_t m_flags;
	BSP* m_left;
	BSP* m_right;
	BSP* m_parent;
	BoundingBox m_bbox;
	std::vector<size_t> m_quadblockIndexes;
};
