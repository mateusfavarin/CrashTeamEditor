#pragma once

#include "geo.h"
#include "text3d.h"

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
		static constexpr unsigned AllowPointRender = 1 << 6;
		static constexpr unsigned DrawPoints = 1 << 7;
		static constexpr unsigned QuadblockLod = 1 << 8;
		static constexpr unsigned FollowCamera = 1 << 9;
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
	void SetGeometry(const std::vector<Primitive>& primitives, unsigned renderFlags = RenderFlags::None, unsigned shaderFlags = ShaderFlags::None);
	void SetGeometry(const std::string& label, Text3D::Align align, const Color& color, float scaleMult = 1.0f);
	size_t UpdatePrimitive(const Primitive& primitive, size_t index);
	int GetRenderFlags() const;
	void SetRenderFlags(unsigned renderFlags);
	int GetShaderFlags() const;
	void SetShaderFlags(unsigned shaderFlags);
	void Clear();
	bool IsReady() const;

private:
	void UpdateTriangle(const Tri& tri, size_t triangleIndex);
	void SetUseLowLOD(bool useLowLOD);
	int GetDatas() const;
	GLuint GetTextureStore() const;
	void UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned renderFlags, unsigned shaderFlags);
	void UpdateIndexBuffer();
	void AppendTextureStore(const std::filesystem::path& texturePath);
	std::vector<unsigned char> LoadTextureData(const std::filesystem::path& path);
	void RebuildTextureData();
	void Bind() const;
	void Unbind() const;
	void Draw() const;
	void Render() const;
	void Dispose();
	void DeleteTextures();
	bool IsRenderingPoints() const;
	void BuildLowLODIndices(const std::vector<Primitive>& primitives);

private:
	GLuint m_VAO = 0;
	GLuint m_VBO = 0;
	GLuint m_EBO = 0;
	GLuint m_textures = 0;
	int m_vertexCount = 0;
	size_t m_indexCount = 0;
	unsigned m_includedData = 0;
	unsigned m_renderFlags = 0;
	unsigned m_shaderFlags = 0;
	bool m_useLowLOD = false;
	size_t m_triCount = 0;
	std::vector<unsigned> m_highLODIndices;
	std::vector<unsigned> m_lowLODIndices;
	std::vector<std::vector<unsigned char>> m_textureStoreData;
	std::unordered_map<std::filesystem::path, size_t> m_textureStoreIndex;
	friend class Model;
	friend class Renderer;
};
