#include "vistree.h"
#include <omp.h>
#include <cmath>
#include <unordered_set>
#include <chrono>
#include <algorithm>

bool BitMatrix::Get(size_t x, size_t y) const
{
	return m_data[(y * m_width) + x] != 0;
}

size_t BitMatrix::GetWidth() const
{
	return m_width;
}

size_t BitMatrix::GetHeight() const
{
	return m_height;
}

void BitMatrix::Set(bool value, size_t x, size_t y)
{
	m_data[(y * m_width) + x] = value ? 1 : 0;
}

void BitMatrix::SetRow(const std::vector<uint8_t>& rowData, size_t y)
{
	if (rowData.size() != m_width) { return; }
	const size_t rowStart = y * m_width;
	std::copy(rowData.begin(), rowData.end(), m_data.begin() + rowStart);
}

bool BitMatrix::IsEmpty() const
{
	return m_data.empty();
}

void BitMatrix::Clear()
{
	m_width = 0;
	m_height = 0;
	m_data.clear();
}

static bool WorldspaceRayTriIntersection(const Vec3& worldSpaceRayOrigin, const Vec3& worldSpaceRayDir, const std::array<Vec3, 3>& tri, float& dist)
{
	//TODO : merge with renderer
	constexpr float failsafe = 0.5f;
	constexpr float barycentricTolerance = 0.5f;

	//moller-trumbore intersection test
	//https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

	Vec3 edge_1 = tri[1] - tri[0], edge_2 = tri[2] - tri[0];
	Vec3 ray_cross_e2 = worldSpaceRayDir.Cross(edge_2);
	float det = edge_1.Dot(ray_cross_e2);

	if (std::abs(det) < EPSILON) { return false; } // ray is parallel to plane

	float inv_det = 1.0f / det;
	Vec3 s = worldSpaceRayOrigin - tri[0];
	float u = inv_det * s.Dot(ray_cross_e2);

	// Allow u to be slightly outside [0, 1] range
	// u < -tolerance means point is beyond edge 0->1
	// u > 1+tolerance means point is beyond the opposite side
	if (u < -barycentricTolerance || u > 1.0f + barycentricTolerance) {
		return false;
	}

	Vec3 s_cross_e1 = s.Cross(edge_1);
	float v = inv_det * worldSpaceRayDir.Dot(s_cross_e1);

	// Allow v to be slightly outside [0, 1] range
	if (v < -barycentricTolerance || v > 1.0f + barycentricTolerance) { return false; }

	// Check if u+v is within triangle (with tolerance)
	// u+v > 1 means outside the triangle on the hypotenuse edge
	if (u + v > 1.0f + barycentricTolerance) { return false; }

	float t = inv_det * edge_2.Dot(s_cross_e1); // time value (interpolant)

	// Allow hits slightly behind the origin (for edge cases where ray starts on surface)
	if (t > -failsafe)
	{
		dist = t;
		return true;
	}

	return false;
}

static bool RayIntersectQuadblockTest(const Vec3& worldSpaceRayOrigin, const Vec3& worldSpaceRayDir, const Quadblock& qb, float& dist)
{
	std::vector<std::array<size_t, 3>> triFacesID = qb.GetTriFacesIndexes();
	for (const std::array<size_t, 3>&ids : triFacesID)
	{
		if (WorldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, qb.GetTriFace(ids[0], ids[1], ids[2]), dist)) { return true; }
	}
	return false;
}

