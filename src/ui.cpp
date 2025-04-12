#include "ui.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <filesystem>

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
				auto selection = pfd::open_file("Level File", ".", {"Level Files", "*.obj"}).result();
				if (!selection.empty())
				{
					const std::filesystem::path levPath = selection.front();
					if (!m_lev.Load(levPath)) { m_lev.Clear(false); }
				}
			}
			bool ready = m_lev.Ready();
			if (ImGui::MenuItem("Save", nullptr, nullptr, ready))
			{
				auto selection = pfd::select_folder("Level Folder", m_lev.GetParentPath().string()).result();
				if (!selection.empty())
				{
					const std::filesystem::path path = selection + "\\";
					m_lev.Save(path);
				}
			}
			if (!ready) { ImGui::SetItemTooltip("BSP Tree must be generated before saving the level. "); }

			if (ImGui::MenuItem("Hot Reload"))
			{
				m_lev.OpenHotReloadWindow();
			}

			ImGui::BeginDisabled(!m_lev.Loaded());
			if (ImGui::BeginMenu("Preset"))
			{
				if (ImGui::MenuItem("Load"))
				{
					std::filesystem::path presetsFolder = m_lev.GetParentPath() / (m_lev.GetName() + "_presets");
					if (!std::filesystem::is_directory(presetsFolder)) { presetsFolder = m_lev.GetParentPath(); }
					auto selection = pfd::open_file("Preset File", presetsFolder.string(), {"Preset Files", "*.json"}, pfd::opt::multiselect).result();
					for (const std::string& filename : selection)
					{
						m_lev.LoadPreset(filename);
					}
				}
				if (ImGui::MenuItem("Save"))
				{
					auto selection = pfd::select_folder("Presets Root Directory", m_lev.GetParentPath().string()).result();
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
