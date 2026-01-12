#pragma once

#include "level.h"

#include <string>

struct Windows
{
	static int w_width;
	static int w_height;
	static bool w_spawn;
	static bool w_level;
	static bool w_material;
	static bool w_animtex;
	static bool w_quadblocks;
	static bool w_checkpoints;
	static bool w_bsp;
	static bool w_renderer;
	static bool w_ghost;
	static bool w_python;
	static std::string lastOpenedFolder;
};

class UI
{
public:
	UI();
	void Render(int width, int height);
private:
	void MainMenu();
	void RenderWorld();

private:
	Level m_lev;
	Renderer m_rend;
	ImGuiIO& m_io;
};
