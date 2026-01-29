#pragma once

#include "geo.h"

#include <string>
#include <vector>

namespace Text3D
{
	enum class Align
	{
		LEFT,
		CENTER,
		RIGHT
	};

	std::vector<Primitive> ToGeometry(const std::string& label, Align align, const Color& color);
}
