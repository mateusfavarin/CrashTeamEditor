#pragma once

#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
//#include "mesh.h"

class Shader
{
private:
  GLuint programId;
public:
  Shader();
  Shader(const char* geomShader, const char* vertShader, const char* fragShader);
  void use();
  void unuse();
  void setUniform(const char* name, bool value) const;
  void setUniform(const char* name, int value) const;
  void setUniform(const char* name, float value) const;
  void setUniform(const char* name, glm::mat4 mat) const;
  void setUniform(const char* name, glm::vec3 vec3) const;
  void Dispose();
};