// Ray-AABB intersection test using the slab method
// Returns true if the ray intersects the bounding box, and sets dist to the distance to the nearest intersection
static bool RayIntersectBoundingBox(const Vec3& rayOrigin, const Vec3& rayDir, const BoundingBox& bbox, float& tmin, float& tmax)
{
	constexpr float epsilon = 0.00001f;
	constexpr float failsafe = 0.5f;

	// Compute inverse ray direction for efficiency
	float invDirX = 1.0f / (std::abs(rayDir.x) < epsilon ? epsilon : rayDir.x);
	float invDirY = 1.0f / (std::abs(rayDir.y) < epsilon ? epsilon : rayDir.y);
	float invDirZ = 1.0f / (std::abs(rayDir.z) < epsilon ? epsilon : rayDir.z);

	// Compute intersection distances for each pair of slabs
	float t1 = (bbox.min.x - rayOrigin.x) * invDirX;
	float t2 = (bbox.max.x - rayOrigin.x) * invDirX;
	float t3 = (bbox.min.y - rayOrigin.y) * invDirY;
	float t4 = (bbox.max.y - rayOrigin.y) * invDirY;
	float t5 = (bbox.min.z - rayOrigin.z) * invDirZ;
	float t6 = (bbox.max.z - rayOrigin.z) * invDirZ;

	// Find the min and max for each axis
	tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
	tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

	// Failsafe : if the rayOrigin is almost inside the BBox, we return true
	if (bbox.min.x - failsafe < rayOrigin.x && rayOrigin.x < bbox.max.x + failsafe
		&& bbox.min.y - failsafe < rayOrigin.y && rayOrigin.y < bbox.max.y + failsafe
		&& bbox.min.z - failsafe < rayOrigin.z && rayOrigin.z < bbox.max.z + failsafe)
	{
		tmin = -1.0f; // We fake tmin negative with a true return to say we are inside
		return true;
	}
	// No intersection if tmax < 0 (box is behind ray) or tmin > tmax (ray misses box)
	if (tmax < 0.0f || tmin > tmax) { return false; }
	return true;
}

// Helper structure to store leaf data with its tmax value
struct LeafWithDistance
{
	std::vector<size_t> quadIndexes; // All quads in this leaf
	float tmax; // Distance to the far side of the leaf's bounding box
};

// Recursively traverse BSP tree to collect leaves with their distances
static void GetPotentialLeavesRecursive(
	const std::vector<Quadblock>& quadblocks,
	const BSP* node,
	const Vec3& rayOrigin,
	const Vec3& rayDir,
	const float maxDist,
	std::vector<LeafWithDistance>& result)
{
	float tmin, tmax;
	// Check if the ray intersects this node's bounding box
	if (!RayIntersectBoundingBox(rayOrigin, rayDir, node->GetBoundingBox(), tmin, tmax)) { return; }

	// Check if the node's bounding box is too far away
	if (tmin > maxDist) { return; }

	// If this is a leaf node, add it's LeafWithDistance
	if (!node->IsBranch())
	{
		result.push_back({node->GetQuadblockIndexes(), tmax});
		return;
	}

	const BSP* leftChild = node->GetLeftChildren();
	const BSP* rightChild = node->GetRightChildren();
	if (leftChild != nullptr)
	{
		GetPotentialLeavesRecursive(quadblocks, leftChild, rayOrigin, rayDir, maxDist, result);
	}

	if (rightChild != nullptr)
	{
		GetPotentialLeavesRecursive(quadblocks, rightChild, rayOrigin, rayDir, maxDist, result);
	}
}

static std::vector<size_t> GetPotentialQuadblockIndexes(
	const std::vector<Quadblock>& quadblocks,
	const BSP* node,
	const Vec3& rayOrigin,
	const Vec3& rayDir,
	const size_t leafBID,
	const float maxDist)
{
	// Return all the quadblock that can potentially block the ray, sorted by leaf's Bbox tmax. (close quads have higher chance to block the ray)

	std::vector<LeafWithDistance> leavesWithDist;
	GetPotentialLeavesRecursive(quadblocks, node, rayOrigin, rayDir, maxDist, leavesWithDist);

	std::sort(leavesWithDist.begin(), leavesWithDist.end(),
		[](const LeafWithDistance& a, const LeafWithDistance& b) { return a.tmax < b.tmax; });

	std::vector<size_t> result;
	for (const LeafWithDistance& leaf : leavesWithDist)
	{
		for (size_t quadID : leaf.quadIndexes)
		{
			const Quadblock& quad = quadblocks[quadID];
			if (quad.GetBSPID() == leafBID || !quad.GetVisTreeTransparent())
			{
				// Ideally, a quad has 8 normal, but I just test 2
				// Fix for triblocks please
				if (quad.GetDrawDoubleSided() || quad.ComputeNormalVector(0, 2, 6).Dot(rayDir) < 0 || quad.ComputeNormalVector(2, 8, 6).Dot(rayDir) < 0)
				{
					result.push_back(quadID);
				}
			}
		}
	}

	return result;
}

