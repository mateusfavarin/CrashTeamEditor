#pragma once

#include <string>
#include <filesystem>
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
	void InitUISettings();
	void SaveUISettings(bool useDefault);
	void CloseImGui();

private:
	std::string m_glslVer;
	GLFWwindow* m_window;
	const std::filesystem::path m_configFile = "ui.json";
	const std::string m_version = "BETA";
};