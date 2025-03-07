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
  Renderer(int width, int height);
  void RescaleFramebuffer(float width, float height);
  void Render(std::vector<Model> models);
  float GetLastDeltaTime(void) const;
  float GetLastTime(void) const;

public:
	GLuint m_texturebuffer;
	int m_width;
	int m_height;

private:
	GLuint m_renderbuffer;
	GLuint m_framebuffer;
	std::map<int, Shader> m_shaderCache;
	float m_time = 0.0f;
	float m_lastFrameTime = 0.0f;
	float m_deltaTime = -1.0f;
};