static std::vector<Vec3> GenerateSamplePointLeaf(const std::vector<Quadblock>& quadblocks, const BSP& leaf, float camera_raise, bool centerOnlySamples)
{
	// For a leaf node, generate all the points for the vis ray test.
	std::vector<Vec3> samples;
	const std::vector<size_t>& quadIndexes = leaf.GetQuadblockIndexes();
	const Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
	const float dedupeThreshold = 0.5f;
	const float dedupeThresholdSquared = dedupeThreshold * dedupeThreshold;

	// Helper to check if a point already exists in samples
	auto isDuplicate = [&samples, dedupeThresholdSquared](const Vec3& point)
		{
			for (const Vec3& existing : samples)
			{
				if ((existing - point).LengthSquared() < dedupeThresholdSquared) { return true; }
			}
			return false;
		};

	// Helper to add point if not duplicate
	auto addIfUnique = [&samples, &isDuplicate](const Vec3& point, bool end)
		{
			if (!isDuplicate(point))
			{
				if (end) { samples.push_back(point); }
				else { samples.insert(samples.begin(), point); }
			}
		};

	for (size_t quadID : quadIndexes)
	{
		Quadblock quad = quadblocks[quadID];
		float up_dist = 0.0f;
		if (quad.GetFlags() & QuadFlags::GROUND) { up_dist = camera_raise; }

		addIfUnique(quad.GetCenter() + (up * up_dist), false);
		if (!centerOnlySamples)
		{
			if (quad.IsQuadblock())
			{
				const Vertex* verts = quad.GetUnswizzledVertices();
				addIfUnique(verts[0].m_pos, true);
				addIfUnique(verts[2].m_pos, true);
				addIfUnique(verts[6].m_pos, true);
				addIfUnique(verts[8].m_pos, true);
			}
			else
			{
				const Vertex* verts = quad.GetUnswizzledVertices();
				addIfUnique(verts[0].m_pos, true);
				addIfUnique(verts[2].m_pos, true);
				addIfUnique(verts[6].m_pos, true);
			}
		}
	}
	return samples;
}

static float GetLeafDistanceSquared(const BSP& leaf1, const BSP& leaf2)
{
	// Return the closest distance between the BBox of leaf1 and leaf2.
	// Return 0.0f if they are intersecting, or one is included in the other
	const BoundingBox& a = leaf1.GetBoundingBox();
	const BoundingBox& b = leaf2.GetBoundingBox();

	float dx = std::max({0.0f, b.min.x - a.max.x, a.min.x - b.max.x});
	float dy = std::max({0.0f, b.min.y - a.max.y, a.min.y - b.max.y});
	float dz = std::max({0.0f, b.min.z - a.max.z, a.min.z - b.max.z});
	return (dx * dx + dy * dy + dz * dz);
}

