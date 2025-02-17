#include "gui_render_settings.h"

float GuiRenderSettings::camFovDeg = 70.f, GuiRenderSettings::camSprintMult = 2.f, GuiRenderSettings::camRotateMult = 1.f, GuiRenderSettings::camMoveMult = 1.f;
bool GuiRenderSettings::showLowLOD = false, GuiRenderSettings::showWireframe = false, GuiRenderSettings::showLevVerts = false, GuiRenderSettings::showBackfaces = true, GuiRenderSettings::showBspRectTree = false, GuiRenderSettings::showLevel = true, GuiRenderSettings::showCheckpoints = false, GuiRenderSettings::showStartpoints = false;
int GuiRenderSettings::renderType = 0, GuiRenderSettings::bspTreeTopDepth = 0, GuiRenderSettings::bspTreeBottomDepth = 0, GuiRenderSettings::bspTreeMaxDepth = 0;
const char* GuiRenderSettings::renderTypeLabels[] = { "VColor", "Diffuse", "(.obj Normals) Face Direction", "(calc Normals) Face Direction", "World Normals", "(abs) World Normals"};