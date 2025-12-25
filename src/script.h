#pragma once

#include <string>

class Level;

namespace Script
{
	std::string ExecutePythonScript(Level& level, const std::string& script);
}
