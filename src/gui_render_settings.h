#pragma once

struct GuiRenderSettings
{
  static float camFovDeg, camSprintMult, camRotateMult, camMoveMult;
  static bool showLowLOD, showWireframe, showLevVerts, showBackfaces, showBspRectTree, showLevel, showCheckpoints, showStartpoints;
  static int renderType, bspTreeTopDepth, bspTreeBottomDepth, bspTreeMaxDepth;
  enum RenderType { //note: updating this also requires updating all shader source.
    VColor,
    Diffuse,
    ObjFaceDirection,
    CalcFaceDirection,
    WorldNormals,
  };
  static const char* renderTypeLabels[];
};