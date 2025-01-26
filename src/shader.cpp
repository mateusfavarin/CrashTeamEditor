#include "shader.h"

#define LOG_BUF_SIZE 1024
static char logBuf[LOG_BUF_SIZE];

Shader::Shader() //todo make this a friend of std::map then make this private.
{
  this->programId = -1;
}

Shader::Shader(const char* vertShader, const char* fragShader)
{
  //geometry shader (do we need this? maybe for accurately displaying quad normals).
  //vertex shader
  GLuint vert = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vert, 1, &vertShader, NULL);
  glCompileShader(vert);
  GLint success = 0;
  glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(vert, LOG_BUF_SIZE, NULL, logBuf);
    fprintf(stderr, "Error, vertex shader compilation failed:\n%s", logBuf);
    throw 0;
  }
  //fragment shader
  GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(frag, 1, &fragShader, NULL);
  glCompileShader(frag);
  success = 0;
  glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(frag, LOG_BUF_SIZE, NULL, logBuf);
    fprintf(stderr, "Error, fragment shader compilation failed:\n%s", logBuf);
    throw 0;
  }
  //link
  this->programId = glCreateProgram();
  glAttachShader(this->programId, vert);
  glAttachShader(this->programId, frag);
  glLinkProgram(this->programId);
  success = 0;
  glGetProgramiv(this->programId, GL_LINK_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(this->programId, LOG_BUF_SIZE, NULL, logBuf);
    fprintf(stderr, "Error, shader link failed:\n%s", logBuf);
    throw 0;
  }
  glDeleteShader(vert);
  glDeleteShader(frag);
}

void Shader::Dispose()
{
  if (this->programId != -1)
    glDeleteProgram(this->programId);
}

void Shader::use()
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUseProgram(this->programId);
}

void Shader::unuse()
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUseProgram(0);
}

void Shader::setUniform(const char* name, bool value) const
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform1i(glGetUniformLocation(this->programId, name), (int)value);
}

void Shader::setUniform(const char* name, int value) const
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform1i(glGetUniformLocation(this->programId, name), value);
}

void Shader::setUniform(const char* name, float value) const
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform1f(glGetUniformLocation(this->programId, name), value);
}

void Shader::setUniform(const char* name, glm::mat4 mat) const
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniformMatrix4fv(glGetUniformLocation(this->programId, name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setUniform(const char* name, glm::vec3 vec3) const
{
  if (this->programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform3f(glGetUniformLocation(this->programId, name), vec3.x, vec3.y, vec3.z);
}
#undef LOG_BUF_SIZE