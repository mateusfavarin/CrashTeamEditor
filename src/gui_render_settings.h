#pragma once

struct GuiRenderSettings
{
  static float camFovDeg, camSprintMult, camRotateMult, camMoveMult;
  static bool showLowLOD, showWireframe, showLevVerts, showBackfaces;
  static int renderType;
  enum RenderType { //note: updating this also requires updating all shader source.
    VColor,
    Diffuse,
    Normals,
    DontDraw //specifically don't draw "faces", but still draw wireframe (if enabled) or verts (if enabled) etc.
  };
  static const char* renderTypeLabels[];
};