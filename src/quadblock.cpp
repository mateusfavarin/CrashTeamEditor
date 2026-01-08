#include "quadblock.h"
#include "utils.h"
#include "gui_render_settings.h"

#include <unordered_map>
#include <unordered_set>
#include <cstring>

Quadblock::Quadblock(const std::string& name, Tri& t0, Tri& t1, Tri& t2, Tri& t3, const Vec3& normal, const std::string& material, bool hasUV, UpdateFilterCallback filterCallback)
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
		throw QuadException(
			("Unique Vertices: " + std::to_string(uniqueCount) + "/3\n" +
			 "Shared Vertices: " + std::to_string(sharedCount) + "/3\n")
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

	auto MakeVertex = [](const Point* p) -> Vertex
		{
			if (!p) { throw QuadException("Tris contain overlapping points"); }
			return Vertex(*p);
		};

	if (hasUV)
	{
		auto UvMatches = [](const Vec2& a, const Vec2& b) -> bool
			{
				constexpr float UV_EPS_SQ = 1.0e-10f;
				const float dx = a.x - b.x;
				const float dy = a.y - b.y;
				return (dx * dx + dy * dy) <= UV_EPS_SQ;
			};

		auto GetCenterUV = [&](const Vec3& pos, Vec2& out) -> bool
			{
				for (size_t i = 0; i < 3; i++)
				{
					if (centerTri->p[i].pos == pos)
					{
						out = centerTri->p[i].uv;
						return true;
					}
				}
				return false;
			};

		size_t bestIndex = 0;
		int bestMatches = -1;
		for (size_t triIndex = 0; triIndex < adjTris.size(); triIndex++)
		{
			const Tri& tri = *adjTris[triIndex];
			int matches = 0;
			for (size_t i = 0; i < 3; i++)
			{
				const Vec3& pos = tri.p[i].pos;
				if (vRefCount[pos] != 3) { continue; }
				Vec2 centerUv;
				if (GetCenterUV(pos, centerUv) && UvMatches(centerUv, tri.p[i].uv)) { matches++; }
			}

			if (matches > bestMatches)
			{
				bestMatches = matches;
				bestIndex = triIndex;
			}
		}

		if (bestMatches != 2) { throw QuadException("At least 2 triangles in the triblock must share 2 UV vertices."); }

		if (bestIndex != 0) { Swap(adjTris[0], adjTris[bestIndex]); }
	}

	try
	{
		std::vector<const Point*> q0Adjs;
		m_p[0] = MakeVertex(FindUniquePoint(*adjTris[0], q0Adjs));
		m_p[1] = MakeVertex(q0Adjs[0]);
		m_p[3] = MakeVertex(q0Adjs[1]);
	}
	catch (const QuadException& e) { throw e; }

	bool q1Next = false;
	for (size_t i = 0; i < 3; i++)
	{
		if (adjTris[1]->p[i].pos == m_p[1].m_pos) { q1Next = true; }
	}
	if (!q1Next) { Swap(adjTris[1], adjTris[2]); }

	try
	{
		std::vector<const Point*> q2Adjs;
		m_p[2] = MakeVertex(FindUniquePoint(*adjTris[1], q2Adjs));
		m_p[4] = q2Adjs[0]->pos == m_p[1].m_pos ? MakeVertex(q2Adjs[1]) : MakeVertex(q2Adjs[0]);
	}
	catch (const QuadException& e) { throw e; }

	try
	{
		std::vector<const Point*> q4Adjs;
		m_p[6] = MakeVertex(FindUniquePoint(*adjTris[2], q4Adjs));
	}
	catch (const QuadException& e) { throw e; }

	Vec3 quadNormal = ComputeNormalVector(0, 2, 6);
	quadNormal = quadNormal / quadNormal.Length();
	Vec3 invertedQuadNormal = quadNormal * -1;
	if ((normal - invertedQuadNormal).Length() < (normal - quadNormal).Length())
	{
		Swap(m_p[1], m_p[3]);
		Swap(m_p[2], m_p[6]);
	}

	m_p[5] = m_p[4];
	m_p[7] = m_p[6];
	m_p[8] = m_p[6];

	auto FindTri = [](const Vec3& pos, const Tri& t0, const Tri& t1, const Tri& t2, const Tri& t3) -> const Tri*
		{
			const Tri* tris[] = {&t0, &t1, &t2, &t3};
			for (size_t i = 0; i < 4; i++)
			{
				const Tri* tri = tris[i];
				for (size_t j = 0; j < 3; j++)
				{
					if (tri->p[j].pos == pos) { return tri; }
				}
			}
			return nullptr;
		};

	auto GetUV = [](const Vec3& pos, const Tri& tri) -> Vec2
		{
			for (size_t i = 0; i < 3; i++)
			{
				if (tri.p[i].pos == pos) { return tri.p[i].uv; }
			}
			return Vec2();
		};

	if (hasUV)
	{
		const Tri* uvt0 = FindTri(m_p[0].m_pos, t0, t1, t2, t3);
		const Tri* uvt1 = FindTri(m_p[2].m_pos, t0, t1, t2, t3);
		const Tri* uvt2 = FindTri(m_p[6].m_pos, t0, t1, t2, t3);

		m_uvs[0] = { GetUV(m_p[0].m_pos, *uvt0), GetUV(m_p[1].m_pos, *uvt0), GetUV(m_p[3].m_pos, *uvt0), GetUV(m_p[4].m_pos, *centerTri) };
		m_uvs[1] = { GetUV(m_p[1].m_pos, *uvt1), GetUV(m_p[2].m_pos, *uvt1), GetUV(m_p[4].m_pos, *uvt1), Vec2() };
		m_uvs[2] = { GetUV(m_p[3].m_pos, *uvt2), GetUV(m_p[4].m_pos, *uvt2), GetUV(m_p[6].m_pos, *uvt2), Vec2() };
		m_uvs[3] = { Vec2(), Vec2(), Vec2(), Vec2() };
		m_uvs[4] = { GetUV(m_p[0].m_pos, *uvt0), GetUV(m_p[2].m_pos, *uvt1), GetUV(m_p[6].m_pos, *uvt2), Vec2() };
	}
	else { ResetUVs(); }

	m_name = name;
	m_material = material;
	m_triblock = true;
	m_filterCallback = filterCallback;
	SetDefaultValues();
}

