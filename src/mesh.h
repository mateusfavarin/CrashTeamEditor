#pragma once

#include "globalimguiglglfw.h"
#include "manual_third_party/glm/glm.hpp"
#include "manual_third_party/glm/gtc/matrix_transform.hpp"
#include "manual_third_party/glm/gtc/type_ptr.hpp"

class Mesh {
private:
  GLuint VAO = 0, VBO = 0;
  int dataBufSize = 0;
  /*friend Model Bind();
  friend Model Unbind();
  friend Model Draw();*/
public:
  //don't call these directly. Use the Model class.
  void Bind();
  void Unbind();
  void Draw();
  void UpdateMesh(float data[], int dataBufSize);
  Mesh(float data[], int dataBufSize);
  Mesh();
  void Dispose();
};