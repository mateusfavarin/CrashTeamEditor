#include <map>
#include <bit>
#include <numeric>

#include "mesh.h"
#include "vertex.h"
#include "gui_render_settings.h"
#include "quadblock.h"
#include "text3d.h"

#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

static size_t GetStrideFloats(unsigned includedDataFlags)
{
	size_t stride = 9; // position + color + normal
	if ((includedDataFlags & Mesh::VBufDataType::UV) != 0) { stride += 3; } // uv + texIndex
	return stride;
}

Mesh::Mesh()
{
	Clear();
}

void Mesh::SetGeometry(const std::vector<Primitive>& primitives, unsigned renderFlags, unsigned shaderFlags)
{
	if (primitives.empty()) { Clear(); return; }

	size_t triCount = 0;
	bool hasUVs = false;
	for (const Primitive& primitive : primitives)
	{
		if (!primitive.texture.empty()) { hasUVs = true; }
		switch (primitive.type)
		{
		case PrimitiveType::TRI:
		case PrimitiveType::LINE:
			triCount += 1;
			break;
		case PrimitiveType::QUAD:
			triCount += 2;
			break;
		default:
			break;
		}
	}

	if (triCount == 0) { return; }

	m_triCount = triCount;
	m_textureStoreData.clear();
	m_textureStoreIndex.clear();
	DeleteTextures();

	const unsigned includedDataFlags = hasUVs ? VBufDataType::UV : VBufDataType::None;
	std::vector<float> data;
	data.reserve(GetStrideFloats(includedDataFlags) * triCount * 3);

	auto AppendTriangle = [&](const Point& p0, const Point& p1, const Point& p2, const std::filesystem::path& texturePath)
	{
		int texIndex = 0;
		if (hasUVs && !texturePath.empty())
		{
			if (!m_textureStoreIndex.contains(texturePath))
			{
				m_textureStoreIndex[texturePath] = m_textureStoreData.size();
				m_textureStoreData.push_back(LoadTextureData(texturePath));
			}
			texIndex = static_cast<int>(m_textureStoreIndex[texturePath]);
		}

		const Point points[3] = { p0, p1, p2 };
		for (const Point& point : points)
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
				data.push_back(std::bit_cast<float>(texIndex));
			}
		}
	};

	for (const Primitive& primitive : primitives)
	{
		const std::filesystem::path texturePath = primitive.texture;
		switch (primitive.type)
		{
		case PrimitiveType::TRI:
			AppendTriangle(primitive.p[0], primitive.p[1], primitive.p[2], texturePath);
			break;
		case PrimitiveType::QUAD:
			AppendTriangle(primitive.p[0], primitive.p[1], primitive.p[2], texturePath);
			AppendTriangle(primitive.p[1], primitive.p[2], primitive.p[3], texturePath);
			break;
		case PrimitiveType::LINE:
			AppendTriangle(primitive.p[0], primitive.p[1], primitive.p[1], texturePath);
			break;
		default:
			break;
		}
	}

	if (!m_textureStoreData.empty()) { RebuildTextureData(); }

	m_highLODIndices.clear();
	m_lowLODIndices.clear();
	m_indexCount = 0;
	m_useLowLOD = false;
	if (renderFlags & RenderFlags::QuadblockLod) { BuildLowLODIndices(primitives); }

	UpdateMesh(data, includedDataFlags, renderFlags, shaderFlags);
}

void Mesh::SetGeometry(const std::string& label, Text3D::Align align, const Color& color)
{
	SetGeometry(Text3D::ToGeometry(label, align, color), Mesh::RenderFlags::DontOverrideRenderFlags | Mesh::RenderFlags::DrawBackfaces | Mesh::RenderFlags::FollowCamera);
}

