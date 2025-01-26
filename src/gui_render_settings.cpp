#include "gui_render_settings.h"

float GuiRenderSettings::camFovDeg = 70.f, GuiRenderSettings::camSprintMult = 2.f, GuiRenderSettings::camRotateMult = 1.f, GuiRenderSettings::camMoveMult = 1.f;
bool GuiRenderSettings::showLowLOD = false, GuiRenderSettings::showWireframe = false, GuiRenderSettings::showLevVerts = false, GuiRenderSettings::showBackfaces = true;
int GuiRenderSettings::renderType = 0;
const char* GuiRenderSettings::renderTypeLabels[] = { "VColor", "Diffuse", "Normals", "Don't Draw Primary Mesh"};