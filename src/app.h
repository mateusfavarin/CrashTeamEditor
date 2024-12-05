#pragma once

#include <SDL.h>

#include <string>

class App
{
public:
	bool Init();
	void Run();
	void Close();
#ifdef _DEBUG
	void RunImGuiExample();
#endif

private:
	bool InitSDL();
	bool InitImGui();
	void CloseSDL();
	void CloseImGui();

private:
	std::string m_glslVer;
	SDL_GLContext m_glContext;
	SDL_Window* m_window;
	const std::string m_version = "BETA";
};