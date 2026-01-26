#pragma once

#include "mesh.h"
#include "text3d.h"
#include "transform.h"

#include <functional>
#include <list>
#include <string>

class Model : public Transform
{
public:
	Model();
	~Model();
  Mesh& GetMesh();
	void SetRenderCondition(const std::function<bool()>& renderCondition);
	Text3D* AddLabel(const std::string& label,
		Text3D::TextAlign align = Text3D::TextAlign::LEFT,
		const Color& color = Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(255u)),
		const Color& backgroundColor = Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u)));
	void RemoveLabels();
	bool RemoveLabel(Text3D* label);
	const std::list<Text3D*>& GetLabels() const;
	void Clear();
	bool IsReady() const;

private:
  void Draw() const;

private:
	Mesh m_mesh;
	std::function<bool()> m_renderCondition;
	std::list<Text3D*> m_labels;

	friend class Renderer;
};
