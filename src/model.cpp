#include "model.h"

#include <algorithm>

#include "text3d.h"

Model::Model()
{
	Clear(true);
	m_renderCondition = []() { return true; };
}

Model::~Model()
{
	ClearModels();
}

Mesh& Model::GetMesh()
{
  return m_mesh;
}

void Model::SetRenderCondition(const std::function<bool()>& renderCondition)
{
	m_renderCondition = renderCondition;
}

Model* Model::AddModel()
{
	m_child.push_back(new Model());
	return m_child.back();
}

void Model::ClearModels()
{
	for (Model* model : m_child) { delete model; }
	m_child.clear();
}

bool Model::RemoveModel(Model* model)
{
	if (!model) { return false; }

	size_t count = std::erase(m_child, model);
	if (count == 0) { return false; }

	delete model;
	return true;
}

void Model::Clear(bool models)
{
	if (models) { ClearModels(); }
	m_mesh.Clear();
	Transform::Clear();
}

bool Model::IsReady() const
{
	return m_mesh.IsReady() && m_renderCondition();
}
