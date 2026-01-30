#pragma once

#include "renderer.h"

#include <string>

class Level;

namespace Script
{
	std::string ExecutePythonScript(Level& level, Renderer& renderer, const std::string& script);
}
