#pragma once

#include "globalimguiglglfw.h"

class Renderer 
{
private:
  GLuint framebuffer;
  GLuint textureColorbuffer;
  GLuint rbo;
  GLuint shaderProgram;
  GLuint VAO, VBO;
  void CopySysNow(void);
public:
  char* buf;
  int width, height;
  Renderer(int width, int height);
  ~Renderer();
  void Render(void);
};