#include "viztree.h"
#define _USE_MATH_DEFINES
#include "math.h"

#include <random>

char& BitMatrix::get(size_t x, size_t y)
{
	return data[(y * width) + x];
}

bool BitMatrix::read(size_t x, size_t y) const
{
	return static_cast<bool>(data[(y * width) + x]);
}

BitMatrix BitMatrix::transposed() const
{
	BitMatrix result = BitMatrix(this->height, this->width);

	for (int x = 0; x < this->width; x++)
	{
		for (int y = 0; y < this->height; y++)
		{
			result.get(y, x) = this->read(x, y);
		}
	}

	return result;
}

BitMatrix BitMatrix::operator|(const BitMatrix& other) const
{
	if (other.width != this->width || other.height != this->height)
		throw;

	BitMatrix result = BitMatrix(this->width, this->height);
	for (int x = 0; x < this->width; x++)
	{
		for (int y = 0; y < this->height; y++)
		{
			result.get(x, y) = this->read(x, y) | other.read(x, y);
		}
	}

	return result;
}

size_t BitMatrix::getWidth() const
{
	return width;
}

size_t BitMatrix::getHeight() const
{
	return height;
}

static float randomInRange(float low, float high)
{
	/*static */
	std::uniform_real_distribution<float> unif(low, high); //this initializes it to the distribution when it is first called, this doesn't work
	static std::default_random_engine re;

	return unif(re);
}

static Vec3 sampleInRectangularPrism(Vec3 p1, Vec3 p2)
{
	return Vec3(randomInRange(p1.x, p2.x), randomInRange(p1.y, p2.y), randomInRange(p1.z, p2.z));
}

static Vec3 sampleOnUnitSphere()
{
	float u = randomInRange(0.0, 1.0);
	float v = randomInRange(0.0, 1.0);

	float theta = 2.0 * M_PI * u;      // azimuthal angle
	float phi = std::acos(2.0 * v - 1.0);  // polar angle

	float x = std::sin(phi) * std::cos(theta);
	float y = std::sin(phi) * std::sin(theta);
	float z = std::cos(phi);

	return Vec3(x, y, z);
}

static Vec3 sampleOnUnitHemisphere()
{
	float u = randomInRange(0.0, 1.0);
	float v = randomInRange(0.0, 1.0);

	float theta = 2.0 * M_PI * u;      // azimuthal angle
	float phi = std::acos(2.0 * v - 1.0);  // polar angle

	float x = std::sin(phi) * std::cos(theta);
	float y = std::sin(phi) * std::sin(theta);
	float z = std::cos(phi);

	return Vec3(std::abs(x), y, z);
}

static std::tuple<Vec3, float> worldspaceRayTriIntersection(Vec3 worldSpaceRayOrigin, Vec3 worldSpaceRayDir, const Vec3 tri[3])
{
	constexpr float epsilon = 0.00001;

	//moller-trumbore intersection test
	//https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

	Vec3 edge_1 = tri[1] - tri[0], edge_2 = tri[2] - tri[0];
	Vec3 ray_cross_e2 = worldSpaceRayDir.Cross(edge_2);
	float det = edge_1.Dot(ray_cross_e2);

	if (std::abs(det) < epsilon)
		return std::make_tuple(Vec3::Zero(), 0.0); //ray is parallel to plane, couldn't possibly intersect with triangle.

	float inv_det = 1.0f / det;
	Vec3 s = worldSpaceRayOrigin - tri[0];
	float u = inv_det * s.Dot(ray_cross_e2);

	if ((u < 0 && std::abs(u) > epsilon) || (u > 1 && std::abs(u - 1.0f) > epsilon))
		return std::make_tuple(Vec3::Zero(), 0.0); //fails a barycentric test

	Vec3 s_cross_e1 = s.Cross(edge_1);
	float v = inv_det * worldSpaceRayDir.Dot(s_cross_e1);

	if ((v < 0 && std::abs(v) > epsilon) || (u + v > 1 && std::abs(u + v - 1) > epsilon))
		return std::make_tuple(Vec3::Zero(), 0.0); //fails a barycentric test

	float t = inv_det * edge_2.Dot(s_cross_e1); //time value (interpolant)

	if (t > epsilon)
	{
		Vec3 collisionPoint = worldSpaceRayOrigin + (worldSpaceRayDir * t); //this is the point *on* the triangle that was collided with.
		return std::make_tuple(collisionPoint, t);
	}
	else
	{
		//line intersects, but ray does not (i.e., behind camera)
		Vec3 collisionPoint = worldSpaceRayOrigin + (worldSpaceRayDir * t); //this is the point *on* the triangle that was collided with.
		return std::make_tuple(collisionPoint, t);
	}
}

