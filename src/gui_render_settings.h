#pragma once

#include "geo.h"

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
  static float camFovDeg, camZoomMult, camRotateMult, camMoveMult, camSprintMult;
  static int camKeyForward, camKeyBack, camKeyLeft, camKeyRight, camKeyUp, camKeyDown, camKeySprint;
  static int camOrbitMouseButton;
  static bool showLowLOD, showWireframe, showLevVerts, showBackfaces, showBspRectTree, showLevel, showCheckpoints, showStartpoints, showVisTree, filterActive, showSelectedQuadblockInfo, showMinimapBounds;
  static Color defaultFilterColor, selectedCheckpointColor;
  static const std::vector<const char*> renderTypeLabels;
};
