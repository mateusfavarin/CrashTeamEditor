#pragma once

#include <vector>

struct GuiRenderSettings
{
	// note: updating this also requires updating all shader source.
  enum RenderType
	{
    Default,
    Texture,
    VertexColor,
    Normals,
  };

  static int renderType, bspTreeTopDepth, bspTreeBottomDepth, bspTreeMaxDepth;
  static float camFovDeg, camSprintMult, camRotateMult, camMoveMult;
  static bool showLowLOD, showWireframe, showLevVerts, showBackfaces, showBspRectTree, showLevel, showCheckpoints, showStartpoints, showVisTree;
  static const std::vector<const char*> renderTypeLabels;
};
