#include "shader.h"

#define LOG_BUF_SIZE 1024
static char logBuf[LOG_BUF_SIZE];

Shader::Shader() //todo make this a friend of std::map then make this private.
{
  m_programId = static_cast<GLuint>(-1);
}

Shader::Shader(const char* vertShader, const char* fragShader)
{
  /* vertex shader */
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
  /* fragment shader */
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

	m_programId = glCreateProgram();
  glAttachShader(m_programId, vert);
  glAttachShader(m_programId, frag);
  glLinkProgram(m_programId);
  success = 0;
  glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(m_programId, LOG_BUF_SIZE, NULL, logBuf);
    fprintf(stderr, "Error, shader link failed:\n%s", logBuf);
    throw 0;
  }
  glDeleteShader(vert);
  glDeleteShader(frag);
}

void Shader::Dispose() const
{
	if (m_programId != -1) { glDeleteProgram(m_programId); }
}

void Shader::Use() const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUseProgram(m_programId);
}

void Shader::Unuse() const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUseProgram(0);
}

void Shader::SetUniform(const char* name, bool value) const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform1i(glGetUniformLocation(m_programId, name), static_cast<int>(value));
}

void Shader::SetUniform(const char* name, int value) const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform1i(glGetUniformLocation(m_programId, name), value);
}

void Shader::SetUniform(const char* name, float value) const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform1f(glGetUniformLocation(m_programId, name), value);
}

void Shader::SetUniform(const char* name, const glm::mat4& mat) const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniformMatrix4fv(glGetUniformLocation(m_programId, name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::SetUniform(const char* name, const glm::vec3& vec3) const
{
  if (m_programId == -1)
  {
    fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor.");
    throw 0;
  }
  glUniform3f(glGetUniformLocation(m_programId, name), vec3.x, vec3.y, vec3.z);
}
#undef LOG_BUF_SIZE
