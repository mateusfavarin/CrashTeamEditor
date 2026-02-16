#pragma once

#include "renderer.h"

#include <string>

class Level;

namespace Script
{
	std::string ExecutePythonScript(Level& level, Renderer& renderer, const std::string& script);
	bool AppendPythonPath(const std::filesystem::path& path, std::string& error);
}
