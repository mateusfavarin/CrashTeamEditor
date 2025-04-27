#include "bsp.h"
#include "psx_types.h"

#include <cstring>

static size_t g_id = 0;

BSP::BSP()
{
	m_id = g_id++;
	m_node = BSPNode::BRANCH;
	m_axis = AxisSplit::NONE;
	m_flags = BSPFlags::NONE;
	m_left = nullptr;
	m_right = nullptr;
	m_bbox = BoundingBox();
	m_quadblockIndexes = std::vector<size_t>();
}

BSP::BSP(BSPNode type, const std::vector<size_t>& quadblockIndexes)
{
	m_id = g_id++;
	m_node = type;
	m_axis = AxisSplit::NONE;
	m_flags = m_node == BSPNode::LEAF ? BSPFlags::LEAF : BSPFlags::NONE;
	m_left = nullptr;
	m_right = nullptr;
	m_bbox = BoundingBox();
	m_quadblockIndexes = quadblockIndexes;
}

size_t BSP::Id() const
{
	return m_id;
}

bool BSP::Empty() const
{
	return m_quadblockIndexes.empty();
}

bool BSP::Valid() const
{
	for (const BSP* bsp : GetTree())
	{
		if (bsp->m_node == BSPNode::BRANCH) { if (!bsp->m_right && !bsp->m_left) { return false; } }
		else if (bsp->Empty()) { return false; }
	}
	return true;
}

bool BSP::IsBranch() const
{
	return m_node == BSPNode::BRANCH;
}

uint16_t BSP::GetFlags() const
{
	return m_flags;
}

const std::string& BSP::GetType() const
{
	const static std::string sBranch = "Branch";
	const static std::string sLeaf = "Leaf";
	return m_node == BSPNode::BRANCH ? sBranch : sLeaf;
}

const std::string& BSP::GetAxis() const
{
	static std::string sX = "X";
	static std::string sY = "Y";
	static std::string sZ = "Z";
	static std::string sNone = "None";
	switch (m_axis)
	{
	case AxisSplit::X: return sX;
	case AxisSplit::Y: return sY;
	case AxisSplit::Z: return sZ;
	}
	return sNone;
}

const BoundingBox& BSP::GetBoundingBox() const
{
	return m_bbox;
}

const std::vector<size_t>& BSP::GetQuadblockIndexes() const
{
	return m_quadblockIndexes;
}

const BSP* BSP::GetLeftChildren() const
{
	return m_left;
}

const BSP* BSP::GetRightChildren() const
{
	return m_right;
}

const std::vector<const BSP*> BSP::GetTree() const
{
	size_t i = 0;
	std::vector<const BSP*> bspNodes = { this };
	while (i < bspNodes.size())
	{
		const BSP* currNode = bspNodes[i++];
		const BSP* leftNode = currNode->m_left;
		if (leftNode) { bspNodes.push_back(leftNode); }
		const BSP* rightNode = currNode->m_right;
		if (rightNode) { bspNodes.push_back(rightNode); }
	}
	return bspNodes;
}

std::vector<const BSP*> BSP::GetLeaves() const
{
	std::vector<const BSP*> ret;
	const std::vector<const BSP*> tree = GetTree();
	for (const BSP* bsp : tree)
	{
		if (bsp->IsBranch()) { continue; }
		for (size_t index : bsp->m_quadblockIndexes) { ret.push_back(bsp); }
	}
	return ret;
}

void BSP::SetQuadblockIndexes(const std::vector<size_t>& quadblockIndexes)
{
	m_quadblockIndexes = quadblockIndexes;
}

void BSP::Clear()
{
	std::vector<BSP*> vBSP = {this};
	size_t currIndex = 0;
	while (currIndex < vBSP.size())
	{
		BSP* pBSP = vBSP[currIndex++];
		if (pBSP->m_right) { vBSP.push_back(pBSP->m_right); }
		if (pBSP->m_left) { vBSP.push_back(pBSP->m_left); }
	}
	for (size_t i = 1; i < vBSP.size(); i++) { delete vBSP[i]; }
	m_right = nullptr;
	m_left = nullptr;
	m_axis = AxisSplit::NONE;
	m_flags = BSPFlags::NONE;
	m_quadblockIndexes.clear();
	g_id = 1;
}

