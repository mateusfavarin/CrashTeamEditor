#include "viztree.h"
#define _USE_MATH_DEFINES
#include "math.h"

#include <random>

class BitMatrix
{
  size_t width, height;
  //std::vector<bool> has a special optimized implementation that packs the bits, however,
  //the "get" function (that returns a ref) wouldn't work as-is. Fix me later probably.
  std::vector<char> data;
public:
  BitMatrix(size_t width, size_t height) : width(width), height(height), data(width * height) {}

  char& get(size_t x, size_t y)
  {
    return data[(y * width) + x];
  }

  bool read(size_t x, size_t y) const
  {
    return static_cast<bool>(data[(y * width) + x]);
  }

  BitMatrix transposed() const 
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

  BitMatrix operator|(const BitMatrix& other) const 
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

  size_t getWidth() const
  {
    return width;
  }

  size_t getHeight() const
  {
    return height;
  }
private:

};

struct Vector3
{
  double x, y, z;

  Vector3() : x(0.0), y(0.0), z(0.0) {}
  Vector3(double x, double y, double z) : x(x), y(y), z(z) {}

  Vector3 operator*(double other) const {
    Vector3 result;
    result.x = this->x * other;
    result.y = this->y * other;
    result.z = this->z * other;
    return result;
  }

  Vector3 operator+(const Vector3& other) const {
    Vector3 result;
    result.x = this->x + other.x;
    result.y = this->y + other.y;
    result.z = this->z + other.z;
    return result;
  }

  Vector3 operator-(const Vector3& other) const {
    Vector3 result;
    result.x = this->x - other.x;
    result.y = this->y - other.y;
    result.z = this->z - other.z;
    return result;
  }

  double dot(const Vector3& other) const {
    Vector3 result;
    result.x = this->x * other.x;
    result.y = this->y * other.y;
    result.z = this->z * other.z;
    return result.x + result.y + result.z;
  }

  Vector3 cross(const Vector3& other) const {
    Vector3 result;
    result.x = (this->y * other.z) - (this->z * other.y);
    result.y = (this->z * other.x) - (this->x * other.z);
    result.z = (this->x * other.y) - (this->y * other.x);
    return result;
  }

  static Vector3 zero()
  {
    return Vector3();
  }

  static Vector3 one()
  {
    return Vector3(1, 1, 1);
  }
};

static double randomInRange(double low, double high)
{
  /*static */ 
  std::uniform_real_distribution<double> unif(low, high); //this initializes it to the distribution when it is first called, this doesn't work
  static std::default_random_engine re;

  return unif(re);
}

static Vector3 sampleInRectangularPrism(Vector3 p1, Vector3 p2)
{
  return Vector3(randomInRange(p1.x, p2.x), randomInRange(p1.y, p2.y), randomInRange(p1.z, p2.z));
}

static Vector3 sampleOnUnitSphere()
{
  double u = randomInRange(0.0, 1.0);
  double v = randomInRange(0.0, 1.0);

  double theta = 2.0 * M_PI * u;      // azimuthal angle
  double phi = std::acos(2.0 * v - 1.0);  // polar angle

  double x = std::sin(phi) * std::cos(theta);
  double y = std::sin(phi) * std::sin(theta);
  double z = std::cos(phi);

  return Vector3(x, y, z);
}

static Vector3 sampleOnUnitHemisphere()
{
  double u = randomInRange(0.0, 1.0);
  double v = randomInRange(0.0, 1.0);

  double theta = 2.0 * M_PI * u;      // azimuthal angle
  double phi = std::acos(2.0 * v - 1.0);  // polar angle

  double x = std::sin(phi) * std::cos(theta);
  double y = std::sin(phi) * std::sin(theta);
  double z = std::cos(phi);

  return Vector3(std::abs(x), y, z);
}