Quadblock::Quadblock(const std::string& name, Quad& q0, Quad& q1, Quad& q2, Quad& q3, const Vec3& normal, const std::string& material, bool hasUV, UpdateFilterCallback filterCallback)
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
		throw QuadException(
			("Unique Vertices: " + std::to_string(uniqueCount) + "/4\n" +
			"Shared Vertices: " + std::to_string(sharedCount) + "/4\n" +
			"Center Vertices: " + std::to_string(centerCount) + "/1\n")
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

	auto MakeVertex = [](const Point* p) -> Vertex
		{
			if (!p) { throw QuadException("Quads contain overlapping points"); }
			return Vertex(*p);
		};

	try
	{
		m_p[1] = p1 ? MakeVertex(p1) : MakeVertex(FindAdjacentPoint(q0, q1, centerVertex));
		m_p[3] = p3 ? MakeVertex(p3) : MakeVertex(FindAdjacentPoint(q0, q2, centerVertex));
		m_p[5] = MakeVertex(FindAdjacentPoint(q1, q3, centerVertex));
		m_p[7] = MakeVertex(FindAdjacentPoint(q2, q3, centerVertex));
	}
	catch (const QuadException& e) { throw e; }

	Vec3 quadNormal = ComputeNormalVector(0, 2, 6);
	quadNormal = quadNormal / quadNormal.Length();
	Vec3 invertedQuadNormal = quadNormal * -1;
	if ((normal - invertedQuadNormal).Length() < (normal - quadNormal).Length())
	{
		Swap(m_p[0], m_p[2]);
		Swap(m_p[3], m_p[5]);
		Swap(m_p[6], m_p[8]);
	}

	auto FindQuad = [](const Vec3& pos, const Quad& q0, const Quad& q1, const Quad& q2, const Quad& q3) -> const Quad*
		{
			const Quad* quads[] = { &q0, &q1, &q2, &q3 };
			for (size_t i = 0; i < 4; i++)
			{
				const Quad* quad = quads[i];
				for (size_t j = 0; j < 4; j++)
				{
					if (quad->p[j].pos == pos) { return quad; }
				}
			}
			return nullptr;
		};

	auto GetUV = [](const Vec3& pos, const Quad& quad) -> Vec2
		{
			for (size_t i = 0; i < 4; i++)
			{
				if (quad.p[i].pos == pos) { return quad.p[i].uv; }
			}
			return Vec2();
		};

	const Quad* uvq0 = FindQuad(m_p[0].m_pos, q0, q1, q2, q3);
	const Quad* uvq1 = FindQuad(m_p[2].m_pos, q0, q1, q2, q3);
	const Quad* uvq2 = FindQuad(m_p[6].m_pos, q0, q1, q2, q3);
	const Quad* uvq3 = FindQuad(m_p[8].m_pos, q0, q1, q2, q3);

	if (hasUV)
	{
		m_uvs[0] = { GetUV(m_p[0].m_pos, *uvq0), GetUV(m_p[1].m_pos, *uvq0), GetUV(m_p[3].m_pos, *uvq0), GetUV(m_p[4].m_pos, *uvq0) };
		m_uvs[1] = { GetUV(m_p[1].m_pos, *uvq1), GetUV(m_p[2].m_pos, *uvq1), GetUV(m_p[4].m_pos, *uvq1), GetUV(m_p[5].m_pos, *uvq1) };
		m_uvs[2] = { GetUV(m_p[3].m_pos, *uvq2), GetUV(m_p[4].m_pos, *uvq2), GetUV(m_p[6].m_pos, *uvq2), GetUV(m_p[7].m_pos, *uvq2) };
		m_uvs[3] = { GetUV(m_p[4].m_pos, *uvq3), GetUV(m_p[5].m_pos, *uvq3), GetUV(m_p[7].m_pos, *uvq3), GetUV(m_p[8].m_pos, *uvq3) };

		float uMin = std::numeric_limits<float>::max(); float vMin = std::numeric_limits<float>::max();
		float uMax = -std::numeric_limits<float>::max(); float vMax = -std::numeric_limits<float>::max();
		for (size_t i = 0; i < 4; i++)
		{
			for (size_t j = 0; j < 4; j++)
			{
				uMin = std::min(uMin, m_uvs[i][j].x); vMin = std::min(vMin, m_uvs[i][j].y);
				uMax = std::max(uMax, m_uvs[i][j].x);	vMax = std::max(vMax, m_uvs[i][j].y);
			}
		}

		bool indexPicked[4] = {false, false, false, false};
		bool boundPicked[4] = {false, false, false, false};
		const QuadUV uvBounds = { Vec2(uMin, vMin), Vec2(uMax, vMin), Vec2(uMin, vMax), Vec2(uMax, vMax)};

		for (size_t indexCount = 0; indexCount < 4; indexCount++)
		{
			size_t bestIndex = 0;
			size_t bestBound = 0;
			float bestDistance = std::numeric_limits<float>::max();
			for (size_t i = 0; i < 4; i++)
			{
				if (indexPicked[i]) { continue; }
				for (size_t j = 0; j < 4; j++)
				{
					if (boundPicked[j]) { continue; }
					float dist = ((m_uvs[0][i].x - uvBounds[j].x) * (m_uvs[0][i].x - uvBounds[j].x)) + ((m_uvs[0][i].y - uvBounds[j].y) * (m_uvs[0][i].y - uvBounds[j].y));
					if (dist < bestDistance)
					{
						bestIndex = i;
						bestBound = j;
						bestDistance = dist;
					}
				}
			}
			indexPicked[bestIndex] = true;
			boundPicked[bestBound] = true;
			m_uvs[4][bestIndex] = uvBounds[bestBound];
		}
	}
	else { ResetUVs(); }

	m_name = name;
	m_material = material;
	m_triblock = false;
	m_filterCallback = filterCallback;
	SetDefaultValues();
}