size_t Mesh::UpdatePrimitive(const Primitive& primitive, size_t index)
{
	switch (primitive.type)
	{
		case PrimitiveType::QUAD:
		{
			Tri triA(primitive.p[0], primitive.p[1], primitive.p[2]);
			triA.texture = primitive.texture;
			UpdateTriangle(triA, index++);
			Tri triB(primitive.p[1], primitive.p[2], primitive.p[3]);
			triB.texture = primitive.texture;
			UpdateTriangle(triB, index++);
		}
		break;
		case PrimitiveType::LINE:
		{
			Tri tri(primitive.p[0], primitive.p[1], primitive.p[1]);
			tri.texture = primitive.texture;
			UpdateTriangle(tri, index++);
		}
		break;
		case PrimitiveType::TRI:
		default:
		{
			Tri tri(primitive.p[0], primitive.p[1], primitive.p[2]);
			tri.texture = primitive.texture;
			UpdateTriangle(tri, index++);
		}
		break;
	}
	return index;
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

	if (m_renderFlags & Mesh::RenderFlags::DrawBackfaces)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
	if (m_renderFlags & Mesh::RenderFlags::DrawWireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth((m_renderFlags & Mesh::RenderFlags::ThickLines) ? 2.5f : 1.0f);
	}
	else
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);
	}
	if (m_renderFlags & Mesh::RenderFlags::ForceDrawOnTop)
	{
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		glEnable(GL_DEPTH_TEST);
	}
	if (m_renderFlags & Mesh::RenderFlags::DrawLinesAA)
	{
		glEnable(GL_LINE_SMOOTH);
	}
	else
	{
		glDisable(GL_LINE_SMOOTH);
	}
	glPointSize(IsRenderingPoints() ? 5.0f : 1.0f);
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
	const GLenum drawMode = IsRenderingPoints() ? GL_POINTS : GL_TRIANGLES;
	if (m_EBO != 0 && m_indexCount > 0)
	{
		glDrawElements(drawMode, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, nullptr);
	}
	else
	{
		glDrawArrays(drawMode, 0, m_vertexCount);
	}
}

void Mesh::Render() const
{
	Bind();
	Draw();
	Unbind();
}

