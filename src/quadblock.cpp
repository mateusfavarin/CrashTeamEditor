#include "quadblock.h"
#include "psx_types.h"

#include <unordered_map>
#include <unordered_set>
#include <cstring>

Quadblock::Quadblock(const std::string& name, Tri& t0, Tri& t1, Tri& t2, Tri& t3, const Vec3& normal, const std::string& material)
{
	std::unordered_map<Vec3, unsigned> vRefCount;
	for (size_t i = 0; i < 3; i++)
	{
		vRefCount[t0.p[i].pos]++; vRefCount[t1.p[i].pos]++; vRefCount[t2.p[i].pos]++; vRefCount[t3.p[i].pos]++;
	}

	size_t uniqueCount = 0;
	size_t sharedCount = 0;
	for (const auto& [v, count] : vRefCount)
	{
		if (count == 1) { uniqueCount++; }
		else if (count == 3) { sharedCount++; }
	}

	bool validTriblock = (uniqueCount == 3) && (sharedCount == 3);
	if (!validTriblock)
	{
		throw std::exception(
			("Unique Vertices: " + std::to_string(uniqueCount) + "/3\n" +
			 "Shared Vertices: " + std::to_string(sharedCount) + "/3\n").c_str()
		);
	}

	auto FindCenterTri = [&vRefCount](Tri*& out, Tri& tri, std::vector<Tri*>& adjTris)
		{
			if (out != nullptr)
			{
				adjTris.push_back(&tri);
				return;
			}
			for (size_t i = 0; i < 3; i++)
			{
				if (vRefCount[tri.p[i].pos] != 3)
				{
					adjTris.push_back(&tri);
					return;
				}
			}
			out = &tri;
		};

	Tri* centerTri = nullptr;
	std::vector<Tri*> adjTris;
	FindCenterTri(centerTri, t0, adjTris); FindCenterTri(centerTri, t1, adjTris);
	FindCenterTri(centerTri, t2, adjTris); FindCenterTri(centerTri, t3, adjTris);

	auto FindUniquePoint = [&vRefCount](Tri& tri, std::vector<const Point*>& adjPts) -> const Point*
		{
			Point* ret = nullptr;
			for (size_t i = 0; i < 3; i++)
			{
				if (vRefCount[tri.p[i].pos] != 3) { ret = &tri.p[i]; }
				else { adjPts.push_back(&tri.p[i]); }
			}
			return ret;
		};

	std::vector<const Point*> q0Adjs;
	m_p[0] = Vertex(*FindUniquePoint(*adjTris[0], q0Adjs));
	m_p[1] = Vertex(*q0Adjs[0]);
	m_p[3] = Vertex(*q0Adjs[1]);

	bool q1Next = false;
	for (size_t i = 0; i < 3; i++)
	{
		if (adjTris[1]->p[i].pos == m_p[1].m_pos) { q1Next = true; }
	}
	if (!q1Next) { Swap(adjTris[1], adjTris[2]); }

	std::vector<const Point*> q2Adjs;
	m_p[2] = Vertex(*FindUniquePoint(*adjTris[1], q2Adjs));
	m_p[4] = q2Adjs[0]->pos == m_p[1].m_pos ? Vertex(*q2Adjs[1]) : Vertex(*q2Adjs[0]);

	std::vector<const Point*> q4Adjs;
	m_p[6] = *FindUniquePoint(*adjTris[2], q4Adjs);

	Vec3 quadNormal = ComputeNormalVector(0, 2, 6);
	quadNormal = quadNormal / quadNormal.Length();
	Vec3 invertedQuadNormal = quadNormal * -1;
	if ((normal - invertedQuadNormal).Length() < (normal - quadNormal).Length())
	{
		Swap(m_p[0], m_p[2]);
		Swap(m_p[3], m_p[4]);
	}

	m_p[5] = m_p[4];
	m_p[7] = m_p[4];
	m_p[8] = m_p[4];

	ComputeBoundingBox();
	m_name = name;
	m_material = material;
	m_triblock = false;
	m_checkpointIndex = -1;
	m_flags = QuadFlags::DEFAULT;
	m_terrain = TerrainType::LABELS.at(TerrainType::DEFAULT);

	for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
	{
		m_faceDrawMode[i] = FaceDrawMode::DRAW_BOTH;
		m_faceRotateFlip[i] = FaceRotateFlip::NONE;
	}
	m_doubleSided = false;
	m_checkpointStatus = true;
}

