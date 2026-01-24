#include "model.h"

Model::Model()
{
	Clear();
	m_renderCondition = []() { return true; };
}

void Model::Draw()
{
	if (m_mesh == nullptr) { return; }

  m_mesh->Bind();
	m_mesh->Draw();
	m_mesh->Unbind();
}

Mesh* Model::GetMesh()
{
  return m_mesh;
}

void Model::SetMesh(Mesh* newMesh)
{
  m_mesh = newMesh;
}

void Model::SetRenderCondition(const std::function<bool()>& renderCondition)
{
	m_renderCondition = renderCondition;
}

glm::mat4 Model::CalculateModelMatrix()
{
  glm::mat4 model = glm::mat4(1.0f); // scale * rotate * translate
  model = glm::translate(model, m_position);
  model *= static_cast<glm::mat<4, 4, float, glm::packed_highp>>(m_rotation);
  model = glm::scale(model, m_scale);
  return model;
}

void Model::Clear()
{
	m_mesh = nullptr;
	m_position = glm::vec3(0.f, 0.f, 0.f);
	m_scale = glm::vec3(1.f, 1.f, 1.f);
	m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
}