static std::tuple<Vec3, float> rayIntersectQuadblockTest(Vec3 worldSpaceRayOrigin, Vec3 worldSpaceRayDir, const Quadblock& qb)
{
	bool isQuadblock = qb.IsQuadblock();
	const Vertex* verts = qb.GetUnswizzledVertices();

	Vec3 tris[3];
	bool collided = false;

	for (int triIndex = 0; triIndex < (isQuadblock ? 8 : 4); triIndex++)
	{
		if (isQuadblock)
		{
			tris[0] = Vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.z);
			tris[1] = Vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.z);
			tris[2] = Vec3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.z);
		}
		else
		{
			tris[0] = Vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.z);
			tris[1] = Vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.z);
			tris[2] = Vec3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.z);
		}

		std::tuple<Vec3, float> queryResult = worldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, tris);

		collided |= (std::get<1>(queryResult) != 0.0);

		if (collided) { return queryResult; }
	}

	for (int triIndex = 0; triIndex < (isQuadblock ? 2 : 1); triIndex++)
	{
		if (isQuadblock)
		{
			tris[0] = Vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.z);
			tris[1] = Vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.z);
			tris[2] = Vec3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.z);
		}
		else
		{
			tris[0] = Vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.z);
			tris[1] = Vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.z);
			tris[2] = Vec3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.z);
		}

		std::tuple<Vec3, float> queryResult = worldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, tris);
		collided |= (std::get<1>(queryResult) != 0.0);

		if (collided) { return queryResult; }
	}

	return std::make_tuple(Vec3::Zero(), 0.0);
}

