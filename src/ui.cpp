#include "ui.h"

#include "renderer.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <cmath>
#include <filesystem>

int Windows::w_width = 1024;
int Windows::w_height = 768;
bool Windows::w_spawn = false;
bool Windows::w_level = false;
bool Windows::w_material = false;
bool Windows::w_animtex = false;
bool Windows::w_quadblocks = false;
bool Windows::w_checkpoints = false;
bool Windows::w_bsp = false;
bool Windows::w_renderer = false;
bool Windows::w_ghost = false;
bool Windows::w_python = false;
std::string Windows::lastOpenedFolder = ".";

void UI::Render(int width, int height)
{
	MainMenu();
	m_lev.RenderUI();
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
				auto selection = pfd::open_file("Level File", Windows::lastOpenedFolder, {"Level Files", "*.obj *.lev"}, pfd::opt::force_path).result();
				if (!selection.empty())
				{
					const std::filesystem::path levPath = selection.front();
					Windows::lastOpenedFolder = levPath.string();
					if (!m_lev.Load(levPath)) { m_lev.Clear(false); }
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
	static Renderer rend = Renderer(io.DisplaySize.x, io.DisplaySize.y);
	rend.SetViewportSize(io.DisplaySize.x, io.DisplaySize.y);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
	{
		int pixelX = static_cast<int>(io.MousePos.x);
		int pixelY = static_cast<int>(io.MousePos.y);
		if (pixelX >= 0 && pixelY >= 0 && pixelX < static_cast<int>(rend.GetWidth()) && pixelY < static_cast<int>(rend.GetHeight()))
		{
			m_lev.ViewportClickHandleBlockSelection(pixelX, pixelY, ImGui::IsKeyDown(ImGuiKey_ModCtrl), rend);
		}
	}

	if (ImGui::IsKeyDown(ImGuiKey_Escape)) { m_lev.ResetRendererSelection(); }

	if (m_lev.UpdateAnimTextures(rend.GetLastDeltaTime())) { m_lev.UpdateAnimationRenderData(); }

	std::vector<Model> modelsToRender;
	m_lev.BuildRenderModels(modelsToRender);
	rend.Render(modelsToRender);

	static float rollingOneSecond = 0;
	static int FPS = -1;
	float fm = fmod(rollingOneSecond, 1.f);
	if (fm != rollingOneSecond && rollingOneSecond >= 1.f) //2nd condition prevents fps not updating if deltaTime exactly equals 1.f
	{
		FPS = static_cast<int>(1.f / rend.GetLastDeltaTime());
		rollingOneSecond = fm;
	}
	rollingOneSecond += rend.GetLastDeltaTime();
	if (FPS >= 0)
	{
		std::string fpsLabel = "FPS: " + std::to_string(FPS);
		ImVec2 textSize = ImGui::CalcTextSize(fpsLabel.c_str());
		ImVec2 pos = ImVec2(io.DisplaySize.x - textSize.x - 10.0f, 10.0f);
		ImGui::GetForegroundDrawList()->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), fpsLabel.c_str());
	}
}
