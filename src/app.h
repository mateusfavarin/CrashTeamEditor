#pragma once

#include <string>
#include "globalimguiglglfw.h"

class App
{
public:
	bool Init();
	void Run();
	void Close();

private:
	bool InitGLFW();
	bool InitImGui();
	void CloseImGui();

private:
	std::string m_glslVer;
	GLFWwindow* m_window;
	const std::string m_version = "BETA";
};