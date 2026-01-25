#pragma once

#include "geo.h"

#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"

#include <filesystem>
#include <unordered_map>
#include <vector>

class Vertex;
struct Vec2;

class Mesh
{
public:
	struct RenderFlags
	{
		static constexpr unsigned None = 0;
		static constexpr unsigned DrawWireframe = 1 << 0;
		static constexpr unsigned ForceDrawOnTop = 1 << 1;
		static constexpr unsigned DrawBackfaces = 1 << 2;
		static constexpr unsigned DrawLinesAA = 1 << 3;
		static constexpr unsigned DontOverrideRenderFlags = 1 << 4;
		static constexpr unsigned ThickLines = 1 << 5;
	};

	struct ShaderFlags // note: updating this also requires updating all shader source.
	{
		static constexpr unsigned None = 0;
		static constexpr unsigned Blinky = 1 << 0;
		static constexpr unsigned DiscardZeroColor = 1 << 1;
	};

	struct VBufDataType
	{
		static constexpr unsigned None = 0;
		static constexpr unsigned UV = 1 << 0;
	};

public:
	Mesh();
	void SetGeometry(const std::vector<Tri>& triangles, unsigned renderFlags = RenderFlags::None, unsigned shaderFlags = ShaderFlags::None);
	void UpdateTriangle(const Tri& tri, size_t triangleIndex);
	int GetDatas() const;
	int GetRenderFlags() const;
	void SetRenderFlags(unsigned renderFlags);
	int GetShaderFlags() const;
	void SetShaderFlags(unsigned shaderFlags);
	void UpdateTextureStore(const std::filesystem::path& texturePath);
	GLuint GetTextureStore() const;
	void Clear();

private:
	void UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned renderFlags, unsigned shaderFlags);
	void AppendTextureStore(const std::filesystem::path& texturePath);
	std::vector<unsigned char> LoadTextureData(const std::filesystem::path& path);
	void RebuildTextureData();
	void Bind() const;
	void Unbind() const;
	void Draw() const;
	void Dispose();
	void DeleteTextures();

private:
	GLuint m_VAO = 0;
	GLuint m_VBO = 0;
	GLuint m_textures = 0;
	int m_vertexCount = 0;
	unsigned m_includedData = 0;
	unsigned m_renderFlags = 0;
	unsigned m_shaderFlags = 0;
	size_t m_triCount = 0;
	std::vector<std::vector<unsigned char>> m_textureStoreData;
	std::unordered_map<std::filesystem::path, size_t> m_textureStoreIndex;
	friend class Model;
};
