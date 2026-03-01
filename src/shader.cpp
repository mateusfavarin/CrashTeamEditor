#include "shader.h"

#define LOG_BUF_SIZE 1024
static char logBuf[LOG_BUF_SIZE];
static constexpr GLuint NO_SHADER = -1;

Shader::Shader()
{
  m_programId = NO_SHADER;
}

Shader::Shader(const char* vertShader, const char* fragShader)
{
  /* vertex shader */
  GLuint vert = GL_CHECK_RET(glCreateShader(GL_VERTEX_SHADER));
  GL_CHECK(glShaderSource(vert, 1, &vertShader, NULL));
  GL_CHECK(glCompileShader(vert));
  GLint success = 0;
  GL_CHECK(glGetShaderiv(vert, GL_COMPILE_STATUS, &success));
  if (!success)
  {
    GL_CHECK(glGetShaderInfoLog(vert, LOG_BUF_SIZE, NULL, logBuf));
  }
  /* fragment shader */
  GLuint frag = GL_CHECK_RET(glCreateShader(GL_FRAGMENT_SHADER));
  GL_CHECK(glShaderSource(frag, 1, &fragShader, NULL));
  GL_CHECK(glCompileShader(frag));
  success = 0;
  GL_CHECK(glGetShaderiv(frag, GL_COMPILE_STATUS, &success));
  if (!success)
  {
    GL_CHECK(glGetShaderInfoLog(frag, LOG_BUF_SIZE, NULL, logBuf));
    fprintf(stderr, "Error, fragment shader compilation failed:\n%s", logBuf);
  }

	m_programId = GL_CHECK_RET(glCreateProgram());
  GL_CHECK(glAttachShader(m_programId, vert));
  GL_CHECK(glAttachShader(m_programId, frag));
  GL_CHECK(glLinkProgram(m_programId));
  success = 0;
  GL_CHECK(glGetProgramiv(m_programId, GL_LINK_STATUS, &success));
  if (!success)
  {
    GL_CHECK(glGetShaderInfoLog(m_programId, LOG_BUF_SIZE, NULL, logBuf));
    fprintf(stderr, "Error, shader link failed:\n%s", logBuf);
  }
  GL_CHECK(glDeleteShader(vert));
  GL_CHECK(glDeleteShader(frag));
}

void Shader::Dispose() const
{
	if (m_programId != NO_SHADER) { GL_CHECK(glDeleteProgram(m_programId)); }
}

void Shader::Use() const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  GL_CHECK(glUseProgram(m_programId));
}

void Shader::Unuse() const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  GL_CHECK(glUseProgram(0));
}

void Shader::SetUniform(const char* name, bool value) const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  const GLint location = GL_CHECK_RET(glGetUniformLocation(m_programId, name));
  GL_CHECK(glUniform1i(location, static_cast<int>(value)));
}

void Shader::SetUniform(const char* name, int value) const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  const GLint location = GL_CHECK_RET(glGetUniformLocation(m_programId, name));
  GL_CHECK(glUniform1i(location, value));
}

void Shader::SetUniform(const char* name, float value) const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  const GLint location = GL_CHECK_RET(glGetUniformLocation(m_programId, name));
  GL_CHECK(glUniform1f(location, value));
}

void Shader::SetUniform(const char* name, const glm::mat4& mat) const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  const GLint location = GL_CHECK_RET(glGetUniformLocation(m_programId, name));
  GL_CHECK(glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat)));
}

void Shader::SetUniform(const char* name, const glm::vec3& vec3) const
{
  if (m_programId == NO_SHADER) { fprintf(stderr, "Error, tried to use a shader that wasn't created with vert/frag shaders. Do not use the default constructor."); }
  const GLint location = GL_CHECK_RET(glGetUniformLocation(m_programId, name));
  GL_CHECK(glUniform3f(location, vec3.x, vec3.y, vec3.z));
}
#undef LOG_BUF_SIZE