void BSP::Generate(const std::vector<Quadblock>& quadblocks, const size_t maxQuadsPerLeaf, const float maxAxisLength)
{
	m_bbox = ComputeBoundingBox(quadblocks, m_quadblockIndexes);

	bool isLeaf = !IsBranch();
	if (isLeaf)
	{
		if (m_bbox.AxisLength() < maxAxisLength || m_quadblockIndexes.size() == 1) { return; }
		m_node = BSPNode::BRANCH;
		m_flags &= ~BSPFlags::LEAF;
	}

	if (m_quadblockIndexes.size() == 1)
	{
		std::vector<size_t> empty = {};
		GenerateOffspring(m_quadblockIndexes, empty, quadblocks, maxQuadsPerLeaf, maxAxisLength);
		return;
	}

	std::vector<size_t> x_right, x_left, y_right, y_left, z_right, z_left;
	float x_score = Split(x_left, x_right, AxisSplit::X, quadblocks);
	float y_score = Split(y_left, y_right, AxisSplit::Y, quadblocks);
	float z_score = Split(z_left, z_right, AxisSplit::Z, quadblocks);
	float bestScore = std::min(std::min(x_score, y_score), z_score);
	if (bestScore == std::numeric_limits<float>::max())
	{
		if (isLeaf)
		{
			m_node = BSPNode::LEAF;
			m_flags |= BSPFlags::LEAF;
		}
		return;
	}
	if (bestScore == x_score)
	{
		m_axis = AxisSplit::X;
		GenerateOffspring(x_left, x_right, quadblocks, maxQuadsPerLeaf, maxAxisLength);
	}
	else if (bestScore == z_score)
	{
		m_axis = AxisSplit::Z;
		GenerateOffspring(z_left, z_right, quadblocks, maxQuadsPerLeaf, maxAxisLength);
	}
	else
	{
		m_axis = AxisSplit::Y;
		GenerateOffspring(y_left, y_right, quadblocks, maxQuadsPerLeaf, maxAxisLength);
	}
}

std::vector<uint8_t> BSP::Serialize(size_t offQuads) const
{
	return m_node == BSPNode::BRANCH ? SerializeBranch() : SerializeLeaf(offQuads);
}

float BSP::GetAxisMidpoint(const AxisSplit axis) const
{
	float midpoint = 0.0f;
	switch (axis)
	{
	case AxisSplit::X: return m_bbox.Midpoint().x;
	case AxisSplit::Y: return m_bbox.Midpoint().y;
	case AxisSplit::Z: return m_bbox.Midpoint().z;
	}
	return midpoint;
}

BoundingBox BSP::ComputeBoundingBox(const std::vector<Quadblock>& quadblocks, const std::vector<size_t>& quadblockIndexes) const
{
	if (quadblockIndexes.empty()) { return {}; }

	Vec3 min = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	Vec3 max = Vec3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
	for (size_t index : quadblockIndexes)
	{
		const Quadblock& quad = quadblocks[index];
		const BoundingBox& quadBbox = quad.GetBoundingBox();
		min.x = std::min(min.x, quadBbox.min.x); max.x = std::max(max.x, quadBbox.max.x);
		min.y = std::min(min.y, quadBbox.min.y); max.y = std::max(max.y, quadBbox.max.y);
		min.z = std::min(min.z, quadBbox.min.z); max.z = std::max(max.z, quadBbox.max.z);
	}
	BoundingBox bbox = { min, max };
	return bbox;
}

