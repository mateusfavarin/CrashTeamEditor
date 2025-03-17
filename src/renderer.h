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
#include <vector>
#include <map>

class Renderer
{
public:
  Renderer(float width, float height);
  void RescaleFramebuffer(float width, float height);
  void Render(std::vector<Model> models);
  float GetLastDeltaTime() const;
  float GetLastTime() const;
	float GetWidth() const;
	float GetHeight() const;
	GLuint GetTexBuffer() const;
	/// <summary>
	/// Does a ray/triangle intersection test. It is not sufficient for the ray and triangle to be in the same
	/// coordinate space, the ray and triangle must be in worldspace.
	/// </summary>
	/// <param name="worldSpaceRay">Ray direction. Origin point is implicitly the camera's worldspace position.</param>
	/// <param name="tri">Worldspace positions for triangle</param>
	/// <returns>(Query Result Position (or vec3::0 if failed), time interpolant value for ray (or -1.0f if failed))</returns>
	std::tuple<glm::vec3, float> WorldspaceRayTriIntersection(glm::vec3 worldSpaceRay, const glm::vec3 tri[3]) const;
	/// <summary>
	/// Converts a screenspace (pixel X & Y) to a worldspace ray emitted from the camera.
	/// </summary>
	/// <param name="pixelX">pixel X coordinate, domain is [0, screenWidth)</param>
	/// <param name="pixelY">pixel Y coordinate, domain is [0, screenHeight)</param>
	glm::vec3 ScreenspaceToWorldRay(int pixelX, int pixelY) const;

private:
	int m_width;
	int m_height;
	GLuint m_texturebuffer;
	GLuint m_renderbuffer;
	GLuint m_framebuffer;
	std::map<int, Shader> m_shaderCache;
	float m_time = 0.0f;
	float m_lastFrameTime = 0.0f;
	float m_deltaTime = -1.0f;
	glm::mat4 m_cameraView;
	glm::mat4 m_perspective;
	glm::vec3 m_camWorldPos = glm::vec3(0.f, 0.f, 3.f);
};