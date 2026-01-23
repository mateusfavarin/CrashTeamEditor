#include "path.h"
#include "gui_render_settings.h"
#include <array>

static const std::array<Color, 9> PrimitiveColors = {
	Color(1.0f, 0.0f, 0.0f),
	Color(0.0f, 1.0f, 0.0f),
	Color(0.0f, 0.0f, 1.0f),
	Color(1.0f, 1.0f, 0.0f),
	Color(1.0f, 0.0f, 1.0f),
	Color(0.0f, 0.5f, 1.0f),
	Color(1.0f, 0.5f, 0.0f),
	Color(0.5f, 1.0f, 0.0f),
	Color(1.0f, 0.5f, 0.5f),
};

static Color GetNextPrimitiveColor()
{
	static size_t primitiveColorIndex = 0;
	const Color color = PrimitiveColors[primitiveColorIndex];
	primitiveColorIndex = (primitiveColorIndex + 1) % PrimitiveColors.size();
	return color;
}

Path::Path()
{
	m_index = 0;
	m_start = 0;
	m_end = 0;
	m_left = nullptr;
	m_right = nullptr;
	m_previewValueStart = 0;
	m_previewLabelStart = std::string();
	m_quadIndexesStart = std::vector<size_t>();
	m_previewValueEnd = 0;
	m_previewLabelEnd = std::string();
	m_quadIndexesEnd = std::vector<size_t>();
	m_previewValueIgnore = 0;
	m_previewLabelIgnore = std::string();
	m_quadIndexesIgnore = std::vector<size_t>();
	m_color = GetNextPrimitiveColor();
}

Path::Path(size_t index)
{
	m_index = index;
	m_start = 0;
	m_end = 0;
	m_left = nullptr;
	m_right = nullptr;
	m_previewValueStart = 0;
	m_previewLabelStart = std::string();
	m_quadIndexesStart = std::vector<size_t>();
	m_previewValueEnd = 0;
	m_previewLabelEnd = std::string();
	m_quadIndexesEnd = std::vector<size_t>();
	m_previewValueIgnore = 0;
	m_previewLabelIgnore = std::string();
	m_quadIndexesIgnore = std::vector<size_t>();
	m_color = GetNextPrimitiveColor();
}

Path::Path(const Path& path)
{
	*this = path;
}

Path::~Path()
{
	delete m_left;
	delete m_right;
	m_left = nullptr;
	m_right = nullptr;
}

size_t Path::GetIndex() const
{
	return m_index;
}

size_t Path::GetStart() const
{
	return m_start;
}

size_t Path::GetEnd() const
{
	return m_end;
}

std::vector<size_t>& Path::GetStartIndexes()
{
	return m_quadIndexesStart;
}

std::vector<size_t>& Path::GetEndIndexes()
{
	return m_quadIndexesEnd;
}

std::vector<size_t>& Path::GetIgnoreIndexes()
{
	return m_quadIndexesIgnore;
}

bool Path::IsReady() const
{
	bool left = true; if (m_left) { left = m_left->IsReady(); }
	bool right = true; if (m_right) { right = m_right->IsReady(); }
	return left && right && !m_quadIndexesStart.empty() && !m_quadIndexesEnd.empty();
}

void Path::SetIndex(size_t index)
{
	m_index = index;
}

void Path::UpdateDist(float dist, const Vec3& refPoint, std::vector<Checkpoint>& checkpoints)
{
	dist += (refPoint - checkpoints[m_end].GetPos()).Length();
	size_t currIndex = m_end;
	while (true)
	{
		Checkpoint& currCheckpoint = checkpoints[currIndex];
		currCheckpoint.UpdateDistFinish(dist + currCheckpoint.GetDistFinish());
		if (currIndex == m_start) { break; }
		currIndex = currCheckpoint.GetDown();
	}
	if (m_left) { m_left->UpdateDist(dist, checkpoints[m_end].GetPos(), checkpoints); }
	if (m_right) { m_right->UpdateDist(dist, checkpoints[m_end].GetPos(), checkpoints); }
}