Quadblock::Quadblock(const PSX::Quadblock& quadblock, const std::vector<PSX::Vertex>& vertices, UpdateFilterCallback filterCallback)
{
	uint16_t reverseIndexMapping[NUM_VERTICES_QUADBLOCK] = { 0, 2, 6, 8, 1, 3, 4, 5, 7 };
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
	{
		uint16_t index = quadblock.index[i];
		const PSX::Vertex& vertex = vertices[index];
		m_p[reverseIndexMapping[i]] = Vertex(vertex);
	}
	SetDefaultValues();

	m_name = "Quadblock " + std::to_string(quadblock.id);
	m_flags = quadblock.flags;
	m_doubleSided = (quadblock.drawOrderLow & (1 << 31)) != 0;
	for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
	{
		uint32_t packedFace = (quadblock.drawOrderLow >> (8 + i * 5)) & 0b11111;
		m_faceRotateFlip[i] = packedFace & 0b111;
		m_faceDrawMode[i] = (packedFace >> 3) & 0b11;
	}
	m_terrain = quadblock.terrain;
	m_checkpointIndex = quadblock.checkpointIndex;
	if (m_checkpointIndex == std::numeric_limits<uint8_t>::max()) { m_checkpointIndex = -1; }
	else { m_checkpointStatus = true; }
	m_material = "default";
	m_triblock = false;
	m_filterCallback = filterCallback;
}