/*
When passing data[], any present data according to the "includedDataFlags" is expected to be in this order:
* vertex/position data (always assumed to be present).
* vcolor
* normal
* uv (if present)
* texindex
*/
void Mesh::UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned renderFlags, unsigned shaderFlags)
{
	includedDataFlags &= VBufDataType::UV;
	const bool hasUV = (includedDataFlags & VBufDataType::UV) != 0;
	const bool reuseBuffers = (m_VAO != 0 && m_VBO != 0 && m_includedData == includedDataFlags);
	if (!reuseBuffers) { Dispose(); }

	const unsigned buffSize = static_cast<unsigned>(data.size() * sizeof(float));
	m_includedData = includedDataFlags;
	m_renderFlags = renderFlags;
	m_shaderFlags = shaderFlags;
	m_vertexCount = 0;

	int ultimateStrideSize = static_cast<int>(GetStrideFloats(includedDataFlags));
	if (ultimateStrideSize > 0)
	{
		m_vertexCount = static_cast<int>(data.size() / ultimateStrideSize);
	}

	if (reuseBuffers)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
		glBufferData(GL_ARRAY_BUFFER, buffSize, data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		UpdateIndexBuffer();
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

	UpdateIndexBuffer();
}

void Mesh::UpdateIndexBuffer()
{
	if (m_highLODIndices.empty())
	{
		if (m_EBO != 0)
		{
			glDeleteBuffers(1, &m_EBO);
			m_EBO = 0;
		}
		m_indexCount = 0;
		return;
	}

	const std::vector<unsigned>& indices = (m_useLowLOD && !m_lowLODIndices.empty()) ? m_lowLODIndices : m_highLODIndices;
	m_indexCount = indices.size();

	if (m_EBO == 0) { glGenBuffers(1, &m_EBO); }

	glBindVertexArray(m_VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)), indices.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Mesh::UpdateTriangle(const Tri& tri, size_t triangleIndex)
{
	if (m_VBO == 0 || triangleIndex >= m_triCount) { return; }

	int texIndex = 0;
	const size_t stride = GetStrideFloats(m_includedData);
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

int Mesh::GetRenderFlags() const
{
	return m_renderFlags;
}

void Mesh::SetRenderFlags(unsigned renderFlags)
{
	m_renderFlags = renderFlags;
}

int Mesh::GetShaderFlags() const
{
	return m_shaderFlags;
}

void Mesh::SetShaderFlags(unsigned shaderFlags)
{
	m_shaderFlags = shaderFlags;
}

void Mesh::SetUseLowLOD(bool useLowLOD)
{
	if (m_useLowLOD == useLowLOD) { return; }
	m_useLowLOD = useLowLOD;
	UpdateIndexBuffer();
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
	m_renderFlags = 0;
	m_shaderFlags = 0;
	m_useLowLOD = false;
	m_indexCount = 0;
	m_highLODIndices.clear();
	m_lowLODIndices.clear();
	m_textureStoreData.clear();
	m_textureStoreIndex.clear();
}

bool Mesh::IsReady() const
{
	return m_VAO != 0 && m_vertexCount > 0;
}

void Mesh::Dispose()
{
	if (m_VAO != 0) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
	if (m_VBO != 0) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
	if (m_EBO != 0) { glDeleteBuffers(1, &m_EBO); m_EBO = 0; }
	m_vertexCount = 0;
	m_indexCount = 0;
}

void Mesh::DeleteTextures()
{
	if (m_textures == 0) { return; }
	glDeleteTextures(1, &m_textures);
	m_textures = 0;
}

bool Mesh::IsRenderingPoints() const
{
	return (m_renderFlags & Mesh::RenderFlags::AllowPointRender) && GuiRenderSettings::showVerts;
}

void Mesh::BuildLowLODIndices(const std::vector<Primitive>& primitives)
{
	unsigned triCount = 0;
	std::vector<unsigned> primitiveBaseVertex;
	primitiveBaseVertex.reserve(primitives.size());
	for (const Primitive& primitive : primitives)
	{
		primitiveBaseVertex.push_back(triCount * 3);
		triCount += (primitive.type == PrimitiveType::QUAD) ? 2 : 1;
	}

	const unsigned vertexCount = triCount * 3;
	m_highLODIndices.resize(vertexCount);
	std::iota(m_highLODIndices.begin(), m_highLODIndices.end(), 0u);
	m_lowLODIndices.clear();
	m_lowLODIndices.reserve(vertexCount / 4);

	constexpr size_t groupSize = 4;
	for (size_t groupStart = 0; groupStart < primitives.size(); groupStart += groupSize)
	{
		const PrimitiveType groupType = primitives[groupStart].type;
		const unsigned base0 = primitiveBaseVertex[groupStart];
		const unsigned base1 = primitiveBaseVertex[groupStart + 1];
		const unsigned base2 = primitiveBaseVertex[groupStart + 2];
		if (groupType == PrimitiveType::QUAD)
		{
			const unsigned base3 = primitiveBaseVertex[groupStart + 3];
			const unsigned v0 = base0 + 0; const unsigned v1 = base1 + 1;
			const unsigned v2 = base2 + 2; const unsigned v3 = base3 + 5;
			m_lowLODIndices.push_back(v0);
			m_lowLODIndices.push_back(v1);
			m_lowLODIndices.push_back(v2);
			m_lowLODIndices.push_back(v1);
			m_lowLODIndices.push_back(v3);
			m_lowLODIndices.push_back(v2);
		}
		else
		{
			m_lowLODIndices.push_back(base0 + 0);
			m_lowLODIndices.push_back(base1 + 1);
			m_lowLODIndices.push_back(base2 + 2);
		}
	}
}
