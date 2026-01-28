#include "minimap.h"
#include "quadblock.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <portable-file-dialogs.h>
#include <algorithm>
#include <limits>

void MinimapConfig::CalculateWorldBoundsFromQuadblocks(const std::vector<Quadblock>& quadblocks)
{
	if (quadblocks.empty()) { return; }

	float minX = std::numeric_limits<float>::max();
	float minZ = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float maxZ = std::numeric_limits<float>::lowest();

    bool found = false;
    for (const Quadblock& qb : quadblocks)
    {
        // Only include quadblocks that have a checkpoint assigned
        if (qb.GetCheckpoint() >= 0)
        {
            const BoundingBox& bbox = qb.GetBoundingBox();
            minX = std::min(minX, bbox.min.x);
            minZ = std::min(minZ, bbox.min.z);
            maxX = std::max(maxX, bbox.max.x);
            maxZ = std::max(maxZ, bbox.max.z);
            found = true;
        }
    }

	// Convert to fixed-point coordinates (FP_ONE_GEO = 64)
	worldStartX = static_cast<int16_t>(minX * FP_ONE_GEO);
	worldStartY = static_cast<int16_t>(minZ * FP_ONE_GEO);
	worldEndX = static_cast<int16_t>(maxX * FP_ONE_GEO);
	worldEndY = static_cast<int16_t>(maxZ * FP_ONE_GEO);
}

PSX::Map MinimapConfig::Serialize() const
{
	PSX::Map map = {};
	map.worldEndX = worldEndX;
	map.worldEndY = worldEndY;
	map.worldStartX = worldStartX;
	map.worldStartY = worldStartY;
	// Icon size is the texture dimensions
	map.iconSizeX = hasTopTexture ? static_cast<int16_t>(topTexture.GetWidth()) : iconSizeX;
	map.iconSizeY = hasTopTexture ? static_cast<int16_t>(topTexture.GetHeight()) : iconSizeY;
	map.driverDotStartX = driverDotStartX;
	map.driverDotStartY = driverDotStartY;
	map.mode = orientationMode;
	map.unk = unk;
	return map;
}

void MinimapConfig::Deserialize(const PSX::Map& map)
{
	worldEndX = map.worldEndX;
	worldEndY = map.worldEndY;
	worldStartX = map.worldStartX;
	worldStartY = map.worldStartY;
	iconSizeX = map.iconSizeX;
	iconSizeY = map.iconSizeY;
	driverDotStartX = map.driverDotStartX;
	driverDotStartY = map.driverDotStartY;
	orientationMode = map.mode;
	unk = map.unk;
}

void MinimapConfig::LoadTextures()
{
	if (!topTexturePath.empty() && std::filesystem::exists(topTexturePath))
	{
		topTexture = Texture(topTexturePath);
		hasTopTexture = !topTexture.IsEmpty();
	}
	if (!bottomTexturePath.empty() && std::filesystem::exists(bottomTexturePath))
	{
		bottomTexture = Texture(bottomTexturePath);
		hasBottomTexture = !bottomTexture.IsEmpty();
	}
}

bool MinimapConfig::IsReady() const
{
	return enabled && hasTopTexture && hasBottomTexture;
}

std::vector<Texture*> MinimapConfig::GetTextures()
{
	std::vector<Texture*> textures;
	if (hasTopTexture) { textures.push_back(&topTexture); }
	if (hasBottomTexture) { textures.push_back(&bottomTexture); }
	return textures;
}

void MinimapConfig::Clear()
{
	worldEndX = 0;
	worldEndY = 0;
	worldStartX = 0;
	worldStartY = 0;
	iconSizeX = 0;
	iconSizeY = 0;
	driverDotStartX = 450;
	driverDotStartY = 180;
	orientationMode = 0;
	unk = 0;
	topTexturePath.clear();
	bottomTexturePath.clear();
	topTexture = Texture();
	bottomTexture = Texture();
	hasTopTexture = false;
	hasBottomTexture = false;
	enabled = false;
}

