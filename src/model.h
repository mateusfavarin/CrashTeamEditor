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
	Model* AddModel();
	void ClearModels();
	bool RemoveModel(Model* model);
	void Clear(bool models);
	bool IsReady() const;

private:
	Mesh m_mesh;
	std::function<bool()> m_renderCondition;
	std::list<Model*> m_child;

	friend class Renderer;
};
