#include <map>
#include <bit>

#include "mesh.h"
#include "vertex.h"
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

static int GetStrideFloats(unsigned includedDataFlags)
{
	int stride = 9; // position + color + normal
	if ((includedDataFlags & Mesh::VBufDataType::UV) != 0) { stride += 3; }
	return stride;
}

Mesh::Mesh()
{
	Clear();
}

void Mesh::SetGeometry(const std::vector<Tri>& triangles, unsigned shaderSettings)
{
	m_triCount = triangles.size();
	m_textureStoreData.clear();
	m_textureStoreIndex.clear();
	DeleteTextures();

	bool hasUVs = false;
	for (const Tri& tri : triangles)
	{
		if (!tri.texture.empty()) { hasUVs = true; break; }
	}

	std::vector<float> data;
	for (const Tri& tri : triangles)
	{
		if (hasUVs && !tri.texture.empty())
		{
			if (!m_textureStoreIndex.contains(tri.texture))
			{
				m_textureStoreIndex[tri.texture] = m_textureStoreData.size();
				m_textureStoreData.push_back(LoadTextureData(tri.texture));
			}
		}

		for (const Point& point : tri.p)
		{
			data.push_back(point.pos.x);
			data.push_back(point.pos.y);
			data.push_back(point.pos.z);

			data.push_back(point.color.Red());
			data.push_back(point.color.Green());
			data.push_back(point.color.Blue());

			data.push_back(point.normal.x);
			data.push_back(point.normal.y);
			data.push_back(point.normal.z);

			if (hasUVs)
			{
				data.push_back(point.uv.x);
				data.push_back(point.uv.y);
				data.push_back(std::bit_cast<float>(static_cast<int>(m_textureStoreIndex[tri.texture])));
			}
		}
	}

	if (!m_textureStoreData.empty())
	{
		RebuildTextureData();
	}

	const unsigned includedDataFlags = hasUVs ? VBufDataType::UV : VBufDataType::None;
	UpdateMesh(data, includedDataFlags, shaderSettings);
}

void Mesh::Bind() const
{
	if (m_VAO != 0)
	{
		glBindVertexArray(m_VAO);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		if (m_includedData & VBufDataType::UV)
		{
			glEnableVertexAttribArray(3);
			glEnableVertexAttribArray(4);
		}
		if (m_textures)
		{
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
		glLineWidth((m_shaderSettings & Mesh::ShaderSettings::ThickLines) ? 2.5f : 1.0f);
	}
	else
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);
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
* vcolor
* normal
* uv (if present)
* texindex
*/
void Mesh::UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned shadSettings)
{
	includedDataFlags &= VBufDataType::UV;
	const bool hasUV = (includedDataFlags & VBufDataType::UV) != 0;
	const bool reuseBuffers = (m_VAO != 0 && m_VBO != 0 && m_includedData == includedDataFlags);
	if (!reuseBuffers) { Dispose(); }

	const unsigned buffSize = static_cast<unsigned>(data.size() * sizeof(float));
	m_includedData = includedDataFlags;
	m_shaderSettings = shadSettings;
	m_vertexCount = 0;

	int ultimateStrideSize = GetStrideFloats(includedDataFlags);
	if (ultimateStrideSize > 0)
	{
		m_vertexCount = static_cast<int>(data.size() / ultimateStrideSize);
	}

	if (reuseBuffers)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, buffSize, data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return;
	}

	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	glGenBuffers(1, &m_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, buffSize, data.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (0 * sizeof(float)));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	if (hasUV)
	{
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, ultimateStrideSize * sizeof(float), (void*) (9 * sizeof(float)));
		glEnableVertexAttribArray(3);
		glVertexAttribIPointer(4, 1, GL_INT, ultimateStrideSize * sizeof(float), (void*) (11 * sizeof(float)));
		glEnableVertexAttribArray(4);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Mesh::UpdateTriangle(const Tri& tri, size_t triangleIndex)
{
	if (m_VBO == 0) { return; }
	if (triangleIndex >= m_triCount) { return; }

	const int stride = GetStrideFloats(m_includedData);
	if (stride <= 0) { return; }

	int texIndex = 0;
	const bool hasUV = (m_includedData & VBufDataType::UV) != 0;
	if (hasUV)
	{
		const std::filesystem::path texturePath = tri.texture;
		if (!texturePath.empty() && !m_textureStoreIndex.contains(texturePath))
		{
			AppendTextureStore(texturePath);
		}
		texIndex = static_cast<int>(m_textureStoreIndex[texturePath]);
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	for (size_t i = 0; i < 3; i++)
	{
		const Point& point = tri.p[i];
		const size_t vertexIndex = triangleIndex * 3 + i;

		const float colorData[3] = { point.color.Red(), point.color.Green(), point.color.Blue() };
		glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>((vertexIndex * stride + 0) * sizeof(float)), sizeof(point.pos), point.pos.Data());
		glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>((vertexIndex * stride + 3) * sizeof(float)), sizeof(colorData), colorData);
		glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>((vertexIndex * stride + 6) * sizeof(float)), sizeof(point.normal), point.normal.Data());

		if (hasUV)
		{
			const float texIndexData = std::bit_cast<float>(texIndex);
			glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>((vertexIndex * stride + 9) * sizeof(float)), sizeof(point.uv), point.uv.Data());
			glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>((vertexIndex * stride + 11) * sizeof(float)), sizeof(texIndexData), &texIndexData);
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
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
	DeleteTextures();

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

void Mesh::Clear()
{
	Dispose();
	DeleteTextures();
	m_includedData = 0;
	m_shaderSettings = 0;
	m_textureStoreData.clear();
	m_textureStoreIndex.clear();
}

void Mesh::Dispose()
{
	if (m_VAO != 0) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
	if (m_VBO != 0) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
	m_vertexCount = 0;
}

void Mesh::DeleteTextures()
{
	if (m_textures == 0) { return; }
	glDeleteTextures(1, &m_textures);
	m_textures = 0;
}