bool MinimapConfig::RenderUI(const std::vector<Quadblock>& quadblocks)
{
	bool boundsChanged = false;
	
	ImGui::Checkbox("Enable Minimap", &enabled);

	if (!enabled) { return false; }

	ImGui::Separator();
	ImGui::Text("World Bounds:");
	
	// Convert fixed-point to float for display (divide by 64)
	float startX = static_cast<float>(worldStartX) / static_cast<float>(FP_ONE_GEO);
	float startY = static_cast<float>(worldStartY) / static_cast<float>(FP_ONE_GEO);
	float endX = static_cast<float>(worldEndX) / static_cast<float>(FP_ONE_GEO);
	float endY = static_cast<float>(worldEndY) / static_cast<float>(FP_ONE_GEO);
	
	if (ImGui::InputFloat("World Start X", &startX, 1.0f, 10.0f, "%.2f"))
	{
		worldStartX = static_cast<int16_t>(startX * static_cast<float>(FP_ONE_GEO));
		boundsChanged = true;
	}
	if (ImGui::InputFloat("World Start Y", &startY, 1.0f, 10.0f, "%.2f"))
	{
		worldStartY = static_cast<int16_t>(startY * static_cast<float>(FP_ONE_GEO));
		boundsChanged = true;
	}
	if (ImGui::InputFloat("World End X", &endX, 1.0f, 10.0f, "%.2f"))
	{
		worldEndX = static_cast<int16_t>(endX * static_cast<float>(FP_ONE_GEO));
		boundsChanged = true;
	}
	if (ImGui::InputFloat("World End Y", &endY, 1.0f, 10.0f, "%.2f"))
	{
		worldEndY = static_cast<int16_t>(endY * static_cast<float>(FP_ONE_GEO));
		boundsChanged = true;
	}

	if (ImGui::Button("Calculate from Quadblocks"))
	{
		CalculateWorldBoundsFromQuadblocks(quadblocks);
		boundsChanged = true;
	}
	ImGui::SetItemTooltip("(Experimental) Automatically calculate world bounds from quadblocks with checkpoints");

	ImGui::Separator();
	ImGui::Text("Driver Icon Start Position (screen %d x %d):", PSX::SCREEN_WIDTH, PSX::SCREEN_HEIGHT);
	if (ImGui::InputScalar("Icon Start X", ImGuiDataType_S16, &driverDotStartX)) {
		if (driverDotStartX < 0) driverDotStartX = 0;
		if (driverDotStartX > PSX::SCREEN_WIDTH) driverDotStartX = PSX::SCREEN_WIDTH;
	}
	if (ImGui::InputScalar("Icon Start Y", ImGuiDataType_S16, &driverDotStartY)) {
		if (driverDotStartY < 0) driverDotStartY = 0;
		if (driverDotStartY > PSX::SCREEN_HEIGHT) driverDotStartY = PSX::SCREEN_HEIGHT;
	}

	ImGui::Separator();
	ImGui::Text("Minimap Orientation:");
	
	// Orientation mode dropdown
	const char* orientationModes[] = { "0°", "90°", "180°", "270°" };
	int currentOrientation = orientationMode;
	if (currentOrientation < 0 || currentOrientation > 3) { currentOrientation = 0; }
	if (ImGui::Combo("Relative rotation", &currentOrientation, orientationModes, 4))
	{
		orientationMode = static_cast<int16_t>(currentOrientation);
	}
	ImGui::SetItemTooltip("Determines minimap clockwise rotation relative to the world\n It doesnt affect texture orientation, it affects how the driver icon moves on the minimap");
	
	ImGui::Separator();
	ImGui::InputScalar("Unknown", ImGuiDataType_S16, &unk);
	ImGui::SetItemTooltip("???");

	ImGui::Separator();
	ImGui::Text("Textures (both halves must have the same dimensions):");

	// Top texture selection
	std::string topPath = topTexturePath.empty() ? "(none)" : topTexturePath.filename().string();
	ImGui::Text("Top:"); ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::BeginDisabled();
	ImGui::InputText("##toptex", &topPath, ImGuiInputTextFlags_ReadOnly);
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Browse##selecttop"))
	{
		auto selection = pfd::open_file("Select Top Minimap Texture", ".",
			{"Image Files", "*.png *.bmp *.jpg *.jpeg", "All Files", "*"}).result();
		if (!selection.empty())
		{
			topTexturePath = selection.front();
			topTexture = Texture(topTexturePath);
			hasTopTexture = !topTexture.IsEmpty();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear##cleartop"))
	{
		topTexturePath.clear();
		topTexture = Texture();
		hasTopTexture = false;
	}

	// Bottom texture selection
	std::string bottomPath = bottomTexturePath.empty() ? "(none)" : bottomTexturePath.filename().string();
	ImGui::Text("Bottom:"); ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::BeginDisabled();
	ImGui::InputText("##bottomtex", &bottomPath, ImGuiInputTextFlags_ReadOnly);
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Browse##selectbottom"))
	{
		auto selection = pfd::open_file("Select Bottom Minimap Texture", ".",
			{"Image Files", "*.png *.bmp *.jpg *.jpeg", "All Files", "*"}).result();
		if (!selection.empty())
		{
			bottomTexturePath = selection.front();
			bottomTexture = Texture(bottomTexturePath);
			hasBottomTexture = !bottomTexture.IsEmpty();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear##clearbottom"))
	{
		bottomTexturePath.clear();
		bottomTexture = Texture();
		hasBottomTexture = false;
	}

	// Status display
	ImGui::Separator();
	if (IsReady())
	{
		// Show texture dimensions
		ImGui::Text("Texture Size: %dx%d pixels", topTexture.GetWidth(), topTexture.GetHeight());
		
		// Check if dimensions match
		if (topTexture.GetWidth() != bottomTexture.GetWidth() || topTexture.GetHeight() != bottomTexture.GetHeight())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: Top and bottom textures have different dimensions!");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Minimap ready!");
		}
	}
	else if (hasTopTexture || hasBottomTexture)
	{
		if (hasTopTexture)
		{
			ImGui::Text("Top texture: %dx%d pixels", topTexture.GetWidth(), topTexture.GetHeight());
		}
		if (hasBottomTexture)
		{
			ImGui::Text("Bottom texture: %dx%d pixels", bottomTexture.GetWidth(), bottomTexture.GetHeight());
		}
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Need both top and bottom textures");
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No minimap textures loaded");
	}
	
	return boundsChanged;
}
