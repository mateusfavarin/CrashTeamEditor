#include "ui.h"

#include "renderer.h"
#include "gui_render_settings.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <cmath>
#include <filesystem>

int Settings::w_width = 1024;
int Settings::w_height = 768;
int Settings::w_x = 50;
int Settings::w_y = 50;
bool Settings::w_maximized = false;
bool Settings::w_spawn = false;
bool Settings::w_level = false;
bool Settings::w_material = false;
bool Settings::w_animtex = false;
bool Settings::w_quadblocks = false;
bool Settings::w_checkpoints = false;
bool Settings::w_bsp = false;
bool Settings::w_renderer = false;
bool Settings::w_ghost = false;
bool Settings::w_python = false;
std::string Settings::m_lastOpenedFolder = ".";
std::string Settings::m_lastOpenedScriptFolder = ".";

UI::UI() : m_rend(Settings::w_width, Settings::w_height)
{
	m_lev.InitModels(m_rend);
}

void UI::Render()
{
	MainMenu();
	m_lev.RenderUI(m_rend);
	RenderWorld();
}

void UI::MainMenu()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open"))
			{
				auto selection = pfd::open_file("Level File", Settings::m_lastOpenedFolder, {"Level Files", "*.obj *.lev"}, pfd::opt::force_path).result();
				if (!selection.empty())
				{
					const std::filesystem::path levPath = selection.front();
					Settings::m_lastOpenedFolder = levPath.string();
					if (!m_lev.Load(levPath)) { m_lev.Clear(false); }
					else { m_rend.SetCameraToLevelSpawn(m_lev.m_spawn[1].pos, m_lev.m_spawn[1].rot); }
				}
			}
			if (ImGui::MenuItem("Save", nullptr, nullptr, m_lev.IsLoaded()))
			{
				auto selection = pfd::select_folder("Level Folder", m_lev.GetParentPath().string(), pfd::opt::force_path).result();
				if (!selection.empty())
				{
					const std::filesystem::path path = selection + "\\";
					m_lev.Save(path);
				}
			}

			if (ImGui::MenuItem("Hot Reload")) { m_lev.OpenHotReloadWindow(); }

			ImGui::BeginDisabled(!m_lev.IsLoaded());
			if (ImGui::BeginMenu("Preset"))
			{
				if (ImGui::MenuItem("Load"))
				{
					std::filesystem::path presetsFolder = m_lev.GetParentPath() / (m_lev.GetName() + "_presets");
					if (!std::filesystem::is_directory(presetsFolder)) { presetsFolder = m_lev.GetParentPath(); }
					auto selection = pfd::open_file("Preset File", presetsFolder.string(), {"Preset Files", "*.json"}, pfd::opt::multiselect | pfd::opt::force_path).result();
					for (const std::string& filename : selection)
					{
						m_lev.LoadPreset(filename);
					}
				}
				if (ImGui::MenuItem("Save"))
				{
					auto selection = pfd::select_folder("Presets Root Directory", m_lev.GetParentPath().string(), pfd::opt::force_path).result();
					if (!selection.empty())
					{
						const std::filesystem::path path = selection + "\\";
						m_lev.SavePreset(path);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void UI::RenderWorld()
{
	if (!m_lev.IsLoaded()) { return; }

	ImGuiIO& io = ImGui::GetIO();
	m_rend.SetViewportSize(io.DisplaySize.x, io.DisplaySize.y);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
	{
		int pixelX = static_cast<int>(io.MousePos.x);
		int pixelY = static_cast<int>(io.MousePos.y);
		if (pixelX >= 0 && pixelY >= 0 && pixelX < static_cast<int>(m_rend.GetWidth()) && pixelY < static_cast<int>(m_rend.GetHeight()))
		{
			m_lev.ViewportClickHandleBlockSelection(pixelX, pixelY, ImGui::IsKeyDown(ImGuiKey_ModShift), m_rend);
		}
	}

	if (ImGui::IsKeyDown(ImGuiKey_Escape)) { m_lev.ResetRendererSelection(); }

	if (m_lev.UpdateAnimTextures(m_rend.GetLastDeltaTime())) { m_lev.UpdateAnimationRenderData(); }

	m_rend.Render(m_lev.m_configFlags & LevConfigFlags::ENABLE_SKYBOX_GRADIENT, m_lev.m_skyGradient);

	static float rollingOneSecond = 0;
	static int FPS = -1;
	float fm = fmod(rollingOneSecond, 1.f);
	if (fm != rollingOneSecond && rollingOneSecond >= 1.f) //2nd condition prevents fps not updating if deltaTime exactly equals 1.f
	{
		FPS = static_cast<int>(1.f / m_rend.GetLastDeltaTime());
		rollingOneSecond = fm;
	}
	rollingOneSecond += m_rend.GetLastDeltaTime();
	if (FPS >= 0)
	{
		std::string fpsLabel = "FPS: " + std::to_string(FPS);
		ImVec2 textSize = ImGui::CalcTextSize(fpsLabel.c_str());
		ImVec2 pos = ImVec2(io.DisplaySize.x - textSize.x - 10.0f, 10.0f);
		ImGui::GetForegroundDrawList()->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), fpsLabel.c_str());
	}

	if (GuiRenderSettings::showSelectedQuadblockInfo)
	{
		bool atLeastOneSelected = (!m_lev.m_rendererSelectedQuadblockIndexes.empty());

		ImVec2 window_pos = ImVec2(5, io.DisplaySize.y - 5);
		ImVec2 window_pos_pivot = ImVec2(0.0f, 1.0f);
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("##RendererQueryPointerInfo", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);

		if (atLeastOneSelected)
		{
			size_t idx = m_lev.m_rendererSelectedQuadblockIndexes.back();
			if (idx < m_lev.m_quadblocks.size())
			{
				const Quadblock& qb = m_lev.m_quadblocks[idx];

				ImGui::Text("Selected Quadblock: ID %zu", idx);
				ImGui::Text("Name: %s", qb.GetName().c_str());
				ImGui::Text("Material: %s", qb.GetMaterial().c_str());

				uint8_t terrain = qb.GetTerrain();
				std::string terrainName = "Unknown";
				for (const auto& pair : TerrainType::LABELS)
				{
					if (pair.second == terrain) { terrainName = pair.first; break; }
				}
				ImGui::Text("Terrain: %s", terrainName.c_str());

				uint16_t flags = qb.GetFlags();
				std::string flagList = "[";
				bool first = true;
				for (const auto& pair : QuadFlags::LABELS)
				{
					if (flags & pair.second)
					{
						if (!first) { flagList += ", "; }
						flagList += pair.first;
						first = false;
					}
				}
				flagList += "]";
				ImGui::Text("Quadflags: %s", flagList.c_str());
				ImGui::Text("Checkpoint Index: %d", qb.GetCheckpoint());

				ImGui::Separator();
			}
		}

		const Vec3& queryPointer = m_lev.m_rendererQueryPoint;
		ImGui::Text("Query Pointer: (%.2f, %.2f, %.2f)", queryPointer.x, queryPointer.y, queryPointer.z);

		ImGui::End();
	}

}