BitMatrix viztree_method_1(const std::vector<Quadblock>& quadblocks, const std::vector<const BSP*>& leaves) {
	/* tldr: for each of the 9 points of a quadblock, randomly sample a ray and test collision against all other quadblocks

	results: 2dMatrix

	for each quadblock: q1
		for each point in q1: p
			for each ray emitted from random unit sphere sample * 100 or something: r
				for each quadblock: q2
					if (p + r intersects with high or low LOD of q2)
						results.add(q2, q1) //q2 can be seen by q1
	*/

	BitMatrix vizMatrix = BitMatrix(leaves.size(), leaves.size());
	std::unordered_map<size_t, size_t> quadIndexesToLeaves;
	for (size_t i = 0; i < leaves.size(); i++)
	{
		std::vector<size_t> quadIndexes = leaves[i]->GetQuadblockIndexes();
		for (size_t index : quadIndexes) { quadIndexesToLeaves[index] = i; }
	}

	for (size_t leafA = 0; leafA < leaves.size(); leafA++)
	{
		//printf("Leaf: %d/%d", leafA, leaves.size());
		vizMatrix.get(leafA, leafA) = true;
		for (size_t leafB = 0; leafB < leaves.size(); leafB++)
		{
			if (vizMatrix.get(leafA, leafB)) { continue; }

			constexpr size_t POINTS_BBOX = 8;
			BoundingBox bboxA = leaves[leafA]->GetBoundingBox();
			BoundingBox bboxB = leaves[leafB]->GetBoundingBox();

			auto GenTestPoints = [](const BoundingBox& bbox, std::array<Vec3, POINTS_BBOX>& points)
				{
					points[0] = bbox.min;
					points[1] = bbox.max;
					points[2] = {bbox.min.x, bbox.min.y, bbox.max.z};
					points[3] = {bbox.min.x, bbox.max.y, bbox.min.z};
					points[4] = {bbox.max.x, bbox.min.y, bbox.min.z};
					points[5] = {bbox.min.x, bbox.max.y, bbox.max.z};
					points[6] = {bbox.max.x, bbox.min.y, bbox.max.z};
					points[7] = {bbox.max.x, bbox.max.y, bbox.min.z};
				};

			std::array<Vec3, 8> ptsA;
			std::array<Vec3, 8> ptsB;
			GenTestPoints(bboxA, ptsA);
			GenTestPoints(bboxB, ptsB);
			bool worthTesting = false;
			for (size_t i = 0; i < POINTS_BBOX; i++)
			{
				for (size_t j = 0; j < POINTS_BBOX; j++)
				{
					if ((ptsA[i] - ptsB[j]).Length() < MAX_DRAW_DISTANCE)
					{
						worthTesting = true;
						break;
					}
				}
				if (worthTesting) { break; }
			}

			if (!worthTesting) { continue; }

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
					const Vec3& directionPos = directionQuad.GetUnswizzledVertices()[4].m_pos;
					Vec3 directionVector = directionPos - center;
					float testDist = directionVector.Length();
					if (testDist > MAX_DRAW_DISTANCE) { continue; }

					directionVector /= testDist;

					size_t leafHit = leafA;
					float bestDistPos = std::numeric_limits<float>::max();
					for (size_t testQuadIndex = 0; testQuadIndex < quadblocks.size(); testQuadIndex++)
					{
						const Quadblock& testQuad = quadblocks[testQuadIndex];
						Vec3 testCenter = testQuad.GetUnswizzledVertices()[4].m_pos;
						if ((testCenter - center).Length() > testDist) { continue; }

						std::tuple<Vec3, float> queryResult = rayIntersectQuadblockTest(center, directionVector, testQuad);
						float t = std::get<1>(queryResult);
						float dist = std::abs(t);
						if (t > 0.0f && dist < bestDistPos)
						{
							bestDistPos = dist;
							leafHit = quadIndexesToLeaves[testQuadIndex];
						}
					}

					if (!vizMatrix.get(leafA, leafHit))
					{
						vizMatrix.get(leafA, leafHit) = true;
						vizMatrix.get(leafHit, leafA) = true;
						if (leafHit == leafB) { foundLeafABHit = true; }
					}
				}
			}
		}
	}
	return vizMatrix;
}

/*

//tldr: for each of the 9 points of a quadblock, sample N rays from fibbonacci sphere and test collision against all other qoadblocks

//https://stackoverflow.com/questions/9600801/evenly-distributing-n-points-on-a-sphere/44164075#44164075

results: 2dMatrix

for each quadblock: q1
	for each point in q1: p
		for each ray emitted from fibbonacci unit sphere sample * 100 or something: r
			for each quadblock: q2
				if (p + r intersects with high or low LOD of q2)
					results.add(q2, q1) //q2 can be seen by q1

*/

/*

//tldr: for n random points in a bsp leaf, sample N rays from a fibbonaci/random sphere and test collision against all other quadblocks

results: 2dMatrix

for each bsp leaf: b
	for each randomly uniform sampled point in b: p
		for each ray emitted from fibbonacci unit sphere sample * 100 or something: r
			for each quadblock: q2
				if (p + r intersects with high or low LOD of q2)
					results.add(q2, q1) //q2 can be seen by q1

*/

/*

bias ideas:

if a bsp leaf has predominantly quadblocks that face the sky (i.e., that bsp leaf is mostly "floor"), then checking visibility for stuff "under" the floor (i.e., against the normal vectors)
is probably a waste of time. Instead, get the average direction + strength of all normal vectors and bias your search moreso in that direction.

*/

/*

The unfortunate downside of what I've done so far is that the "default state" is that everything is invisible, then stuff is "ruled in" as time goes on. Maybe it would be better to figure out
a way to do the opposite?

Make a progressive system that becomes more & more accurate, and it can be paused and chosen if "good enough", ideally a system where each iteration is consistent & can be resumed maybe?

*/