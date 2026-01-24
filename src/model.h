#pragma once

#include "glm.hpp"
#include "common.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "gtc/quaternion.hpp"
#include "mesh.h"

class Model
{
public:
	Model();
  Model(Mesh* mesh, const glm::vec3& position, const glm::vec3& scale, const glm::quat& rotation);
  glm::mat4 CalculateModelMatrix();
  Mesh* GetMesh();
  void SetMesh(Mesh* newMesh = nullptr);
  void Draw();
	void Clear();

private:
	Mesh* m_mesh;
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
};