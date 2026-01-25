#pragma once

#include <string>
#include <map>
#include <utility>

#include "mesh.h"

struct ShaderTemplates
{
  static std::string vert_vcolornormaltex;
  static std::string vert_vcolornormal;

  static std::string frag_vcolornormaltex;
  static std::string frag_vcolornormal;

  static std::map<int, std::pair<std::string, std::string>> datasToShaderSourceMap;
};
