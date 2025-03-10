#pragma once

#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"

class Mesh
{
public:
	struct ShaderSettings //note: updating this also requires updating all shader source.
	{
		static constexpr unsigned None = 0;
		static constexpr unsigned DrawWireframe = 1;
		static constexpr unsigned DrawBackfaces = 4;
	};

	struct VBufDataType
	{ //all are assumed to have vertex data (otherwise, what the hell are we drawing?)
		static constexpr unsigned VertexPos = 1; //implicitly always on
		static constexpr unsigned Barycentric = 2;
		static constexpr unsigned VColor = 4;
		static constexpr unsigned Normals = 8;
		static constexpr unsigned STUV_1 = 16; //tex coords
	};

public:
	Mesh() {};
	void UpdateMesh(const std::vector<float>& data, unsigned includedDataFlags, unsigned shadSettings, bool dataIsInterlaced = true);
	int GetDatas() const;
	int GetShaderSettings() const;
	void SetShaderSettings(unsigned shadSettings);

private:
	void Bind() const;
	void Unbind() const;
	void Draw() const;
	void Dispose();

private:
	GLuint m_VAO = 0;
	GLuint m_VBO = 0;
	int m_dataBufSize = 0;
	unsigned m_includedData = 0;
	unsigned m_shaderSettings = 0;

	friend class Model;
};