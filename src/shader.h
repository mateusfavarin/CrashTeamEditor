#pragma once

#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
//#include "mesh.h"

class Shader
{
public:
  Shader();
  Shader(const char* vertShader, const char* fragShader);
  void Use() const;
  void Unuse() const;
  void SetUniform(const char* name, bool value) const;
  void SetUniform(const char* name, int value) const;
  void SetUniform(const char* name, float value) const;
  void SetUniform(const char* name, const glm::mat4& mat) const;
  void SetUniform(const char* name, const glm::vec3& vec3) const;
  void Dispose() const;

private:
	GLuint m_programId;
};
