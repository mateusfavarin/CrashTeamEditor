#pragma once

#include "globalimguiglglfw.h"

class Renderer 
{
private:
  GLuint VAO, VBO;
  GLuint shaderProgram;
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