std::vector<Checkpoint> Path::GeneratePath(size_t pathStartIndex, std::vector<Quadblock>& quadblocks)
{
	/*
		Begin from the start point, find all neighbour quadblocks.
		Find the midpoint of the quad group, then find the closest vertex to
		this midpoint. Repeat this process until you're neighboring the end path.
	*/

	size_t visitedCount = 0;
	std::vector<size_t> startEndIndexes;
	std::vector<bool> visitedQuadblocks(quadblocks.size(), false);
	GetStartEndIndexes(startEndIndexes);
	for (const size_t index : startEndIndexes)
	{
		visitedQuadblocks[index] = true;
		visitedCount++;
	}

	for (size_t i = 0; i < quadblocks.size(); i++)
	{
		if (!quadblocks[i].GetCheckpointStatus())
		{
			visitedQuadblocks[i] = true;
			visitedCount++;
		}
	}

	std::vector<size_t> currQuadblocks = m_quadIndexesStart;
	std::vector<std::vector<size_t>> quadIndexesPerChunk;
	while (true)
	{
		std::vector<size_t> nextQuadblocks;
		if (visitedCount < quadblocks.size())
		{
			for (const size_t index : currQuadblocks)
			{
				for (size_t i = 0; i < quadblocks.size(); i++)
				{
					if (!visitedQuadblocks[i] && quadblocks[index].Neighbours(quadblocks[i]))
					{
						nextQuadblocks.push_back(i);
						visitedQuadblocks[i] = true;
						visitedCount++;
					}
				}
			}
		}
		quadIndexesPerChunk.push_back(currQuadblocks);
		if (nextQuadblocks.empty()) { break; }
		currQuadblocks = nextQuadblocks;
	}
	quadIndexesPerChunk.push_back(m_quadIndexesEnd);

	Vec3 lastChunkVertex;
	float distStart = 0.0f;
	std::vector<float> distStarts;
	std::vector<Checkpoint> checkpoints;
	int currCheckpointIndex = static_cast<int>(pathStartIndex);
	for (const std::vector<size_t>& quadIndexSet : quadIndexesPerChunk)
	{
		BoundingBox bbox;
		bbox.min = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		bbox.max = Vec3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		for (const size_t index : quadIndexSet)
		{
			Quadblock& quadblock = quadblocks[index];
			if (!quadblock.GetCheckpointPathable()) { continue; }
			const BoundingBox& quadBbox = quadblock.GetBoundingBox();
			bbox.min.x = std::min(bbox.min.x, quadBbox.min.x); bbox.max.x = std::max(bbox.max.x, quadBbox.max.x);
			bbox.min.y = std::min(bbox.min.y, quadBbox.min.y); bbox.max.y = std::max(bbox.max.y, quadBbox.max.y);
			bbox.min.z = std::min(bbox.min.z, quadBbox.min.z); bbox.max.z = std::max(bbox.max.z, quadBbox.max.z);
		}

		Vec3 chunkVertex;
		size_t chunkQuadIndex = 0;
		Vec3 chunkCenter = bbox.Midpoint();
		float closestDist = std::numeric_limits<float>::max();
		for (const size_t index : quadIndexSet)
		{
			Vec3 closestVertex;
			Quadblock& quadblock = quadblocks[index];
			if (quadblock.GetCheckpointPathable())
			{
				float dist = quadblock.DistanceClosestVertex(closestVertex, chunkCenter);
				if (dist < closestDist)
				{
					closestDist = dist;
					chunkVertex = closestVertex;
					chunkQuadIndex = index;
				}
			}
			quadblock.SetCheckpoint(currCheckpointIndex);
		}
		if (!checkpoints.empty()) { distStart += (lastChunkVertex - chunkVertex).Length(); }
		distStarts.push_back(distStart);
		checkpoints.emplace_back(currCheckpointIndex, chunkVertex, quadblocks[chunkQuadIndex].GetName());
		checkpoints.back().SetColor(m_color);
		checkpoints.back().UpdateUp(currCheckpointIndex + 1);
		checkpoints.back().UpdateDown(currCheckpointIndex - 1);
		currCheckpointIndex++;
		lastChunkVertex = chunkVertex;
	}

	m_start = pathStartIndex;
	m_end = m_start + checkpoints.size() - 1;

	const size_t ckptCount = checkpoints.size();
	for (size_t i = 0; i < ckptCount; i++)
	{
		checkpoints[i].UpdateDistFinish(distStarts[ckptCount - 1] - distStarts[i]);
		// At this moment, ckpt[i].dtf correspond to the distance to the end of the current path
	}

	checkpoints.front().UpdateDown(NONE_CHECKPOINT_INDEX);
	checkpoints.back().UpdateUp(NONE_CHECKPOINT_INDEX);

	pathStartIndex += checkpoints.size();
	std::vector<Checkpoint> leftCheckpoints, rightCheckpoints;
	if (m_left)
	{
		leftCheckpoints = m_left->GeneratePath(pathStartIndex, quadblocks);
		checkpoints.back().UpdateLeft(leftCheckpoints.back().GetIndex());
		checkpoints.front().UpdateLeft(leftCheckpoints.front().GetIndex());
		leftCheckpoints.back().UpdateRight(checkpoints.back().GetIndex());
		leftCheckpoints.front().UpdateRight(checkpoints.front().GetIndex());
		pathStartIndex += leftCheckpoints.size();
	}
	if (m_right)
	{
		rightCheckpoints = m_right->GeneratePath(pathStartIndex, quadblocks);
		checkpoints.back().UpdateRight(rightCheckpoints.back().GetIndex());
		checkpoints.front().UpdateRight(rightCheckpoints.front().GetIndex());
		rightCheckpoints.back().UpdateLeft(checkpoints.back().GetIndex());
		rightCheckpoints.front().UpdateLeft(checkpoints.front().GetIndex());
	}

	for (const Checkpoint& checkpoint : leftCheckpoints) { checkpoints.push_back(checkpoint); }
	for (const Checkpoint& checkpoint : rightCheckpoints) { checkpoints.push_back(checkpoint); }

	return checkpoints;
}

