#include "mesh.h"

void Mesh::Bind() const
{
  if (m_VAO != 0)
  {
    glBindVertexArray(m_VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    {
      GLenum err = glGetError();
			if (err != GL_NO_ERROR) { fprintf(stderr, "Error a! %d\n", err); }
    }
  }

  if (m_shaderSettings & Mesh::ShaderSettings::DrawBackfaces)
  {
    glDisable(GL_CULL_FACE);
  }
  else
  {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
  }
  if (m_shaderSettings & Mesh::ShaderSettings::DrawWireframe)
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  else
  {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  if (m_shaderSettings & Mesh::ShaderSettings::ForceDrawOnTop)
  {
    glDisable(GL_DEPTH_TEST);
  }
  else
  {
    glEnable(GL_DEPTH_TEST);
  }
  if (m_shaderSettings & Mesh::ShaderSettings::DrawLinesAA)
  {
    glEnable(GL_LINE_SMOOTH);
  }
  else
  {
    glDisable(GL_LINE_SMOOTH);
  }
}

void Mesh::Unbind() const
{
  glBindVertexArray(0);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(3);
  {
    GLenum err = glGetError();
		if (err != GL_NO_ERROR) { fprintf(stderr, "Error b! %d\n", err); }
  }
}

void Mesh::Draw() const
{
  GLuint currentProgram;
  glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&currentProgram);
	if (m_VAO != 0) { glDrawArrays(GL_TRIANGLES, 0, m_dataBufSize / sizeof(float)); }
}

/// <summary>
/// When passing data[], any present data according to the "includedDataFlags" is expected to be in this order:
///
/// vertex/position data (always assumed to be present).
/// barycentric (1, 0, 0), (0, 1, 0), (0, 0, 1).
/// normal
/// vcolor
/// stuv_1
/// </summary>
void Mesh::UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned shadSettings, bool dataIsInterlaced)
{
  Dispose();

  includedDataFlags |= VBufDataType::VertexPos;

  m_dataBufSize = static_cast<unsigned>(data.size() * sizeof(float));
  m_includedData = includedDataFlags;
  m_shaderSettings = shadSettings;

  glGenVertexArrays(1, &m_VAO);
  glBindVertexArray(m_VAO);

  glGenBuffers(1, &m_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  glBufferData(GL_ARRAY_BUFFER, m_dataBufSize, data.data(), GL_STATIC_DRAW);

  int ultimateStrideSize = 0;
  for (size_t i = 0; i < sizeof(int) * 8; i++)
  {
    switch ((includedDataFlags) & (1 << i))
    {
    case VBufDataType::VertexPos: //dimension = 3
      ultimateStrideSize += 3;
      break;
    case VBufDataType::Barycentric: //dimension = 3
      ultimateStrideSize += 3;
      break;
    case VBufDataType::VColor: //dimension = 3
      ultimateStrideSize += 3;
      break;
    case VBufDataType::Normals: //dimension = 3
      ultimateStrideSize += 3;
      break;
    case VBufDataType::STUV_1: //undecided 2/4 idk probably 2
      fprintf(stderr, "Unimplemented VBufDataType::STUV_1 in Mesh::UpdateMesh()");
      throw 0;
      break;
    }
  }

  if (dataIsInterlaced)
  {
    //although this works, it's possible that what I'm trying to do here can be done much more simply via some opengl feature(s).
    for (int openglPositionCounter = 0, takenSize = 0, takenCount = 0; openglPositionCounter < 5 /*# of flags in VBufDataType*/; openglPositionCounter++)
    {
      switch (1 << openglPositionCounter)
      {
			case VBufDataType::VertexPos: //position
        {
          constexpr int dim = 3;
          glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*)(takenSize * sizeof(float)));
          glEnableVertexAttribArray(takenCount);
          takenCount++;
          takenSize += dim;
        }
        break;
			case VBufDataType::Barycentric: //barycentric
        {
        constexpr int dim = 3;
        glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*)(takenSize * sizeof(float)));
        glEnableVertexAttribArray(takenCount);
        takenCount++;
        takenSize += dim;
        }
        break;
      case VBufDataType::VColor:
        if (includedDataFlags & VBufDataType::VColor)
        {
          constexpr int dim = 3;
          glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*)(takenSize * sizeof(float)));
          glEnableVertexAttribArray(takenCount);
          takenCount++;
          takenSize += dim;
        }
        break;
      case VBufDataType::Normals:
        if (includedDataFlags & VBufDataType::Normals)
        {
          constexpr int dim = 3;
          glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*)(takenSize * sizeof(float)));
          glEnableVertexAttribArray(takenCount);
          takenCount++;
          takenSize += dim;
        }
        break;
      case VBufDataType::STUV_1:
        if (includedDataFlags & VBufDataType::STUV_1)
        {
          fprintf(stderr, "Unimplemented VBufDataType::STUV_1 in Mesh::UpdateMesh()");
          throw 0;
        }
        break;
      }
    }
  }
  else
  {
    fprintf(stderr, "Unimplemented dataIsInterlaced=false in Mesh::UpdateMesh()");
    throw 0;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

int Mesh::GetDatas() const
{
  return m_includedData;
}

int Mesh::GetShaderSettings() const
{
  return m_shaderSettings;
}

void Mesh::SetShaderSettings(unsigned shadSettings)
{
  m_shaderSettings = shadSettings;
}

void Mesh::Dispose()
{
	if (m_VAO != 0) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
	if (m_VBO != 0) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
  m_dataBufSize = 0;
}