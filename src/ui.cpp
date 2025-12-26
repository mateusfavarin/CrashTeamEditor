#include "ui.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

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
