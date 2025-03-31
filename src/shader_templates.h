#pragma once

#include <string>
#include <map>
#include <tuple>

#include "mesh.h"

struct ShaderTemplates
{
  static std::string geom_vcolornormaltex;
  static std::string geom_vcolornormal;
  static std::string geom_vcolor;
  static std::string geom_normal;
  static std::string geom_; //shader designed for "only required" fields, i.e., vertex positions

  static std::string vert_vcolornormaltex;
  static std::string vert_vcolornormal;
  static std::string vert_vcolor;
  static std::string vert_normal;
  static std::string vert_; //shader designed for "only required" fields, i.e., vertex positions

  static std::string frag_vcolornormaltex;
  static std::string frag_vcolornormal;
  static std::string frag_vcolor;
  static std::string frag_normal;
  static std::string frag_; //shader designed for "only required" fields, i.e., vertex positions

  static std::map<int, std::tuple<std::string, std::string, std::string>> datasToShaderSourceMap;
};