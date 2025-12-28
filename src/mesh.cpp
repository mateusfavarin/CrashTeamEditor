#include <map>

#include "mesh.h"
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

void Mesh::Bind() const
{
	if (m_VAO != 0)
	{
		glBindVertexArray(m_VAO);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		if (m_textures)
		{
			glEnableVertexAttribArray(3);
			glEnableVertexAttribArray(4);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, m_textures);
		}
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
	glDisableVertexAttribArray(4);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	{
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) { fprintf(stderr, "Error b! %d\n", err); }
	}
}

void Mesh::Draw() const
{
	if (m_VAO == 0) { return; }
	glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
}

/*
When passing data[], any present data according to the "includedDataFlags" is expected to be in this order:
* vertex/position data (always assumed to be present).
* barycentric (1, 0, 0), (0, 1, 0), (0, 0, 1).
* normal
* vcolor
* stuv_1
*/
void Mesh::UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned shadSettings, bool dataIsInterlaced)
{
	includedDataFlags |= VBufDataType::VertexPos;
	const bool reuseBuffers = (m_VAO != 0 && m_VBO != 0 && m_includedData == includedDataFlags);
	if (!reuseBuffers) { Dispose(); }

	m_dataBufSize = static_cast<unsigned>(data.size() * sizeof(float));
	m_includedData = includedDataFlags;
	m_shaderSettings = shadSettings;
	m_vertexCount = 0;

	int ultimateStrideSize = 0;
	for (size_t i = 0; i < sizeof(int) * 8; i++)
	{
		switch ((includedDataFlags) & (1 << i))
		{
		case VBufDataType::VertexPos:
			ultimateStrideSize += 3;
			break;
		case VBufDataType::Barycentric:
			ultimateStrideSize += 3;
			break;
		case VBufDataType::VertexColor:
			ultimateStrideSize += 3;
			break;
		case VBufDataType::Normals:
			ultimateStrideSize += 3;
			break;
		case VBufDataType::STUV:
			ultimateStrideSize += 2;
			break;
		case VBufDataType::TexIndex:
			ultimateStrideSize += 1;
			break;
		}
	}

	if (ultimateStrideSize > 0)
	{
		m_vertexCount = static_cast<int>(data.size() / ultimateStrideSize);
	}

	if (reuseBuffers && dataIsInterlaced)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, m_dataBufSize, data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return;
	}

	if (!dataIsInterlaced)
	{
		fprintf(stderr, "Unimplemented dataIsInterlaced=false in Mesh::UpdateMesh()");
		throw 0;
	}

	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, m_dataBufSize, data.data(), GL_STATIC_DRAW);

	//although this works, it's possible that what I'm trying to do here can be done much more simply via some opengl feature(s).
	for (int openglPositionCounter = 0, takenSize = 0, takenCount = 0; openglPositionCounter < 6 /*# of flags in VBufDataType*/; openglPositionCounter++)
	{
		switch (1 << openglPositionCounter)
		{
		case VBufDataType::VertexPos:
			if ((includedDataFlags & VBufDataType::VertexPos) != 0)
			{
				constexpr int dim = 3;
				glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (takenSize * sizeof(float)));
				glEnableVertexAttribArray(takenCount);
				takenCount++;
				takenSize += dim;
			}
			break;
		case VBufDataType::Barycentric:
			if ((includedDataFlags & VBufDataType::Barycentric) != 0)
			{
				constexpr int dim = 3;
				glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (takenSize * sizeof(float)));
				glEnableVertexAttribArray(takenCount);
				takenCount++;
				takenSize += dim;
			}
			break;
		case VBufDataType::VertexColor:
			if ((includedDataFlags & VBufDataType::VertexColor) != 0)
			{
				constexpr int dim = 3;
				glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (takenSize * sizeof(float)));
				glEnableVertexAttribArray(takenCount);
				takenCount++;
				takenSize += dim;
			}
			break;
		case VBufDataType::Normals:
			if ((includedDataFlags & VBufDataType::Normals) != 0)
			{
				constexpr int dim = 3;
				glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (takenSize * sizeof(float)));
				glEnableVertexAttribArray(takenCount);
				takenCount++;
				takenSize += dim;
			}
			break;
		case VBufDataType::STUV:
			if ((includedDataFlags & VBufDataType::STUV) != 0)
			{
				constexpr int dim = 2;
				glVertexAttribPointer(takenCount, dim, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (takenSize * sizeof(float)));
				glEnableVertexAttribArray(takenCount);
				takenCount++;
				takenSize += dim;
			}
			break;
		case VBufDataType::TexIndex:
			if ((includedDataFlags & VBufDataType::TexIndex) != 0)
			{
				//this only works because sizeof(float) == sizeof(int)
				constexpr int dim = 1;
				glVertexAttribIPointer(takenCount, dim, GL_INT, ultimateStrideSize * sizeof(int), (void*) (takenSize * sizeof(int)));
				glEnableVertexAttribArray(takenCount);
				takenCount++;
				takenSize += dim;
			}
			break;
		}
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

static constexpr int textureWidth = 256;
static constexpr int textureHeight = 256;
std::vector<unsigned char> Mesh::LoadTextureData(const std::filesystem::path& path)
{
	constexpr int textureChannels = 4;
	constexpr size_t textureLayerSize = static_cast<size_t>(textureWidth) * textureHeight * textureChannels;
	std::vector<unsigned char> data(textureLayerSize);
	int w = 0, h = 0, channels = 0;
	stbi_uc* originalData = stbi_load(path.generic_string().c_str(), &w, &h, &channels, textureChannels);

	if (originalData == nullptr)
	{
		memset(data.data(), 0x7F, data.size());
		for (size_t i = 3; i < data.size(); i += textureChannels) { data[i] = static_cast<unsigned char>(0xFF); }
	}
	else
	{
		memset(data.data(), 0xFF, data.size());
		stbir_resize(originalData, w, h, textureChannels * w, data.data(), textureWidth, textureHeight, textureChannels * textureWidth,
			stbir_pixel_layout::STBIR_RGBA, stbir_datatype::STBIR_TYPE_UINT8, stbir_edge::STBIR_EDGE_CLAMP, stbir_filter::STBIR_FILTER_POINT_SAMPLE);
		stbi_image_free(originalData);
	}
	return data;
}

void Mesh::RebuildTextureData()
{
	if (m_textures) { glDeleteTextures(1, &m_textures); m_textures = 0; }

	glGenTextures(1, &m_textures);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_textures);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, textureWidth, textureHeight, static_cast<GLsizei>(m_textureStoreData.size()), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	for (size_t i = 0; i < m_textureStoreData.size(); i++)
	{
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, static_cast<GLint>(i), textureWidth, textureHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, m_textureStoreData[i].data());
	}
}

void Mesh::UpdateTextureStore(const std::filesystem::path& texturePath)
{
	if (!m_textureStoreIndex.contains(texturePath)) { AppendTextureStore(texturePath); return; }
	m_textureStoreData[m_textureStoreIndex[texturePath]] = LoadTextureData(texturePath);
	RebuildTextureData();
}

void Mesh::AppendTextureStore(const std::filesystem::path& texturePath)
{
	m_textureStoreIndex[texturePath] = m_textureStoreData.size();
	m_textureStoreData.push_back(LoadTextureData(texturePath));
	RebuildTextureData();
}

GLuint Mesh::GetTextureStore() const
{
	return m_textures;
}

void Mesh::Dispose()
{
	if (m_VAO != 0) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
	if (m_VBO != 0) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
	m_dataBufSize = 0;
	m_vertexCount = 0;
}
