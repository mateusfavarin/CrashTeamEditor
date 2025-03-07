#pragma once

#include <vector>

struct GuiRenderSettings
{
	// note: updating this also requires updating all shader source.
  enum RenderType
	{
    VColor,
    Diffuse,
    Normals
  };

  static int renderType;
  static float camFovDeg, camSprintMult, camRotateMult, camMoveMult;
  static bool showLowLOD, showWireframe, showLevVerts, showBackfaces;
  static const std::vector<const char*> renderTypeLabels;
};