#include "gui_render_settings.h"

float GuiRenderSettings::camFovDeg = 70.0f;
float GuiRenderSettings::camZoomMult = 1.0f;
float GuiRenderSettings::camRotateMult = 1.5f;
float GuiRenderSettings::camMoveMult = 2.5f;
float GuiRenderSettings::camSprintMult = 3.0f;
bool GuiRenderSettings::showLowLOD = false;
bool GuiRenderSettings::showWireframe = false;
bool GuiRenderSettings::showLevVerts = false;
bool GuiRenderSettings::showBackfaces = true;
bool GuiRenderSettings::showBspRectTree = false;
bool GuiRenderSettings::showLevel = true;
bool GuiRenderSettings::showCheckpoints = false;
bool GuiRenderSettings::showStartpoints = false;
bool GuiRenderSettings::showVisTree = false;
int GuiRenderSettings::renderType = 0;
int GuiRenderSettings::bspTreeTopDepth = 0;
int GuiRenderSettings::bspTreeBottomDepth = 0;
int GuiRenderSettings::bspTreeMaxDepth = 0;
const std::vector<const char*> GuiRenderSettings::renderTypeLabels = { "Default", "Texture", "Vertex Color", "Normals"};