const std::string& Quadblock::GetName() const
{
	return m_name;
}

bool Quadblock::IsQuadblock() const
{
	return !m_triblock;
}

const Vec3& Quadblock::GetCenter() const
{
	return m_p[4].m_pos;
}

Vec3 Quadblock::GetNormal() const
{
	Vec3 normal = ComputeNormalVector(0, 2, 6);
	normal.Normalize();
	return normal;
}

uint8_t Quadblock::GetTerrain() const
{
	return m_terrain;
}

uint16_t Quadblock::GetFlags() const
{
	return m_flags;
}

QuadblockTrigger Quadblock::GetTrigger() const
{
	return m_trigger;
}

size_t Quadblock::GetTurboPadIndex() const
{
	return m_turboPadIndex;
}

size_t Quadblock::GetBSPID() const
{
	return m_bspID;
}

void Quadblock::SetBSPID(size_t id) const
{
	m_bspID = id;
}

bool Quadblock::GetHide() const
{
	return m_hide;
}

bool Quadblock::GetAnimated() const
{
	return m_animated;
}

bool Quadblock::GetFilter() const
{
	return m_filter;
}

const Color& Quadblock::GetFilterColor() const
{
	return m_filterColor;
}

bool Quadblock::GetCheckpointStatus() const
{
	return m_checkpointStatus;
}

bool Quadblock::GetCheckpointPathable() const
{
	return m_checkpointPathable;
}

const QuadUV& Quadblock::GetQuadUV(size_t quad) const
{
	return m_uvs[quad];
}

const std::filesystem::path& Quadblock::GetTexPath() const
{
	return m_texPath;
}

const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>& Quadblock::GetUVs() const
{
	return m_uvs;
}

size_t Quadblock::GetRenderHighLodPointIndex() const
{
	return m_renderHighLodPointIndex;
}

size_t Quadblock::GetRenderLowLodPointIndex() const
{
	return m_renderLowLodPointIndex;
}

size_t Quadblock::GetRenderHighLodUVIndex() const
{
	return m_renderHighLodUVIndex;
}

size_t Quadblock::GetRenderLowLodUVIndex() const
{
	return m_renderLowLodUVIndex;
}

size_t Quadblock::GetRenderHighLodOctoPointIndex() const
{
	return m_renderHighLodOctoPointIndex;
}

size_t Quadblock::GetRenderLowLodOctoPointIndex() const
{
	return m_renderLowLodOctoPointIndex;
}

size_t Quadblock::GetRenderFilterHighLodEdgeIndex() const
{
	return m_renderFilterHighLodEdgeIndex;
}

size_t Quadblock::GetRenderFilterLowLodEdgeIndex() const
{
	return m_renderFilterLowLodEdgeIndex;
}

void Quadblock::SetRenderIndices(size_t highPointIndex, size_t lowPointIndex, size_t highUvIndex, size_t lowUvIndex, size_t highOctoIndex, size_t lowOctoIndex)
{
	m_renderHighLodPointIndex = highPointIndex;
	m_renderLowLodPointIndex = lowPointIndex;
	m_renderHighLodUVIndex = highUvIndex;
	m_renderLowLodUVIndex = lowUvIndex;
	m_renderHighLodOctoPointIndex = highOctoIndex;
	m_renderLowLodOctoPointIndex = lowOctoIndex;
}