void Path::GetStartEndIndexes(std::vector<size_t>& out) const
{
	if (m_left) { m_left->GetStartEndIndexes(out); }
	if (m_right) { m_right->GetStartEndIndexes(out); }
	for (const size_t index : m_quadIndexesStart) { out.push_back(index); }
	for (const size_t index : m_quadIndexesIgnore) { out.push_back(index); }
	for (const size_t index : m_quadIndexesEnd) { out.push_back(index); }
}

Path& Path::operator=(const Path& path)
{
	if (this == &path) { return *this; }
	m_index = path.m_index;
	m_start = path.m_start;
	m_end = path.m_end;
	if (path.m_left) { m_left = new Path(*path.m_left); }
	else { m_left = nullptr; }
	if (path.m_right) { m_right = new Path(*path.m_right); }
	else { m_right = nullptr; }
	m_previewValueStart = path.m_previewValueStart;
	m_previewLabelStart = path.m_previewLabelStart;
	m_quadIndexesStart = path.m_quadIndexesStart;
	m_previewValueEnd = path.m_previewValueEnd;
	m_previewLabelEnd = path.m_previewLabelEnd;
	m_quadIndexesEnd = path.m_quadIndexesEnd;
	m_previewValueIgnore = path.m_previewValueIgnore;
	m_previewLabelIgnore = path.m_previewLabelIgnore;
	m_quadIndexesIgnore = path.m_quadIndexesIgnore;
	m_color = path.m_color;
	return *this;
}

const Color& Path::GetColor() const
{
	return m_color;
}

void Path::SetColor(const Color& color)
{
	m_color = color;
}
