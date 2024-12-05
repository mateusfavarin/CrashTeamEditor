#pragma once

#include "level.h"

class UI
{
public:
	void Render(int width, int height);
private:
	void MainMenu();

private:
	Level m_lev;
};