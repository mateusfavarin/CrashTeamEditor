#pragma once

#include "globalimguiglglfw.h"

class Renderer 
{
private:
  GLuint VAO, VBO;
  void CopySysNow(void);
public:
  GLuint shaderProgram;
  GLuint renderbuffer;
  GLuint framebuffer;
  GLuint texturebuffer;
  unsigned char* buf;
  int width, height;
  Renderer(int width, int height);
  ~Renderer();
  void Render(void);
};