static std::tuple<Vector3, double> worldspaceRayTriIntersection(Vector3 worldSpaceRayOrigin, Vector3 worldSpaceRayDir, const Vector3 tri[3])
{
  constexpr double epsilon = 0.00001;

  //moller-trumbore intersection test
  //https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

  Vector3 edge_1 = tri[1] - tri[0], edge_2 = tri[2] - tri[0];
  Vector3 ray_cross_e2 = worldSpaceRayDir.cross(edge_2);
  double det = edge_1.dot(ray_cross_e2);

  if (std::abs(det) < epsilon)
    return std::make_tuple(Vector3::zero(), 0.0); //ray is parallel to plane, couldn't possibly intersect with triangle.

  float inv_det = 1.0f / det;
  Vector3 s = worldSpaceRayOrigin - tri[0];
  float u = inv_det * s.dot(ray_cross_e2);

  if ((u < 0 && std::abs(u) > epsilon) || (u > 1 && std::abs(u - 1.0f) > epsilon))
    return std::make_tuple(Vector3::zero(), 0.0); //fails a barycentric test

  Vector3 s_cross_e1 = s.cross(edge_1);
  float v = inv_det * worldSpaceRayDir.dot(s_cross_e1);

  if ((v < 0 && std::abs(v) > epsilon) || (u + v > 1 && std::abs(u + v - 1) > epsilon))
    return std::make_tuple(Vector3::zero(), 0.0); //fails a barycentric test

  float t = inv_det * edge_2.dot(s_cross_e1); //time value (interpolant)

  if (t > epsilon)
  {
    Vector3 collisionPoint = worldSpaceRayOrigin + (worldSpaceRayDir * t); //this is the point *on* the triangle that was collided with.
    return std::make_tuple(collisionPoint, t);
  }
  else
  {
    //line intersects, but ray does not (i.e., behind camera)
    Vector3 collisionPoint = worldSpaceRayOrigin + (worldSpaceRayDir * t); //this is the point *on* the triangle that was collided with.
    return std::make_tuple(collisionPoint, t);
  }
}

static std::tuple<Vector3, double> rayIntersectQuadblockTest(Vector3 worldSpaceRayOrigin, Vector3 worldSpaceRayDir, const Quadblock& qb)
{
  bool isQuadblock = qb.IsQuadblock();
  const Vertex* verts = qb.GetUnswizzledVertices();

  Vector3 tris[3];
  bool collided = false;

  for (int triIndex = 0; triIndex < (isQuadblock ? 8 : 4); triIndex++)
  {
    if (isQuadblock)
    {
      tris[0] = Vector3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][0]].m_pos.z);
      tris[1] = Vector3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][1]].m_pos.z);
      tris[2] = Vector3(verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadHLODVertArrangements[triIndex][2]].m_pos.z);
    }
    else
    {
      tris[0] = Vector3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][0]].m_pos.z);
      tris[1] = Vector3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][1]].m_pos.z);
      tris[2] = Vector3(verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triHLODVertArrangements[triIndex][2]].m_pos.z);
    }

    std::tuple<Vector3, double> queryResult = worldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, tris);

    collided |= (std::get<1>(queryResult) != 0.0);

    if (collided) { return queryResult; }
  }

  for (int triIndex = 0; triIndex < (isQuadblock ? 2 : 1); triIndex++)
  {
    if (isQuadblock)
    {
      tris[0] = Vector3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][0]].m_pos.z);
      tris[1] = Vector3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][1]].m_pos.z);
      tris[2] = Vector3(verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::quadLLODVertArrangements[triIndex][2]].m_pos.z);
    }
    else
    {
      tris[0] = Vector3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][0]].m_pos.z);
      tris[1] = Vector3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][1]].m_pos.z);
      tris[2] = Vector3(verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.x, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.y, verts[FaceIndexConstants::triLLODVertArrangements[triIndex][2]].m_pos.z);
    }

    std::tuple<Vector3, double> queryResult = worldspaceRayTriIntersection(worldSpaceRayOrigin, worldSpaceRayDir, tris);
    collided |= (std::get<1>(queryResult) != 0.0);

    if (collided) { return queryResult; }
  }

  return std::make_tuple(Vector3::zero(), 0.0);
}

