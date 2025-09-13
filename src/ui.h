#pragma once

#include "level.h"

struct Windows
{
	static bool w_spawn;
	static bool w_level;
	static bool w_material;
	static bool w_animtex;
	static bool w_quadblocks;
	static bool w_checkpoints;
	static bool w_bsp;
	static bool w_renderer;
	static bool w_ghost;
};

class UI
{
public:
	void Render(int width, int height);
private:
	void MainMenu();

private:
	Level m_lev;
};