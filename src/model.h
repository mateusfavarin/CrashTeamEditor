#pragma once

#include "glm.hpp"
#include "common.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "gtc/quaternion.hpp"
#include "mesh.h"

#include <functional>

class Model
{
public:
	Model();
  glm::mat4 CalculateModelMatrix();
  Mesh* GetMesh();
  void SetMesh(Mesh* newMesh = nullptr);
	void SetRenderCondition(const std::function<bool()>& renderCondition);
  void Draw();
	void Clear();

private:
	Mesh* m_mesh;
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
	std::function<bool()> m_renderCondition;

	friend class Renderer;
};