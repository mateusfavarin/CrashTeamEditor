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
	Model() {};
  Model(Mesh* mesh, glm::vec3 position = glm::vec3(0.f, 0.f, 0.f), glm::vec3 scale = glm::vec3(1.f, 1.f, 1.f), glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f));
  glm::mat4 CalculateModelMatrix();
  Mesh* GetMesh();
  void SetMesh(Mesh* newMesh = nullptr);
  void Draw();

private:
	Mesh* m_mesh = nullptr;
	glm::vec3 m_position = glm::vec3(0.f, 0.f, 0.f);
	glm::vec3 m_scale = glm::vec3(1.f, 1.f, 1.f);
	glm::quat m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
};