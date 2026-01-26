#include "model.h"

#include <algorithm>

#include "text3d.h"

Model::Model()
{
	Clear();
	m_renderCondition = []() { return true; };
}

Model::~Model()
{
	RemoveLabels();
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

Text3D* Model::AddLabel(const std::string& label, Text3D::TextAlign align, const Color& color, const Color& backgroundColor)
{
	m_labels.push_back(new Text3D(label, align, color, backgroundColor));
	return m_labels.back();
}

void Model::RemoveLabels()
{
	for (Text3D* label : m_labels) { delete label; }
	m_labels.clear();
}

bool Model::RemoveLabel(Text3D* label)
{
	if (!label) { return false; }

	size_t count = std::erase(m_labels, label);
	if (count == 0) { return false; }

	delete label;
	return true;
}

const std::list<Text3D*>& Model::GetLabels() const
{
	return m_labels;
}

void Model::Clear()
{
	RemoveLabels();
	m_mesh.Clear();
	Transform::Clear();
}

bool Model::IsReady() const
{
	return m_mesh.IsReady() && m_renderCondition();
}