BitMatrix GenerateVisTree(const std::vector<Quadblock>& quadblocks, const BSP* root, const VisTreeSettings& settings)
{
	auto start_time = std::chrono::high_resolution_clock::now();

	const float maxDistanceSquared = settings.farClipDistance * settings.farClipDistance;
	std::vector<const BSP*> leaves = root->GetLeaves();
	BitMatrix vizMatrix = BitMatrix(leaves.size(), leaves.size());
	const int leafCount = static_cast<int>(leaves.size());

	const float cameraHeight = 5.0f;
	const float failsafe = 0.5f;

	const int quadCount = static_cast<int>(quadblocks.size());
	std::vector<size_t> quadIndexesToLeaves(quadCount);
	for (size_t i = 0; i < leaves.size(); i++)
	{
		const std::vector<size_t>& quadIndexes = leaves[i]->GetQuadblockIndexes();
		for (size_t index : quadIndexes) { quadIndexesToLeaves[index] = i; }
	}

	std::vector<std::vector<Vec3>> sourceSamples(leaves.size());
	std::vector<std::vector<Vec3>> targetSamples(leaves.size());
	for (size_t i = 0; i < leaves.size(); i++)
	{
		sourceSamples[i] = GenerateSamplePointLeaf(quadblocks, *leaves[i], cameraHeight, settings.centerOnlySamples);
		targetSamples[i] = GenerateSamplePointLeaf(quadblocks, *leaves[i], 0.0f, settings.centerOnlySamples);
	}

	std::vector<std::vector<uint8_t>> visibilityRows(leaves.size(), std::vector<uint8_t>(leaves.size(), 0));
#pragma omp parallel for schedule(dynamic)
	for (int leafAInt = 0; leafAInt < leafCount; leafAInt++)
	{
		const size_t leafA = static_cast<size_t>(leafAInt);
		const int leafBStart = settings.commutativeRays ? leafAInt : 0;
		for (int leafBInt = leafBStart; leafBInt < leafCount; leafBInt++)
		{
			const size_t leafB = static_cast<size_t>(leafBInt);

			bool foundLeafABHit = (leafA == leafB);
			const std::vector<Vec3>& sampleA = sourceSamples[leafA];
			const std::vector<Vec3>& sampleB = targetSamples[leafB];

			float distBboxsquared = GetLeafDistanceSquared(*leaves[leafA], *leaves[leafB]);
			// If minDistance is positive, and bigger than distBbox
			if ((settings.nearClipDistance > -EPSILON) && (settings.nearClipDistance * settings.nearClipDistance >= distBboxsquared))
			{
				foundLeafABHit = true;
			}

			for (const Vec3& pointA : sampleA)
			{
				if (foundLeafABHit) { break; }

				for (const Vec3& pointB : sampleB)
				{
					if (foundLeafABHit) { break; }
					Vec3 directionVector = pointB - pointA;
					if (directionVector.LengthSquared() > maxDistanceSquared) { continue; }
					directionVector.Normalize();

					// Calculate distance range to leafB's bounding box
					float tmin, tmax;
					const BoundingBox& bboxB = leaves[leafB]->GetBoundingBox();
					bool intersect = RayIntersectBoundingBox(pointA, directionVector, bboxB, tmin, tmax);
					if (!intersect)
					{
						// Weird edge case that shouldn't happen, but still does...
						continue;
					}
					if (tmin < 0.0f)
					{
						// We are inside the Bbox.
						foundLeafABHit = true;
						break;
					}

					std::vector<size_t> potentialQuads = GetPotentialQuadblockIndexes(quadblocks, root, pointA, directionVector, leaves[leafB]->GetId(), tmax);

					float closestDist = std::numeric_limits<float>::max();
					size_t closestLeaf = leafA;
					bool foundBlockingQuad = false;

					// Single loop: test quads and check for blocking simultaneously
					for (size_t i = 0; i < potentialQuads.size(); i++)
					{
						if (foundBlockingQuad) { break; }

						size_t testQuadIndex = potentialQuads[i];
						size_t quadLeaf = quadIndexesToLeaves[testQuadIndex];

						float dist = 0.0f;
						const Quadblock& testQuad = quadblocks[testQuadIndex];

						if (RayIntersectQuadblockTest(pointA, directionVector, testQuad, dist))
						{
							// Early exit check: if quad hits before tmin and is not from leafB, it's blocking
							if (quadLeaf != leafB && (dist + failsafe < tmin))
							{
								foundBlockingQuad = true;
								break;
							}

							if (dist - (quadLeaf == leafB ? failsafe : 0.0f) < closestDist - (closestLeaf == leafB ? failsafe : 0.0f))
							{
								closestDist = dist;
								closestLeaf = quadLeaf;
							}
						}
					}

					// If we found a blocking quad, skip to next pointB
					if (foundBlockingQuad) { continue; }

					if (closestLeaf == leafB) { foundLeafABHit = true; }
				}
			}

			if (foundLeafABHit)
			{
				visibilityRows[leafA][leafB] = 1;
				if (settings.commutativeRays) { visibilityRows[leafB][leafA] = 1; }
			}
		}
	}

	for (size_t leafA = 0; leafA < leaves.size(); leafA++)
	{
		vizMatrix.SetRow(visibilityRows[leafA], leafA);
	}

	int count = 0;
	for (size_t leafA = 0; leafA < leaves.size(); leafA++)
	{
		for (size_t leafB = 0; leafB < leaves.size(); leafB++)
		{
			if (vizMatrix.Get(leafA, leafB)) { count++; }
		}
	}
	int max = static_cast<int>(leaves.size() * leaves.size());
	float ratio = 100.0f * static_cast<float>(count) / static_cast<float>(max);
	printf("Visibility: %d/%d,  %f%%\n", count, max, ratio);

	auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
	long long total_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
	long long mins = total_seconds / 60;
	long long secs = total_seconds % 60;

	printf("Runtime %lldmin %lldsec\n", mins, secs);
	return vizMatrix;
}
