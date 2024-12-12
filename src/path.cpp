#include "path.h"

#include <algorithm>

Path::Path(size_t index)
{
	m_index = index;
	m_previewValueStart = 0;
	m_previewLabelStart = std::string();
	m_quadIndexesStart = std::vector<size_t>();
	m_previewValueEnd = 0;
	m_previewLabelEnd = std::string();
	m_quadIndexesEnd = std::vector<size_t>();
}

bool Path::Ready() const
{
	return !m_quadIndexesStart.empty() && !m_quadIndexesEnd.empty();
}

std::vector<Checkpoint> Path::GeneratePath(size_t pathStartIndex, std::vector<Quadblock>& quadblocks) const
{
	/*
		Begin from the start point, find all neighbour quadblocks.
		Find the midpoint of the quad group, then find the closest vertex to
		this midpoint. Repeat this process until you're neighboring the end path.
	*/

	size_t visitedCount = 0;
	std::vector<bool> visitedQuadblocks(quadblocks.size(), false);
	for (const size_t index : m_quadIndexesStart)
	{
		visitedQuadblocks[index] = true;
		visitedCount++;
	}
	for (const size_t index : m_quadIndexesEnd)
	{
		visitedQuadblocks[index] = true;
		visitedCount++;
	}
	for (size_t i = 0; i < quadblocks.size(); i++)
	{
		if (!(quadblocks[i].Flags() & QuadFlags::GROUND))
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

	std::vector<Vec3> chunkPos;
	std::vector<size_t> chunkPosQuadIndex;
	const size_t numPotentialCheckpoints = quadIndexesPerChunk.size();
	for (const std::vector<size_t>& quadIndexSet : quadIndexesPerChunk)
	{
		BoundingBox bbox;
		bbox.min = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		bbox.max = Vec3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		for (const size_t index : quadIndexSet)
		{
			const BoundingBox& quadBbox = quadblocks[index].GetBoundingBox();
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
			float dist = quadblocks[index].DistanceClosestVertex(closestVertex, chunkCenter);
			if (dist < closestDist)
			{
				closestDist = dist;
				chunkVertex = closestVertex;
				chunkQuadIndex = index;
			}
		}
		chunkPos.push_back(chunkVertex);
		chunkPosQuadIndex.push_back(chunkQuadIndex);
	}

	/*
		Checkpoints need to be close enough to each other, otherwise
		lap counts can happen in random places. However, the engine has a limit
		of 255 checkpoints per track, so making checkpoints too close is an issue too.
		Treshold of 12.0f distance is being used in order to group checkpoints too close together.

		TODO: handle when checkpoints are too far apart. Maybe create "ghost" checkpoints that aren't
		assigned to any quadblock, however this might run into the 255 limit too fast.
	*/

	std::vector<Checkpoint> checkpoints;
	constexpr float MAX_DIST_BETWEEN_CHECKPOINTS = 12.0f;
	std::vector<std::vector<std::vector<size_t>*>> quadGroups;

	float distFinish = 0.0f;
	size_t checkpointCount = 0;
	int currIndex = static_cast<int>(numPotentialCheckpoints) - 1;
	Vec3 lastCheckpointPos = chunkPos[currIndex];
	while (currIndex >= 0)
	{
		int nextIndex;
		float distToLastChunk = (lastCheckpointPos - chunkPos[currIndex]).Length();
		for (nextIndex = currIndex - 1; nextIndex >= 0; nextIndex--)
		{
			if (currIndex == static_cast<int>(numPotentialCheckpoints) - 1) { break; }
			float chunkDistance = (chunkPos[nextIndex + 1] - chunkPos[nextIndex]).Length();
			if (distToLastChunk + chunkDistance > MAX_DIST_BETWEEN_CHECKPOINTS) { break; }
			distToLastChunk += chunkDistance;
		}
		quadGroups.push_back(std::vector<std::vector<size_t>*>());
		distFinish += distToLastChunk;
		checkpoints.emplace_back(distFinish, chunkPos[nextIndex + 1], quadblocks[chunkPosQuadIndex[nextIndex + 1]].Name());
		lastCheckpointPos = chunkPos[nextIndex + 1];
		while (currIndex > nextIndex)
		{
			quadGroups[checkpointCount].push_back(&quadIndexesPerChunk[currIndex]);
			currIndex--;
		}
		checkpointCount++;
	}

	/*
		We finally have every checkpoint for this path.
		Assign IDs for them and to the quad group.
	*/

	int finalCheckpointIndex = static_cast<int>(pathStartIndex) + static_cast<int>(checkpointCount) - 1;
	for (size_t i = 0; i < checkpoints.size(); i++)
	{
		checkpoints[i].UpdateIndex(finalCheckpointIndex);
		checkpoints[i].UpdateUp(finalCheckpointIndex + 1);
		checkpoints[i].UpdateDown(finalCheckpointIndex - 1);

		std::vector<std::vector<size_t>*>& quadGroup = quadGroups[i];
		for (std::vector<size_t>* group : quadGroup)
		{
			for (const size_t index : *group)
			{
				quadblocks[index].SetCheckpoint(finalCheckpointIndex);
			}
		}
		finalCheckpointIndex--;
	}

	checkpoints.front().UpdateUp(NONE_CHECKPOINT_INDEX);
	checkpoints.back().UpdateDown(NONE_CHECKPOINT_INDEX);
	std::reverse(checkpoints.begin(), checkpoints.end());
	return checkpoints;
}

