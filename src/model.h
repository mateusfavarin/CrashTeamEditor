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
  glm::mat4 CalculateModelMatrix() const;
  Mesh& GetMesh();
	void SetRenderCondition(const std::function<bool()>& renderCondition);
  void Draw() const;
	void Clear();
	bool IsReady() const;

private:
	Mesh m_mesh;
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
	std::function<bool()> m_renderCondition;
};