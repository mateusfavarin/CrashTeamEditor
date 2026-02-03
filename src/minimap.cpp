#include "minimap.h"
#include "quadblock.h"

#include <stb_image.h>
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
	map.orientationMode = orientationMode;
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
	orientationMode = map.orientationMode;
	unk = map.unk;
}

void MinimapConfig::LoadTextures()
{
	lastError.clear();
	hasTopTexture = false;
	hasBottomTexture = false;
	topTexture = Texture();
	bottomTexture = Texture();

	if (sourceTexturePath.empty()) {
		lastError = "No file selected";
		return;
	}
	if (!std::filesystem::exists(sourceTexturePath)) {
		lastError = "File does not exist";
		return;
	}

	// Load the source image using stb_image
	int width = 0, height = 0, channels = 0;
	unsigned char* sourceImage = stbi_load(sourceTexturePath.string().c_str(), &width, &height, &channels, 4); // Force RGBA
	if (sourceImage == nullptr) {
		lastError = "Failed to load image (unsupported format or corrupted file)";
		return;
	}

	if (width <= 0 || height <= 0) {
		stbi_image_free(sourceImage);
		lastError = "Image has invalid dimensions";
		return;
	}

	// Image should be twice the height (top + bottom halves)
	if (height % 2 != 0) {
		stbi_image_free(sourceImage);
		lastError = "Image height must be even (for splitting into top/bottom)";
		return;
	}

	int halfHeight = height / 2;
	int stretchedWidth = static_cast<int>(width * 1.5f);

	if (stretchedWidth <= 0) {
		stbi_image_free(sourceImage);
		lastError = "Stretched width is invalid (check image width)";
		return;
	}

	// Create buffers for stretched top and bottom halves
	std::vector<unsigned char> topPixels(stretchedWidth * halfHeight * 4);
	std::vector<unsigned char> bottomPixels(stretchedWidth * halfHeight * 4);

	// Stretch and split the image using nearest-neighbor sampling
	for (int y = 0; y < halfHeight; y++) {
		for (int x = 0; x < stretchedWidth; x++) {
			int srcX = static_cast<int>(x / 1.5f);
			if (srcX >= width) srcX = width - 1;

			// Top half
			int srcTopIdx = (y * width + srcX) * 4;
			int dstTopIdx = (y * stretchedWidth + x) * 4;
			topPixels[dstTopIdx + 0] = sourceImage[srcTopIdx + 0];
			topPixels[dstTopIdx + 1] = sourceImage[srcTopIdx + 1];
			topPixels[dstTopIdx + 2] = sourceImage[srcTopIdx + 2];
			topPixels[dstTopIdx + 3] = sourceImage[srcTopIdx + 3];

			// Bottom half
			int srcBottomIdx = ((y + halfHeight) * width + srcX) * 4;
			int dstBottomIdx = (y * stretchedWidth + x) * 4;
			bottomPixels[dstBottomIdx + 0] = sourceImage[srcBottomIdx + 0];
			bottomPixels[dstBottomIdx + 1] = sourceImage[srcBottomIdx + 1];
			bottomPixels[dstBottomIdx + 2] = sourceImage[srcBottomIdx + 2];
			bottomPixels[dstBottomIdx + 3] = sourceImage[srcBottomIdx + 3];
		}
	}

	stbi_image_free(sourceImage);

	topTexture = Texture::CreateFromPixelData(topPixels.data(), stretchedWidth, halfHeight);
	bottomTexture = Texture::CreateFromPixelData(bottomPixels.data(), stretchedWidth, halfHeight);

	hasTopTexture = !topTexture.IsEmpty();
	hasBottomTexture = !bottomTexture.IsEmpty();

	if (!hasTopTexture || !hasBottomTexture) {
		lastError = "Failed to create minimap textures from image data";
	}
}

bool MinimapConfig::IsReady() const
{
	return enabled && hasTopTexture && hasBottomTexture && lastError.empty();
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
	sourceTexturePath.clear();
	topTexture = Texture();
	bottomTexture = Texture();
	hasTopTexture = false;
	hasBottomTexture = false;
	enabled = false;
	lastError.clear();
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
	ImGui::Text("Minimap Texture:");

	// Single texture selection
	std::string sourcePath = sourceTexturePath.empty() ? "(none)" : sourceTexturePath.filename().string();
	ImGui::Text("Image:"); ImGui::SameLine();
	ImGui::SetNextItemWidth(250.0f);
	ImGui::BeginDisabled();
	ImGui::InputText("##sourcetex", &sourcePath, ImGuiInputTextFlags_ReadOnly);
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Browse##selectsource"))
	{
		auto selection = pfd::open_file("Select Minimap Texture (will be split into top/bottom)", ".",
			{"Image Files", "*.png *.bmp *.jpg *.jpeg", "All Files", "*"}).result();
		if (!selection.empty())
		{
			sourceTexturePath = selection.front();
			LoadTextures(); // Process the image immediately
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear##clearsource"))
	{
		sourceTexturePath.clear();
		topTexture = Texture();
		bottomTexture = Texture();
		hasTopTexture = false;
		hasBottomTexture = false;
		lastError.clear();
	}

	// Status display
	ImGui::Separator();
	if (IsReady())
	{
		ImGui::Text("Processed Size (each half): %dx%d pixels", topTexture.GetWidth(), topTexture.GetHeight());
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Minimap ready!");
	}
	else if (!sourceTexturePath.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", lastError.empty() ? "Unknown error" : lastError.c_str());
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No minimap texture loaded");
	}

	return boundsChanged;
}