Quadblock::Quadblock(const std::string& name, Quad& q0, Quad& q1, Quad& q2, Quad& q3, const Vec3& normal, const std::string& material)
{
	std::unordered_map<Vec3, unsigned> vRefCount;
	for (size_t i = 0; i < 4; i++)
	{
		vRefCount[q0.p[i].pos]++; vRefCount[q1.p[i].pos]++; vRefCount[q2.p[i].pos]++; vRefCount[q3.p[i].pos]++;
	}

	Vec3 centerVertex;
	std::unordered_set<Vec3> uniqueVertices;
	size_t uniqueCount = 0;
	size_t sharedCount = 0;
	size_t centerCount = 0;
	for (const auto& [v, count] : vRefCount)
	{
		if (count == 1) { uniqueVertices.insert(v); uniqueCount++; }
		else if (count == 2) { sharedCount++; }
		else if (count == 4) { centerVertex = v; centerCount++; }
	}

	bool validQuadblock = (uniqueCount == 4) && (sharedCount == 4) && (centerCount == 1);
	if (!validQuadblock)
	{
		throw std::exception(
			("Unique Vertices: " + std::to_string(uniqueCount) + "/4\n" +
			"Shared Vertices: " + std::to_string(sharedCount) + "/4\n" +
			"Center Vertices: " + std::to_string(centerCount) + "/1\n").c_str()
		);
	}

	bool foundCenter = false;
	for (size_t i = 0; i < 4; i++)
	{
		if (uniqueVertices.contains(q0.p[i].pos)) { m_p[0] = Vertex(q0.p[i]); }
		if (uniqueVertices.contains(q1.p[i].pos)) { m_p[2] = Vertex(q1.p[i]); }
		if (uniqueVertices.contains(q2.p[i].pos)) { m_p[6] = Vertex(q2.p[i]); }
		if (uniqueVertices.contains(q3.p[i].pos)) { m_p[8] = Vertex(q3.p[i]); }
		if (!foundCenter && centerVertex == q0.p[i].pos)
		{
			m_p[4] = Vertex(q0.p[i]);
			foundCenter = true;
		}
	}

	auto FindAdjacentPoint = [](const Quad& from, const Quad& to, const Vec3& ignore) -> const Point*
		{
			for (size_t i = 0; i < 4; i++)
			{
				if (from.p[i].pos == ignore) { continue; }
				for (size_t j = 0; j < 4; j++)
				{
					if (from.p[i].pos == to.p[j].pos) { return &from.p[i]; }
				}
			}
			return nullptr;
		};

	const Point* p1 = FindAdjacentPoint(q0, q1, centerVertex);
	const Point* p3 = FindAdjacentPoint(q0, q2, centerVertex);
	if (!p1)
	{
		Swap(m_p[2], m_p[8]);
		Swap(q1, q3);
	}
	else if (!p3)
	{
		Swap(m_p[6], m_p[8]);
		Swap(q2, q3);
	}

	m_p[1] = p1 ? Vertex(*p1) : Vertex(*FindAdjacentPoint(q0, q1, centerVertex));
	m_p[3] = p3 ? Vertex(*p3) : Vertex(*FindAdjacentPoint(q0, q2, centerVertex));
	m_p[5] = Vertex(*FindAdjacentPoint(q1, q3, centerVertex));
	m_p[7] = Vertex(*FindAdjacentPoint(q2, q3, centerVertex));

	Vec3 quadNormal = ComputeNormalVector(0, 2, 6);
	quadNormal = quadNormal / quadNormal.Length();
	Vec3 invertedQuadNormal = quadNormal * -1;
	if ((normal - invertedQuadNormal).Length() < (normal - quadNormal).Length())
	{
		Swap(m_p[0], m_p[2]);
		Swap(m_p[3], m_p[5]);
		Swap(m_p[6], m_p[8]);
	}

	ComputeBoundingBox();
	m_name = name;
	m_material = material;
	m_triblock = false;
	m_checkpointIndex = -1;
	m_flags = QuadFlags::DEFAULT;
	m_terrain = TerrainType::LABELS.at(TerrainType::DEFAULT);
	for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
	{
		m_faceDrawMode[i] = FaceDrawMode::DRAW_BOTH;
		m_faceRotateFlip[i] = FaceRotateFlip::NONE;
	}
	m_doubleSided = false;
	m_checkpointStatus = true;
}

