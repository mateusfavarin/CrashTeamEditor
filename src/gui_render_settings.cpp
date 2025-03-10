#include "gui_render_settings.h"

float GuiRenderSettings::camFovDeg = 70.f;
float GuiRenderSettings::camSprintMult = 2.0f;
float GuiRenderSettings::camRotateMult = 1.0f;
float GuiRenderSettings::camMoveMult = 1.0f;
bool GuiRenderSettings::showLowLOD = false;
bool GuiRenderSettings::showWireframe = false;
bool GuiRenderSettings::showLevVerts = false;
bool GuiRenderSettings::showBackfaces = true;
int GuiRenderSettings::renderType = 0;
const std::vector<const char*> GuiRenderSettings::renderTypeLabels = { "VColor", "Diffuse", ".obj Normals" };