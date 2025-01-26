#pragma once

#include <string>
#include <map>
#include <tuple>

#include "mesh.h"

struct ShaderTemplates
{
  static std::string vert_vcolornormal;
  static std::string vert_vcolor;
  static std::string vert_normal;
  static std::string vert_; //shader designed for "only required" fields, i.e., vertex positions & barycentric

  static std::string frag_vcolornormal;
  static std::string frag_vcolor;
  static std::string frag_normal;
  static std::string frag_; //shader designed for "only required" fields, i.e., vertex positions & barycentric

  static std::map<Mesh::VBufDataType, std::tuple<std::string, std::string>> datasToShaderSourceMap;
};