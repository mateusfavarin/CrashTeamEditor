#pragma once

#include "mesh.h"
#include "transform.h"

#include <functional>

class Model : public Transform
{
public:
	Model();
  Mesh& GetMesh();
	void SetRenderCondition(const std::function<bool()>& renderCondition);
	void Clear();
	bool IsReady() const;

private:
  void Draw() const;

private:
	Mesh m_mesh;
	std::function<bool()> m_renderCondition;

	friend class Renderer;
};