const std::string& Quadblock::Name() const
{
	return m_name;
}

const Vec3& Quadblock::Center() const
{
	return m_p[4].m_pos;
}

uint8_t Quadblock::Terrain() const
{
	return m_terrain;
}

uint16_t Quadblock::Flags() const
{
	return m_flags;
}

bool& Quadblock::CheckpointStatus()
{
	return m_checkpointStatus;
}

void Quadblock::SetTerrain(uint8_t terrain)
{
	m_terrain = terrain;
}

void Quadblock::SetFlag(uint16_t flag)
{
	m_flags = flag;
}

void Quadblock::SetCheckpoint(int index)
{
	m_checkpointIndex = index;
}

void Quadblock::SetDrawDoubleSided(bool active)
{
	m_doubleSided = active;
}

const BoundingBox& Quadblock::GetBoundingBox() const
{
	return m_bbox;
}

std::vector<Vertex> Quadblock::GetVertices() const
{
	/*                                 0       1       2       3       4       5       6       7       8    */
	std::vector<Vertex> vertices = { m_p[0], m_p[2], m_p[6], m_p[8], m_p[1], m_p[3], m_p[4], m_p[5], m_p[7] };
	return vertices;
}

float Quadblock::DistanceClosestVertex(Vec3& out, const Vec3& v) const
{
	float minDist = std::numeric_limits<float>::max();
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
	{
		float dist = (v - m_p[i].m_pos).Length();
		if (dist < minDist)
		{
			minDist = dist;
			out = m_p[i].m_pos;
		}
	}
	return minDist;
}

bool Quadblock::Neighbours(const Quadblock& quadblock, float threshold) const
{
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
	{
		for (size_t j = 0; j < NUM_VERTICES_QUADBLOCK; j++)
		{
			if ((m_p[i].m_pos - quadblock.m_p[j].m_pos).Length() < threshold) { return true; }
		}
	}
	return false;
}

