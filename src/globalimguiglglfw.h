#pragma once

#ifndef OPENGLHEADERS_H
//#define OPENGLHEADERS_H

//imgui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "manual_third_party/glad.h"

#endif // OPENGLHEADERS_H