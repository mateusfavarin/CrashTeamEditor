#pragma once

//resources I found helpful when developing this:
//https://www.codingwiththomas.com/blog/rendering-an-opengl-framebuffer-into-a-dear-imgui-window
//https://github.com/ThoSe1990/opengl_imgui/blob/main/src/main.cpp
//https://github.com/JoeyDeVries/LearnOpenGL/blob/master/src/4.advanced_opengl/5.2.framebuffers_exercise1/framebuffers_exercise1.cpp
//https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#example-for-opengl-users
//chatgpt
//https://learnopengl.com/Getting-started/Hello-Triangle

#include "globalimguiglglfw.h"
#include "shader.h"
#include "mesh.h"
#include "model.h"
#include "camera.h"
#include "geo.h"
#include "lev.h"

#include <list>
#include <array>
#include <unordered_map>

class Renderer
{
public:
	Renderer(int width, int height);
	~Renderer();
	Model* CreateModel();
	bool DeleteModel(Model* model);

	void RescaleFramebuffer(float width, float height);
	void Render(bool skyGradientEnabled, const std::array<ColorGradient, NUM_GRADIENT>& skyGradients);
	void SetViewportSize(float width, float height);
	void SetCameraToLevelSpawn(const Vec3& pos, const Vec3& rot);
	float GetLastDeltaTime() const;
	float GetLastTime() const;
	float GetWidth() const;
	float GetHeight() const;
	GLuint GetTexBuffer() const;
	std::tuple<glm::vec3, float> WorldspaceRayTriIntersection(glm::vec3 worldSpaceRay, const glm::vec3 tri[3]) const;
	glm::vec3 ScreenspaceToWorldRay(int pixelX, int pixelY) const;

private:
	void RenderSkyGradient(const std::array<ColorGradient, NUM_GRADIENT>& skyGradients);

private:
	int m_width;
	int m_height;
	GLuint m_texturebuffer;
	GLuint m_renderbuffer;
	GLuint m_framebuffer;
	std::unordered_map<int, Shader> m_shaderCache;
	float m_time = 0.0f;
	float m_lastFrameTime = 0.0f;
	float m_deltaTime = -1.0f;
	glm::mat4 m_perspective;
	Camera m_camera;
	bool m_skyGradientEnabled = false;
	std::array<ColorGradient, NUM_GRADIENT> m_skyGradients;
	std::list<Model*> m_modelList;
};
