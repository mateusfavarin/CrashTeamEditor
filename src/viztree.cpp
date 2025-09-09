#include "viztree.h"
#define _USE_MATH_DEFINES
#include "math.h"

#include <random>

bool BitMatrix::Get(size_t x, size_t y) const
{
	return m_data[(y * m_width) + x];
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
	m_data[(y * m_width) + x] = value;
}

static bool WorldspaceRayTriIntersection(const Vec3& worldSpaceRayOrigin, const Vec3& worldSpaceRayDir, const Vec3* tri, float& dist)
{
	constexpr float epsilon = 0.00001;

	//moller-trumbore intersection test
	//https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

	Vec3 edge_1 = tri[1] - tri[0], edge_2 = tri[2] - tri[0];
	Vec3 ray_cross_e2 = worldSpaceRayDir.Cross(edge_2);
	float det = edge_1.Dot(ray_cross_e2);

	if (std::abs(det) < epsilon) { return false; } // ray is parallel to plane, couldn't possibly intersect with triangle.

	float inv_det = 1.0f / det;
	Vec3 s = worldSpaceRayOrigin - tri[0];
	float u = inv_det * s.Dot(ray_cross_e2);

	if ((u < 0 && std::abs(u) > epsilon) || (u > 1 && std::abs(u - 1.0f) > epsilon)) { return false; } //fails a barycentric test

	Vec3 s_cross_e1 = s.Cross(edge_1);
	float v = inv_det * worldSpaceRayDir.Dot(s_cross_e1);

	if ((v < 0 && std::abs(v) > epsilon) || (u + v > 1 && std::abs(u + v - 1) > epsilon)) { return false; } //fails a barycentric test

	float t = inv_det * edge_2.Dot(s_cross_e1); // time value (interpolant)
	if (t > epsilon) { dist = std::abs(t); return true; }
	return false;
}

static bool RayIntersectQuadblockTest(const Vec3& worldSpaceRayOrigin, const Vec3& worldSpaceRayDir, const Quadblock& qb, float& dist)
{
	bool isQuadblock = qb.IsQuadblock();
	const Vertex* verts = qb.GetUnswizzledVertices();

	Vec3 tris[3];
	bool collided = false;
	if (isQuadblock)
	{
		for (int triIndex = 0; triIndex < 8; triIndex++)
		{
			tris[0] = Vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.z);
			tris[1] = Vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.z);
			tris[2] = Vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.z);
			if (WorldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, tris, dist)) { return true; }
		}
	}
	else
	{
		for (int triIndex = 0; triIndex < 4; triIndex++)
		{
			tris[0] = Vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.z);
			tris[1] = Vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.z);
			tris[2] = Vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.z);
			if (WorldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, tris, dist)) { return true; }
		}
	}
	return false;
}

BitMatrix GenerateVisTree(const std::vector<Quadblock>& quadblocks, const std::vector<const BSP*>& leaves)
{
	BitMatrix vizMatrix = BitMatrix(leaves.size(), leaves.size());
	std::unordered_map<size_t, size_t> quadIndexesToLeaves;
	for (size_t i = 0; i < leaves.size(); i++)
	{
		std::vector<size_t> quadIndexes = leaves[i]->GetQuadblockIndexes();
		for (size_t index : quadIndexes) { quadIndexesToLeaves[index] = i; }
	}

	for (size_t leafA = 0; leafA < leaves.size(); leafA++)
	{
		vizMatrix.Set(true, leafA, leafA);
		for (size_t leafB = 0; leafB < leaves.size(); leafB++)
		{
			if (vizMatrix.Get(leafA, leafB)) { continue; }

			bool foundLeafABHit = false;
			std::vector<size_t> quadIndexesA = leaves[leafA]->GetQuadblockIndexes();
			std::vector<size_t> quadIndexesB = leaves[leafB]->GetQuadblockIndexes();

			for (size_t quadA : quadIndexesA)
			{
				if (foundLeafABHit) { break; }

				Quadblock sourceQuad = quadblocks[quadA];
				sourceQuad.TranslateNormalVec(0.5f);
				const Vec3& center = sourceQuad.GetUnswizzledVertices()[4].m_pos;
				for (size_t quadB : quadIndexesB)
				{
					if (foundLeafABHit) { break; }

					const Quadblock& directionQuad = quadblocks[quadB];
					const Vertex* directionVertices = directionQuad.GetUnswizzledVertices();
					const Vec3& directionPos = directionVertices[4].m_pos;
					Vec3 directionVector = directionPos - center;
					directionVector.Normalize();

					size_t leafHit = leafA;
					float bestDistPos = std::numeric_limits<float>::max();
					for (size_t testQuadIndex = 0; testQuadIndex < quadblocks.size(); testQuadIndex++)
					{
						const Quadblock& testQuad = quadblocks[testQuadIndex];
						const Vertex* testVertices = directionQuad.GetUnswizzledVertices();
						float dist = 0.0f;
						if (!RayIntersectQuadblockTest(center, directionVector, testQuad, dist)) { continue; }
						if (dist < bestDistPos)
						{
							bestDistPos = dist;
							leafHit = quadIndexesToLeaves[testQuadIndex];
						}
					}

					if (!vizMatrix.Get(leafA, leafHit))
					{
						vizMatrix.Set(true, leafA, leafHit);
						vizMatrix.Set(true, leafHit, leafA);
						if (leafHit == leafB) { foundLeafABHit = true; }
					}
				}
			}
		}
	}
	return vizMatrix;
}