float BSP::Split(std::vector<size_t>& left, std::vector<size_t>& right, const AxisSplit axis, const std::vector<Quadblock>& quadblocks) const
{
	float midpoint = GetAxisMidpoint(axis);
	for (size_t index : m_quadblockIndexes)
	{
		const Quadblock& quad = quadblocks[index];
		switch (axis)
		{
		case AxisSplit::X:
			if (quad.GetCenter().x > midpoint) { right.push_back(index); }
			else { left.push_back(index); }
			break;
		case AxisSplit::Y:
			if (quad.GetCenter().y > midpoint) { right.push_back(index); }
			else { left.push_back(index); }
			break;
		case AxisSplit::Z:
			if (quad.GetCenter().z > midpoint) { right.push_back(index); }
			else { left.push_back(index); }
			break;
		}
	}
	if (left.empty() || right.empty()) { return std::numeric_limits<float>::max(); }
	float perimeterLeft = ComputeBoundingBox(quadblocks, left).SemiPerimeter();
	float perimeterRight = ComputeBoundingBox(quadblocks, right).SemiPerimeter();
	return perimeterLeft + perimeterRight;
}

void BSP::GenerateOffspring(std::vector<size_t>& left, std::vector<size_t>& right, const std::vector<Quadblock>& quadblocks, const size_t maxQuadsPerLeaf, const float maxAxisLength)
{
	if (left.size() < maxQuadsPerLeaf) { if (!left.empty()) { m_left = new BSP(BSPNode::LEAF, left); } }
	else { m_left = new BSP(BSPNode::BRANCH, left); }
	if (m_left) { m_left->Generate(quadblocks, maxQuadsPerLeaf, maxAxisLength); }

	if (right.size() < maxQuadsPerLeaf) { if (!right.empty()) { m_right = new BSP(BSPNode::LEAF, right); } }
	else { m_right = new BSP(BSPNode::BRANCH, right); }
	if (m_right) { m_right->Generate(quadblocks, maxQuadsPerLeaf, maxAxisLength); }
}

std::vector<uint8_t> BSP::SerializeBranch() const
{
	PSX::BSPBranch branch = {};
	std::vector<uint8_t> buffer(sizeof(branch));
	branch.flag = m_flags;
	branch.id = static_cast<uint16_t>(m_id);
	branch.bbox.min = ConvertVec3(m_bbox.min, FP_ONE_GEO);
	branch.bbox.max = ConvertVec3(m_bbox.max, FP_ONE_GEO);
	branch.axis = { 0, 0, 0 };
	switch (m_axis)
	{
	case AxisSplit::X: branch.axis.x = 0x1000; break;
	case AxisSplit::Y: branch.axis.y = 0x1000; break;
	case AxisSplit::Z: branch.axis.z = 0x1000; break;
	}
	if (m_left)
	{
		branch.leftChild = static_cast<uint16_t>(m_left->m_id);
		if (!m_left->IsBranch()) { branch.leftChild |= BSPID::LEAF; }
	}
	else { branch.leftChild = BSPID::EMPTY; }
	if (m_right)
	{
		branch.rightChild = static_cast<uint16_t>(m_right->m_id);
		if (!m_right->IsBranch()) { branch.rightChild |= BSPID::LEAF; }
	}
	else { branch.rightChild = BSPID::EMPTY; }
	branch.unk1 = 0xFF40;
	branch.unk2 = 0;
	branch.unk3 = 0;
	std::memcpy(buffer.data(), &branch, sizeof(branch));
	return buffer;
}

std::vector<uint8_t> BSP::SerializeLeaf(size_t offQuads) const
{
	PSX::BSPLeaf leaf = {};
	std::vector<uint8_t> buffer(sizeof(leaf));
	leaf.flag = m_flags;
	leaf.id = static_cast<uint16_t>(m_id);
	leaf.bbox.min = ConvertVec3(m_bbox.min, FP_ONE_GEO);
	leaf.bbox.max = ConvertVec3(m_bbox.max, FP_ONE_GEO);
	leaf.offHitbox = 0;
	leaf.numQuads = static_cast<uint32_t>(m_quadblockIndexes.size());
	leaf.offQuads = static_cast<uint32_t>(offQuads);
	leaf.unk1 = 0;
	std::memcpy(buffer.data(), &leaf, sizeof(leaf));
	return buffer;
}