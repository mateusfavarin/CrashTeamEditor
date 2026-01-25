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

void Model::SetRenderCondition(const std::function<bool()>& renderCondition)
{
	m_renderCondition = renderCondition;
}

void Model::Clear()
{
	m_mesh.Clear();
	Transform::Clear();
}

bool Model::IsReady() const
{
	return m_mesh.IsReady() && m_renderCondition();
}
