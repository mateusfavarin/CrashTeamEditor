#pragma once

#include "globalimguiglglfw.h"

class Renderer 
{
private:
  GLuint rbo;
  GLuint shaderProgram;
  GLuint VAO, VBO;
  void CopySysNow(void);
public:
  GLuint framebuffer;
  GLuint textureColorbuffer;
  unsigned char* buf;
  int width, height;
  Renderer(int width, int height);
  ~Renderer();
  void Render(void);
};