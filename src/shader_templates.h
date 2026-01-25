#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include "mesh.h"

struct ShaderTemplates
{
  static std::string vert_vcolornormaltex;
  static std::string vert_vcolornormal;

  static std::string frag_vcolornormaltex;
  static std::string frag_vcolornormal;

  static std::string vert_skygradient;
  static std::string frag_skygradient;

  static std::unordered_map<int, std::pair<std::string, std::string>> datasToShaderSourceMap;
};
