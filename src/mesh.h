#pragma once

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
	struct ShaderSettings //note: updating this also requires updating all shader source.
	{
		static constexpr unsigned None = 0;
		static constexpr unsigned DrawWireframe = 1 << 0;
		static constexpr unsigned ForceDrawOnTop = 1 << 1;
		static constexpr unsigned DrawBackfaces = 1 << 2;
		static constexpr unsigned DrawLinesAA = 1 << 3;
		static constexpr unsigned DontOverrideShaderSettings = 1 << 4;
		static constexpr unsigned Blinky = 1 << 5;
		static constexpr unsigned ThickLines = 1 << 6;
		static constexpr unsigned DiscardZeroColor = 1 << 7;
	};

	struct VBufDataType
	{
		static constexpr unsigned Position = 1 << 0;
		static constexpr unsigned Color = 1 << 1;
		static constexpr unsigned Normal = 1 << 2;
		static constexpr unsigned UV = 1 << 3; //tex coords
		static constexpr unsigned TexIndex = 1 << 4;
	};

public:
	Mesh();
	void UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned shadSettings);
	void UpdatePoint(const Vertex& vert, size_t vertexIndex);
	void UpdateOctoPoint(const Vertex& vert, size_t baseVertexIndex);
	void UpdateUV(const Vec2& uv, int textureIndex, size_t vertexIndex);
	int GetDatas() const;
	int GetShaderSettings() const;
	void SetShaderSettings(unsigned shadSettings);
	void UpdateTextureStore(const std::filesystem::path& texturePath);
	GLuint GetTextureStore() const;
	void Clear();

private:
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
	unsigned m_shaderSettings = 0;
	std::vector<std::vector<unsigned char>> m_textureStoreData;
	std::unordered_map<std::filesystem::path, size_t> m_textureStoreIndex;
	friend class Model;
};
