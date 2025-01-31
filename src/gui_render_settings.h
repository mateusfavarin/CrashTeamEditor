#pragma once

struct GuiRenderSettings
{
  static float camFovDeg, camSprintMult, camRotateMult, camMoveMult;
  static bool showLowLOD, showWireframe, showLevVerts, showBackfaces;
  static int renderType;
  enum RenderType { //note: updating this also requires updating all shader source.
    VColor,
    Diffuse,
    Normals
  };
  static const char* renderTypeLabels[];
};