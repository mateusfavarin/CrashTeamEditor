#include "model.h"

Model::Model(Mesh* mesh, glm::vec3 position, glm::vec3 scale, glm::quat rotation)
{
  this->mesh = mesh;
  this->position = position;
  this->scale = scale;
  this->rotation = rotation;
}

Model::Model() : Model(nullptr) { }

void Model::Draw()
{
  if (this->mesh != nullptr)
  {
    this->mesh->Bind();
    this->mesh->Draw();
    this->mesh->Unbind();
  }
}

Mesh* Model::GetMesh()
{
  return this->mesh;
}

void Model::SetMesh(Mesh* newMesh)
{
  this->mesh = newMesh;
}

glm::mat4 Model::CalculateModelMatrix()
{
  glm::mat4 model = glm::mat4(1.0f); // scale * rotate * translate
  model = glm::translate(model, this->position);
  model *= (glm::mat<4, 4, float, glm::packed_highp>)this->rotation;
  model = glm::scale(model, this->scale);
  return model;
}