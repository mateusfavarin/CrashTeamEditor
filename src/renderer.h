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

class Renderer 
{
private:
  GLuint VAO, VBO;
  //GLuint shaderProgram;
  Shader shader;
  GLuint renderbuffer;
  GLuint framebuffer;
public:
  GLuint texturebuffer;
  int width, height;
  Renderer(int width, int height);
  void RescaleFramebuffer(float width, float height);
  ~Renderer();
  void Render(void);
};