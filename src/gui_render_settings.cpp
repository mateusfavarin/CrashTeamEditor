#include "gui_render_settings.h"
#include <imgui.h>

float GuiRenderSettings::camFovDeg = 70.0f;
float GuiRenderSettings::camZoomMult = 1.0f;
float GuiRenderSettings::camRotateMult = 1.5f;
float GuiRenderSettings::camMoveMult = 2.5f;
float GuiRenderSettings::camSprintMult = 3.0f;
int GuiRenderSettings::camKeyForward = ImGuiKey_W;
int GuiRenderSettings::camKeyBack = ImGuiKey_S;
int GuiRenderSettings::camKeyLeft = ImGuiKey_A;
int GuiRenderSettings::camKeyRight = ImGuiKey_D;
int GuiRenderSettings::camKeyUp = ImGuiKey_E;
int GuiRenderSettings::camKeyDown = ImGuiKey_Q;
int GuiRenderSettings::camKeySprint = ImGuiKey_ModShift;
int GuiRenderSettings::camOrbitMouseButton = ImGuiMouseButton_Middle;
bool GuiRenderSettings::showLowLOD = false;
bool GuiRenderSettings::showWireframe = false;
bool GuiRenderSettings::showLevVerts = false;
bool GuiRenderSettings::showBackfaces = true;
bool GuiRenderSettings::showBspRectTree = false;
bool GuiRenderSettings::showLevel = true;
bool GuiRenderSettings::showCheckpoints = false;
bool GuiRenderSettings::showStartpoints = false;
bool GuiRenderSettings::showVisTree = false;
bool GuiRenderSettings::filterActive = true;
bool GuiRenderSettings::showSelectedQuadblockInfo = true;
bool GuiRenderSettings::showMinimapBounds = false;
Color GuiRenderSettings::defaultFilterColor = Color(static_cast<unsigned char>(255), static_cast<unsigned char>(128), static_cast<unsigned char>(0));
Color GuiRenderSettings::selectedCheckpointColor = Color(static_cast<unsigned char>(0), static_cast < unsigned char>(255), static_cast < unsigned char>(255));
int GuiRenderSettings::renderType = 0;
int GuiRenderSettings::bspTreeTopDepth = 0;
int GuiRenderSettings::bspTreeBottomDepth = 0;
int GuiRenderSettings::bspTreeMaxDepth = 0;
const std::vector<const char*> GuiRenderSettings::renderTypeLabels = { "Default", "Texture", "Vertex Color", "Normals"};
