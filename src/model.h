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
  Mesh& GetMesh();
	void SetPosition(const Vec3& pos);
	void SetScale(const Vec3& scale);
	void SetRotation(const Vec3& rotation);
	Vec3 GetPosition() const;
	Vec3 GetScale() const;
	Vec3 GetRotation() const;
	void SetRenderCondition(const std::function<bool()>& renderCondition);
	void Clear();
	bool IsReady() const;

private:
  void Draw() const;
  glm::mat4 CalculateModelMatrix() const;

private:
	Mesh m_mesh;
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
	std::function<bool()> m_renderCondition;

	friend class Renderer;
};