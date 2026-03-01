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
#include <GLFW/glfw3.h> // normally would drag system OpenGL headers, but not with GLFW_INCLUDE_NONE
#include "manual_third_party/glad.h"

#ifndef GL_CHECK
#if defined(_DEBUG) || !defined(NDEBUG)
inline void GlCheckError(const char* stmt, const char* file, int line)
{
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) { fprintf(stderr, "GL error 0x%X after %s (%s:%d)\n", err, stmt, file, line); }
}

template <typename T>
inline T GlCheckErrorRet(T value, const char* stmt, const char* file, int line)
{
	GlCheckError(stmt, file, line);
	return value;
}

#define GL_CHECK(stmt) do { stmt; GlCheckError(#stmt, __FILE__, __LINE__); } while (0)
#define GL_CHECK_RET(stmt) GlCheckErrorRet((stmt), #stmt, __FILE__, __LINE__)
#else
#define GL_CHECK(stmt) stmt
#define GL_CHECK_RET(stmt) stmt
#endif
#endif

#endif // OPENGLHEADERS_H
