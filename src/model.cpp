#include "model.h"

Model::Model(const Mesh* mesh, const glm::vec3& position, const glm::vec3& scale, const glm::quat& rotation)
{
  m_mesh = mesh;
  m_position = position;
  m_scale = scale;
  m_rotation = rotation;
}

void Model::Draw()
{
	if (m_mesh == nullptr) { return; }

  m_mesh->Bind();
	m_mesh->Draw();
	m_mesh->Unbind();
}

const Mesh* Model::GetMesh()
{
  return m_mesh;
}

void Model::SetMesh(const Mesh* newMesh)
{
  m_mesh = newMesh;
}

glm::mat4 Model::CalculateModelMatrix()
{
  glm::mat4 model = glm::mat4(1.0f); // scale * rotate * translate
  model = glm::translate(model, m_position);
  model *= static_cast<glm::mat<4, 4, float, glm::packed_highp>>(m_rotation);
  model = glm::scale(model, m_scale);
  return model;
}