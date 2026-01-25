#include "model.h"

Model::Model()
{
	Clear();
	m_renderCondition = []() { return true; };
}

void Model::Draw() const
{
  m_mesh.Bind();
	m_mesh.Draw();
	m_mesh.Unbind();
}

Mesh& Model::GetMesh()
{
  return m_mesh;
}

void Model::SetPosition(const Vec3& pos)
{
	m_position.x = pos.x;
	m_position.y = pos.y;
	m_position.z = pos.z;
}

void Model::SetScale(const Vec3& scale)
{
	m_scale.x = scale.x;
	m_scale.y = scale.y;
	m_scale.z = scale.z;
}

void Model::SetRotation(const Vec3& rotation)
{
	m_rotation = glm::quat(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z)));
}

Vec3 Model::GetPosition() const
{
	return Vec3(m_position.x, m_position.y, m_position.z);
}

Vec3 Model::GetScale() const
{
	return Vec3(m_scale.x, m_scale.y, m_scale.z);
}

Vec3 Model::GetRotation() const
{
	glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(m_rotation));
	return Vec3(eulerAngles.x, eulerAngles.y, eulerAngles.z);
}

void Model::SetRenderCondition(const std::function<bool()>& renderCondition)
{
	m_renderCondition = renderCondition;
}

glm::mat4 Model::CalculateModelMatrix() const
{
  glm::mat4 model = glm::mat4(1.0f); // scale * rotate * translate
  model = glm::translate(model, m_position);
  model *= static_cast<glm::mat<4, 4, float, glm::packed_highp>>(m_rotation);
  model = glm::scale(model, m_scale);
  return model;
}

void Model::Clear()
{
	m_mesh.Clear();
	m_position = glm::vec3(0.f, 0.f, 0.f);
	m_scale = glm::vec3(1.f, 1.f, 1.f);
	m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
}

bool Model::IsReady() const
{
	return m_mesh.IsReady() && m_renderCondition();
}