void Quadblock::SetRenderFilterIndices(size_t highEdgeIndex, size_t lowEdgeIndex)
{
	m_renderFilterHighLodEdgeIndex = highEdgeIndex;
	m_renderFilterLowLodEdgeIndex = lowEdgeIndex;
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

int Quadblock::GetCheckpoint() const
{
	return m_checkpointIndex;
}

void Quadblock::SetDrawDoubleSided(bool active)
{
	m_doubleSided = active;
}

void Quadblock::SetCheckpointStatus(bool active)
{
	m_checkpointStatus = active;
}

void Quadblock::SetCheckpointPathable(bool pathable)
{
	m_checkpointPathable = pathable;
}

void Quadblock::SetName(const std::string& name)
{
	m_name = name;
}

void Quadblock::SetTurboPadIndex(size_t index)
{
	m_turboPadIndex = index;
}

void Quadblock::SetHide(bool active)
{
	m_hide = active;
}

void Quadblock::SetTextureID(size_t id, size_t quad)
{
	m_textureIDs[quad] = id;
}

void Quadblock::SetAnimTextureOffset(size_t relOffset, size_t levOffset, size_t quad)
{
	m_animTexOffset[quad] = relOffset + levOffset;
}

void Quadblock::SetTrigger(QuadblockTrigger trigger)
{
	m_trigger = trigger;
}

void Quadblock::SetTexPath(const std::filesystem::path& path)
{
	m_texPath = path;
}

void Quadblock::SetAnimated(bool animated)
{
	m_animated = animated;
}

void Quadblock::SetFilter(bool filter)
{
	m_filter = filter;
	m_filterCallback(*this);
}

void Quadblock::SetFilterColor(const Color& color)
{
	m_filterColor = color;
	m_filterCallback(*this);
}

void Quadblock::SetSpeedImpact(int speed)
{
	m_downforce = speed;
}

void Quadblock::Translate(float ratio, const Vec3& direction)
{
	for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++) { m_p[i].m_pos += direction * ratio; }
	ComputeBoundingBox();
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

const Vertex* const Quadblock::GetUnswizzledVertices() const
{
	return m_p;
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

std::vector<uint8_t> Quadblock::Serialize(size_t id, size_t offTextures, const std::vector<size_t>& vertexIndexes) const
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
	if (m_animated)
	{
		quadblock.offMidTextures[0] = static_cast<uint32_t>(m_animTexOffset[0] | 1);
		quadblock.offMidTextures[1] = static_cast<uint32_t>(m_animTexOffset[1] | 1);
		quadblock.offMidTextures[2] = static_cast<uint32_t>(m_animTexOffset[2] | 1);
		quadblock.offMidTextures[3] = static_cast<uint32_t>(m_animTexOffset[3] | 1);
		quadblock.offLowTexture = static_cast<uint32_t>(offTextures + (m_textureIDs[4] * sizeof(PSX::TextureGroup)));
	}
	else
	{
		quadblock.offMidTextures[0] = static_cast<uint32_t>(offTextures + (m_textureIDs[0] * sizeof(PSX::TextureGroup)));
		quadblock.offMidTextures[1] = static_cast<uint32_t>(offTextures + (m_textureIDs[1] * sizeof(PSX::TextureGroup)));
		quadblock.offMidTextures[2] = static_cast<uint32_t>(offTextures + (m_textureIDs[2] * sizeof(PSX::TextureGroup)));
		quadblock.offMidTextures[3] = static_cast<uint32_t>(offTextures + (m_textureIDs[3] * sizeof(PSX::TextureGroup)));
		quadblock.offLowTexture = static_cast<uint32_t>(offTextures + (m_textureIDs[4] * sizeof(PSX::TextureGroup)));
	}
	quadblock.bbox.min = ConvertVec3(m_bbox.min, FP_ONE_GEO);
	quadblock.bbox.max = ConvertVec3(m_bbox.max, FP_ONE_GEO);
	quadblock.terrain = m_terrain;
	quadblock.weatherIntensity = 0;
	quadblock.weatherVanishRate = 0;
	quadblock.speedImpact = static_cast<int8_t>(m_downforce);
	const size_t idVis = id / 32;
	quadblock.id = static_cast<uint16_t>((32 * idVis) + (31 - (id % 32)));
	quadblock.checkpointIndex = static_cast<uint8_t>(m_checkpointIndex);
	quadblock.triNormalVecBitshift = static_cast<uint8_t>(std::round(std::log2(std::max(ComputeNormalVector(0, 2, 6).Length(), ComputeNormalVector(2, 8, 6).Length()) * 512.0f)));

	auto CalculateNormalDividend = [this](size_t id0, size_t id1, size_t id2, float scaler) -> int16_t
		{
			return static_cast<int16_t>(std::round(scaler / ComputeNormalVector(id0, id1, id2).Length()));
		};

	float scaler = static_cast<float>(1 << quadblock.triNormalVecBitshift);
	quadblock.triNormalVecDividend[0] = CalculateNormalDividend(0, 1, 3, scaler);
	quadblock.triNormalVecDividend[1] = CalculateNormalDividend(1, 4, 3, scaler);
	quadblock.triNormalVecDividend[2] = CalculateNormalDividend(4, 1, 2, scaler);
	quadblock.triNormalVecDividend[3] = CalculateNormalDividend(3, 4, 6, scaler);
	quadblock.triNormalVecDividend[4] = CalculateNormalDividend(7, 4, 5, scaler);
	quadblock.triNormalVecDividend[5] = CalculateNormalDividend(5, 8, 7, scaler);
	quadblock.triNormalVecDividend[6] = CalculateNormalDividend(2, 5, 4, scaler);
	quadblock.triNormalVecDividend[7] = CalculateNormalDividend(6, 4, 7, scaler);
	quadblock.triNormalVecDividend[9] = CalculateNormalDividend(2, 8, 6, scaler); /* low LoD */
	quadblock.triNormalVecDividend[8] = CalculateNormalDividend(0, 2, 6, scaler); /* low LoD */
	std::memcpy(buffer.data(), &quadblock, sizeof(quadblock));
	return buffer;
}

void Quadblock::SetDefaultValues()
{
	ComputeBoundingBox();
	m_checkpointIndex = -1;
	m_flags = QuadFlags::DEFAULT;
	m_terrain = TerrainType::LABELS.at(TerrainType::DEFAULT);

	for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
	{
		m_faceDrawMode[i] = FaceDrawMode::DRAW_BOTH;
		m_faceRotateFlip[i] = FaceRotateFlip::NONE;
	}
	m_doubleSided = false;
	m_checkpointPathable = true;
	m_checkpointStatus = false;
	m_trigger = QuadblockTrigger::NONE;
	m_turboPadIndex = TURBO_PAD_INDEX_NONE;
	m_hide = false;
	m_animated = false;
	m_filter = false;
	m_downforce = 0;
	m_filterColor = GuiRenderSettings::defaultFilterColor;
	m_renderHighLodPointIndex = RENDER_INDEX_NONE;
	m_renderLowLodPointIndex = RENDER_INDEX_NONE;
	m_renderHighLodUVIndex = RENDER_INDEX_NONE;
	m_renderLowLodUVIndex = RENDER_INDEX_NONE;
	m_renderHighLodOctoPointIndex = RENDER_INDEX_NONE;
	m_renderLowLodOctoPointIndex = RENDER_INDEX_NONE;
	m_renderFilterHighLodEdgeIndex = RENDER_INDEX_NONE;
	m_renderFilterLowLodEdgeIndex = RENDER_INDEX_NONE;
}

Vec3 Quadblock::ComputeNormalVector(size_t id0, size_t id1, size_t id2) const
{
	Vec3 a = m_p[id0].m_pos - m_p[id1].m_pos;
	Vec3 b = m_p[id2].m_pos - m_p[id0].m_pos;
	return a.Cross(b);
}

void Quadblock::ResetUVs()
{
	m_uvs[0] = {Vec2(0.0f, 0.0f), Vec2(0.5f, 0.0f), Vec2(0.0f, 0.5f), Vec2(0.5f, 0.5f)};
	m_uvs[1] = {Vec2(0.5f, 0.0f), Vec2(1.0f, 0.0f), Vec2(0.5f, 0.5f), Vec2(1.0f, 0.5f)};
	m_uvs[2] = {Vec2(0.0f, 0.5f), Vec2(0.5f, 0.5f), Vec2(0.0f, 1.0f), Vec2(0.5f, 1.0f)};
	m_uvs[3] = {Vec2(0.5f, 0.5f), Vec2(1.0f, 0.5f), Vec2(0.5f, 1.0f), Vec2(1.0f, 1.0f)};
	m_uvs[4] = {Vec2(0.0f, 0.0f), Vec2(1.0f, 0.0f), Vec2(0.0f, 1.0f), Vec2(1.0f, 1.0f)};
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
