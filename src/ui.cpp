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
				auto selection = pfd::open_file("Level File").result();
				if (!selection.empty())
				{
					const std::filesystem::path levPath = selection.front();
					if (!m_lev.Load(levPath)) { m_lev.Clear(); }
				}
			}
			bool ready = m_lev.Ready();
			if (ImGui::MenuItem("Save", nullptr, nullptr, ready))
			{
				auto selection = pfd::select_folder("Level Folder").result();
				if (!selection.empty())
				{
					const std::filesystem::path path = selection + "\\";
					m_lev.Save(path);
				}
			}
			if (!ready) { ImGui::SetItemTooltip("BSP Tree must be generated before saving the level. "); }
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}