std::vector<uint8_t> Quadblock::Serialize(size_t id, size_t offTextures, size_t offVisibleSet, const std::vector<size_t>& vertexIndexes) const
{
	PSX::Quadblock quadblock = {};
	std::vector<uint8_t> buffer(sizeof(quadblock));
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
	{
		quadblock.index[i] = static_cast<uint16_t>(vertexIndexes[i]);
	}
	quadblock.flags = m_flags;
	quadblock.drawOrderLow = m_doubleSided ? (1 << 31) : 0;
	for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
	{
		uint32_t packedFace = m_faceRotateFlip[i] | (m_faceDrawMode[i] << 3);
		quadblock.drawOrderLow |= packedFace << (8 + i * 5);
	}
	quadblock.drawOrderHigh = 0;
	quadblock.offMidTextures[0] = static_cast<uint32_t>(offTextures);
	quadblock.offMidTextures[1] = static_cast<uint32_t>(offTextures);
	quadblock.offMidTextures[2] = static_cast<uint32_t>(offTextures);
	quadblock.offMidTextures[3] = static_cast<uint32_t>(offTextures);
	quadblock.bbox.min = ConvertVec3(m_bbox.min, FP_ONE_GEO);
	quadblock.bbox.max = ConvertVec3(m_bbox.max, FP_ONE_GEO);
	quadblock.terrain = m_terrain;
	quadblock.weatherIntensity = 0;
	quadblock.weatherVanishRate = 0;
	quadblock.speedImpact = 0;
	quadblock.id = static_cast<uint16_t>((id & 0xffe0) + 0x1f - (id & 0x1f));
	quadblock.checkpointIndex = static_cast<uint8_t>(m_checkpointIndex);
	if (m_triblock)
	{
		quadblock.triNormalVecBitshift = static_cast<uint8_t>(std::round(std::log2(ComputeNormalVector(0, 2, 6).Length()) * 512.0f));
	}
	else
	{
		quadblock.triNormalVecBitshift = static_cast<uint8_t>(std::round(std::log2(std::max(ComputeNormalVector(0, 2, 6).Length(), ComputeNormalVector(2, 8, 6).Length()) * 512.0f)));
	}
	quadblock.offLowTexture = static_cast<uint32_t>(offTextures);
	quadblock.offVisibleSet = static_cast<uint32_t>(offVisibleSet);

	auto CalculateNormalDividend = [this](size_t id0, size_t id1, size_t id2, float scaler) -> uint16_t
		{
			return static_cast<uint16_t>(std::round(scaler / ComputeNormalVector(id0, id1, id2).Length()));
		};

	float scaler = static_cast<float>(1 << quadblock.triNormalVecBitshift);
	quadblock.triNormalVecDividend[0] = CalculateNormalDividend(0, 1, 3, scaler);
	quadblock.triNormalVecDividend[1] = CalculateNormalDividend(1, 4, 3, scaler);
	quadblock.triNormalVecDividend[2] = CalculateNormalDividend(4, 1, 2, scaler);
	quadblock.triNormalVecDividend[3] = CalculateNormalDividend(3, 4, 6, scaler);
	if (!m_triblock)
	{
		quadblock.triNormalVecDividend[4] = CalculateNormalDividend(7, 4, 5, scaler);
		quadblock.triNormalVecDividend[5] = CalculateNormalDividend(5, 8, 7, scaler);
		quadblock.triNormalVecDividend[6] = CalculateNormalDividend(2, 5, 4, scaler);
		quadblock.triNormalVecDividend[7] = CalculateNormalDividend(6, 4, 7, scaler);
		quadblock.triNormalVecDividend[9] = CalculateNormalDividend(2, 8, 6, scaler); /* low LoD */
	}
	quadblock.triNormalVecDividend[8] = CalculateNormalDividend(0, 2, 6, scaler); /* low LoD */
	std::memcpy(buffer.data(), &quadblock, sizeof(quadblock));
	return buffer;
}

Vec3 Quadblock::ComputeNormalVector(size_t id0, size_t id1, size_t id2) const
{
	Vec3 a = m_p[id0].m_pos - m_p[id1].m_pos;
	Vec3 b = m_p[id2].m_pos - m_p[id0].m_pos;
	return a.Cross(b);
}

void Quadblock::ComputeBoundingBox()
{
	Vec3 min = Vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	Vec3 max = Vec3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
	{
		min.x = std::min(min.x, m_p[i].m_pos.x); max.x = std::max(max.x, m_p[i].m_pos.x);
		min.y = std::min(min.y, m_p[i].m_pos.y); max.y = std::max(max.y, m_p[i].m_pos.y);
		min.z = std::min(min.z, m_p[i].m_pos.z); max.z = std::max(max.z, m_p[i].m_pos.z);
	}
	m_bbox.min = min;
	m_bbox.max = max;
}