BitMatrix viztree_method_1(const std::vector<const Quadblock>& quadblocks) {
  /* tldr: for each of the 9 points of a quadblock, randomly sample a ray and test collision against all other quadblocks

  results: 2dMatrix

  for each quadblock: q1
    for each point in q1: p
      for each ray emitted from random unit sphere sample * 100 or something: r
        for each quadblock: q2
          if (p + r intersects with high or low LOD of q2)
            results.add(q2, q1) //q2 can be seen by q1
  */
  constexpr int perStepSampleCount = 1000;

  BitMatrix vizMatrix = BitMatrix(quadblocks.size(), quadblocks.size());

  for (size_t sourceQuadIndex = 0; sourceQuadIndex < quadblocks.size(); sourceQuadIndex++)
  {
    const Quadblock& sourceQuad = quadblocks[sourceQuadIndex];
    for (int p_ind = 0; p_ind < 9; p_ind++)
    {
      const Vertex& vert = sourceQuad.GetUnswizzledVertices()[p_ind];
      Vector3 p = Vector3(vert.m_pos.x, vert.m_pos.y, vert.m_pos.z);

      for (int ray_ind = 0; ray_ind < perStepSampleCount; ray_ind++)
      {
        Vector3 r = sampleOnUnitHemisphere(); //hemisphere because the ray test goes in both directions
        std::vector<std::tuple<size_t, double>> negativeSuccesses, positiveSuccesses;
        for (size_t testQuadIndex = 0; testQuadIndex < quadblocks.size(); testQuadIndex++) //we do NOT want to count "sourceQuad" in this iteration. fix this.
        {
          const Quadblock& testQuad = quadblocks[testQuadIndex];
          std::tuple<Vector3, double> queryResult = rayIntersectQuadblockTest(p, r, testQuad);
          double t = std::get<1>(queryResult);
          if (t != 0.0)
          {
            if (t > 0.0)
            {
              positiveSuccesses.push_back(std::make_tuple(testQuadIndex, t));
            }
            else
            {
              negativeSuccesses.push_back(std::make_tuple(testQuadIndex, t));
            }
          }
        }

        std::sort(negativeSuccesses.begin(), negativeSuccesses.end(),
          [](const std::tuple<size_t, double>& a, const std::tuple<size_t, double>& b) {
            return std::get<1>(a) > std::get<1>(b);
          });

        std::sort(positiveSuccesses.begin(), positiveSuccesses.end(),
          [](const std::tuple<size_t, double>& a, const std::tuple<size_t, double>& b) {
            return std::get<1>(a) < std::get<1>(b);
          });

        if (negativeSuccesses.size() > 0)
        {
          vizMatrix.get(sourceQuadIndex, std::get<0>(negativeSuccesses[0])) = true;
        }
        if (positiveSuccesses.size() > 0)
        {
          vizMatrix.get(sourceQuadIndex, std::get<0>(positiveSuccesses[0])) = true;
        }
      }
    }
  }

  // These two lines mean: if quadblock A is visible from B, then quadblock B is also visible from A.
  BitMatrix transposedVizMatrix = vizMatrix.transposed();
  vizMatrix = vizMatrix | transposedVizMatrix;

  return vizMatrix;
}

BitMatrix quadVizToBspViz(const BitMatrix& quadViz, const std::vector<const BSP&>& leaves)
{
  BitMatrix bspViz = BitMatrix(leaves.size(), leaves.size());

  for (size_t a = 0; a < leaves.size(); a++)
  {
    const BSP leaf_a = leaves[a];
    const std::vector<size_t>& qbIndeces_a = leaf_a.GetQuadblockIndexes();
    for (size_t b = 0; b < leaves.size(); b++)
    {
      const BSP leaf_b = leaves[b];
      const std::vector<size_t>& qbIndeces_b = leaf_b.GetQuadblockIndexes();
      for (size_t aqb = 0; aqb < qbIndeces_a.size(); aqb++)
      {
        size_t aqbIndex = qbIndeces_a[aqb];
        for (size_t bqb = 0; bqb < qbIndeces_b.size(); aqb++)
        {
          size_t bqbIndex = qbIndeces_a[bqb];

          if (quadViz.read(aqbIndex, bqbIndex))
          {
            bspViz.get(a, b) = bspViz.get(b, a) = 1;
          }
        }
      }
    }
  }

  return bspViz;
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