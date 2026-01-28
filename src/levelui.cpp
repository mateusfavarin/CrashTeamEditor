#include "geo.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "path.h"
#include "quadblock.h"
#include "vertex.h"
#include "utils.h"
#include "gui_render_settings.h"
#include "mesh.h"
#include "texture.h"
#include "ui.h"
#include "script.h"
#include "minimap.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <portable-file-dialogs.h>

#include <string>
#include <chrono>
#include <functional>
#include <fstream>
#include <sstream>
#include <cstdint>

class ButtonUI
{
public:
	ButtonUI();
	ButtonUI(long long timeout);
	bool Show(const std::string& label, const std::string& message, bool unsavedChanges);

private:
	static constexpr long long DEFAULT_TIMEOUT = 1;
	long long m_timeout;
	std::string m_labelTriggered;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_messageTimeoutStart;
};

ButtonUI::ButtonUI()
{
	m_timeout = DEFAULT_TIMEOUT;
	m_labelTriggered = std::string();
	m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
}

ButtonUI::ButtonUI(long long timeout)
{
	m_timeout = timeout;
	m_labelTriggered = std::string();
	m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
}

bool ButtonUI::Show(const std::string& label, const std::string& message, bool unsavedChanges)
{
	bool ret = false;
	if (ImGui::Button(label.c_str()))
	{
		m_labelTriggered = label;
		m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
		ret = true;
	}
	if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_messageTimeoutStart).count() < m_timeout
		&& m_labelTriggered == label)
	{
		ImGui::Text(message.c_str());
	}
	else if (unsavedChanges)
	{
		const ImVec4 redColor = {247.0f / 255.0f, 44.0f / 255.0f, 37.0f / 255.0f, 1.0f};
		ImGui::PushStyleColor(ImGuiCol_Text, redColor);
		ImGui::Text("Unsaved changes.");
		ImGui::PopStyleColor();
	}
	return ret;
}

template<typename T>
static bool UIFlagCheckbox(T& var, const T flag, const std::string& title)
{
	bool active = var & flag;
	if (ImGui::Checkbox(title.c_str(), &active))
	{
		if (active) { var |= flag; }
		else { var &= ~flag; }
		return true;
	}
	return false;
}

void BoundingBox::RenderUI() const
{
	ImGui::Text("Max:"); ImGui::SameLine();
	ImGui::BeginDisabled();
	ImGui::InputFloat3("##max", const_cast<float*>(&max.x));
	ImGui::EndDisabled();
	ImGui::Text("Min:"); ImGui::SameLine();
	ImGui::BeginDisabled();
	ImGui::InputFloat3("##min", const_cast<float*>(&min.x));
	ImGui::EndDisabled();
}

void BSP::RenderUI(const std::vector<Quadblock>& quadblocks)
{
	std::string title = GetType() + " " + std::to_string(m_id);
	if (ImGui::TreeNode(title.c_str()))
	{
		if (IsBranch()) { ImGui::Text(("Axis:  " + GetAxis()).c_str()); }
		ImGui::Text(("Quads: " + std::to_string(m_quadblockIndexes.size())).c_str());
		if (ImGui::TreeNode("Quadblock List:"))
		{
			constexpr size_t QUADS_PER_LINE = 10;
			for (size_t i = 0; i < m_quadblockIndexes.size(); i++)
			{
				ImGui::Text((quadblocks[m_quadblockIndexes[i]].GetName() + ", ").c_str());
				if (((i + 1) % QUADS_PER_LINE) == 0 || i == m_quadblockIndexes.size() - 1) { continue; }
				ImGui::SameLine();
			}
			ImGui::TreePop();
		}
		ImGui::Text("Bounding Box:");
		m_bbox.RenderUI();
		if (m_left) { m_left->RenderUI(quadblocks); }
		if (m_right) { m_right->RenderUI(quadblocks); }
		ImGui::TreePop();
	}
}

void Checkpoint::RenderUI(size_t numCheckpoints, const std::vector<Quadblock>& quadblocks)
{
	if (ImGui::TreeNode(("Checkpoint " + std::to_string(m_index)).c_str()))
	{
		ImGui::Text("Pos:       "); ImGui::SameLine(); ImGui::InputFloat3("##pos", m_pos.Data());
		ImGui::Text("Quad:      "); ImGui::SameLine();
		if (ImGui::BeginCombo("##quad", m_uiPosQuad.c_str()))
		{
			for (const Quadblock& quadblock : quadblocks)
			{
				if (ImGui::Selectable(quadblock.GetName().c_str()))
				{
					m_uiPosQuad = quadblock.GetName();
					m_pos = quadblock.GetCenter();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("Update checkpoint position by selecting a specific quadblock.");

		ImGui::Text("Distance:  "); ImGui::SameLine();
		if (ImGui::InputFloat("##dist", &m_distToFinish)) { m_distToFinish = m_distToFinish < 0.0f ? 0.0f : m_distToFinish; }
		ImGui::SetItemTooltip("Distance from checkpoint to the finish line.");

		auto LinkUI = [](int index, size_t numCheckpoints, int& dir, std::string& s, const std::string& title)
			{
				if (dir == NONE_CHECKPOINT_INDEX) { s = DEFAULT_UI_CHECKBOX_LABEL; }
				ImGui::Text(title.c_str()); ImGui::SameLine();
				if (ImGui::BeginCombo(("##" + title).c_str(), s.c_str()))
				{
					if (ImGui::Selectable(DEFAULT_UI_CHECKBOX_LABEL.c_str())) { dir = NONE_CHECKPOINT_INDEX; }
					for (int i = 0; i < numCheckpoints; i++)
					{
						if (i == index) { continue; }
						std::string str = "Checkpoint " + std::to_string(i);
						if (ImGui::Selectable(str.c_str()))
						{
							s = str;
							dir = i;
						}
					}
					ImGui::EndCombo();
				}
			};

		LinkUI(m_index, numCheckpoints, m_up, m_uiLinkUp, "Link up:   ");
		LinkUI(m_index, numCheckpoints, m_down, m_uiLinkDown, "Link down: ");
		LinkUI(m_index, numCheckpoints, m_left, m_uiLinkLeft, "Link left: ");
		LinkUI(m_index, numCheckpoints, m_right, m_uiLinkRight, "Link right:");

		if (ImGui::Button("Delete")) { m_delete = true; }
		ImGui::TreePop();
	}
}

template<typename T, MaterialType M>
bool MaterialProperty<T, M>::RenderUI(const std::string& material, const std::vector<size_t>& quadblockIndexes, std::vector<Quadblock>& quadblocks)
{
	if constexpr (M == MaterialType::TERRAIN)
	{
		ImGui::Text("Terrain:"); ImGui::SameLine();
		if (ImGui::BeginCombo("##terrain", GetPreview(material).c_str()))
		{
			for (const auto& [label, terrain] : TerrainType::LABELS)
			{
				if (ImGui::Selectable(label.c_str()))
				{
					SetPreview(material, label);
				}
			}
			ImGui::EndCombo();
		} ImGui::SameLine();

		static ButtonUI terrainApplyButton = ButtonUI();
		if (terrainApplyButton.Show(("Apply##terrain" + material).c_str(), "Terrain type successfully updated.", UnsavedChanges(material)))
		{
			Apply(material, quadblockIndexes, quadblocks);
			return true;
		}
	}
	else if constexpr (M == MaterialType::QUAD_FLAGS)
	{
		if (ImGui::TreeNode("Quad Flags"))
		{
			for (const auto& [label, flag] : QuadFlags::LABELS)
			{
				UIFlagCheckbox(GetPreview(material), flag, label);
			}

			static ButtonUI quadFlagsApplyButton = ButtonUI();
			if (quadFlagsApplyButton.Show(("Apply##quadflags" + material).c_str(), "Quad flags successfully updated.", UnsavedChanges(material)))
			{
				Apply(material, quadblockIndexes, quadblocks);
				return true;
			}
			static ButtonUI killPlaneButton = ButtonUI();
			if (killPlaneButton.Show("Kill Plane##quadflags", "Modified quad flags to kill plane.", false))
			{
				SetPreview(material, QuadFlags::INVISIBLE_TRIGGER | QuadFlags::OUT_OF_BOUNDS | QuadFlags::MASK_GRAB | QuadFlags::WALL | QuadFlags::NO_COLLISION);
				Apply(material, quadblockIndexes, quadblocks);
				return true;
			}
			ImGui::TreePop();
		}
	}
	else if constexpr (M == MaterialType::DRAW_FLAGS)
	{
		if (ImGui::TreeNode("Draw Flags"))
		{
			T& preview = GetPreview(material);
			ImGui::Checkbox("Double Sided", &preview);

			static ButtonUI drawFlagsApplyButton = ButtonUI();
			if (drawFlagsApplyButton.Show(("Apply##drawflags" + material).c_str(), "Draw flags successfully updated.", UnsavedChanges(material)))
			{
				Apply(material, quadblockIndexes, quadblocks);
				return true;
			}
			ImGui::TreePop();
		}
	}
	else if constexpr (M == MaterialType::CHECKPOINT)
	{
		T& preview = GetPreview(material);
		ImGui::Checkbox("Checkpoint", &preview);
		ImGui::SameLine();

		static ButtonUI checkpointApplyButton = ButtonUI();
		if (checkpointApplyButton.Show(("Apply##checkpoint" + material).c_str(), "Checkpoint status successfully updated.", UnsavedChanges(material)))
		{
			Apply(material, quadblockIndexes, quadblocks);
			return true;
		}
	}
	else if constexpr (M == MaterialType::TURBO_PAD)
	{
		T& trigger = GetPreview(material);
		ImGui::Text("Trigger:"); ImGui::SameLine();
		if (ImGui::RadioButton("None", trigger == QuadblockTrigger::NONE))
		{
			trigger = QuadblockTrigger::NONE;
		} ImGui::SameLine();
		if (ImGui::RadioButton("Turbo Pad", trigger == QuadblockTrigger::TURBO_PAD))
		{
			trigger = QuadblockTrigger::TURBO_PAD;
		} ImGui::SameLine();
		if (ImGui::RadioButton("Super Turbo Pad", trigger == QuadblockTrigger::SUPER_TURBO_PAD))
		{
			trigger = QuadblockTrigger::SUPER_TURBO_PAD;
		} ImGui::SameLine();
		static ButtonUI padApplyButton = ButtonUI();
		if (padApplyButton.Show(("Apply##pad" + material).c_str(), "Turbo pad status successfully updated.", UnsavedChanges(material)))
		{
			Apply(material, quadblockIndexes, quadblocks);
			return true;
		}
	}
	else if constexpr (M == MaterialType::SPEED_IMPACT)
	{
		T& preview = GetPreview(material);
		ImGui::Text("Downforce:"); ImGui::SameLine();
		if (ImGui::InputInt("##downforce", &preview)) { preview = Clamp(preview, static_cast<T>(INT8_MIN), static_cast<T>(INT8_MAX)); }
		ImGui::SameLine();
		static ButtonUI speedApplyButton = ButtonUI();
		if (speedApplyButton.Show(("Apply##downforce" + material).c_str(), "Downforce successfully updated.", UnsavedChanges(material)))
		{
			Apply(material, quadblockIndexes, quadblocks);
			return true;
		}
	}
	else if constexpr (M == MaterialType::CHECKPOINT_PATHABLE)
	{
		T& preview = GetPreview(material);
		ImGui::Checkbox("Checkpoint Pathable", &preview);
		ImGui::SameLine();

		static ButtonUI pathableApplyButton = ButtonUI();
		if (pathableApplyButton.Show(("Apply##pathable" + material).c_str(), "Checkpoint pathable status successfully updated.", UnsavedChanges(material)))
		{
			Apply(material, quadblockIndexes, quadblocks);
			return true;
		}
	}
	return false;
}

void Level::RenderUI()
{
	if (m_showLogWindow)
	{
		if (ImGui::Begin("Log", &m_showLogWindow))
		{
			if (!m_logMessage.empty()) { ImGui::Text(m_logMessage.c_str()); }
			if (!m_invalidQuadblocks.empty())
			{
				ImGui::Text("Error - the following quadblocks are not in the valid format:");
				for (size_t i = 0; i < m_invalidQuadblocks.size(); i++)
				{
					const std::string& quadblock = std::get<0>(m_invalidQuadblocks[i]);
					const std::string& errorMessage = std::get<1>(m_invalidQuadblocks[i]);
					if (ImGui::TreeNode((quadblock + "##" + std::to_string(i)).c_str()))
					{
						ImGui::Text(errorMessage.c_str());
						ImGui::TreePop();
					}
				}
			}
		}
		ImGui::End();
	}

	if (m_showHotReloadWindow)
	{
		if (ImGui::Begin("Hot Reload", &m_showHotReloadWindow))
		{
			std::string levPath = m_hotReloadLevPath.string();
			ImGui::Text("Lev Path"); ImGui::SameLine();
			ImGui::InputText("##levpath", &levPath, ImGuiInputTextFlags_ReadOnly);
			ImGui::SetItemTooltip(levPath.c_str()); ImGui::SameLine();
			if (ImGui::Button("...##levhotreload"))
			{
				auto selection = pfd::open_file("Lev File", m_parentPath.string(), {"Lev Files", "*.lev"}, pfd::opt::force_path).result();
				if (!selection.empty()) { m_hotReloadLevPath = selection.front(); }
			}

			std::string vrmPath = m_hotReloadVRMPath.string();
			ImGui::Text("Vrm Path"); ImGui::SameLine();
			ImGui::InputText("##vrmpath", &vrmPath, ImGuiInputTextFlags_ReadOnly);
			ImGui::SetItemTooltip(vrmPath.c_str()); ImGui::SameLine();
			if (ImGui::Button("...##vrmhotreload"))
			{
				auto selection = pfd::open_file("Vrm File", m_parentPath.string(), {"Vrm Files", "*.vrm"}, pfd::opt::force_path).result();
				if (!selection.empty()) { m_hotReloadVRMPath = selection.front(); }
			}

			const std::string successMessage = "Successfully hot reloaded.";
			const std::string failMessage = "Failed hot reloading.\nMake sure Duckstation is opened and that the game is unpaused.";

			bool disabled = levPath.empty();
			ImGui::BeginDisabled(disabled);
			static ButtonUI hotReloadButton = ButtonUI(5);
			static std::string hotReloadMessage;
			if (hotReloadButton.Show("Hot Reload##btn", hotReloadMessage, false))
			{
				if (HotReload(levPath, vrmPath, "duckstation")) { hotReloadMessage = successMessage; }
				else { hotReloadMessage = failMessage; }
			}
			ImGui::EndDisabled();
			if (disabled) { ImGui::SetItemTooltip("You must select the lev path before hot reloading."); }

			bool vrmDisabled = vrmPath.empty();
			ImGui::BeginDisabled(vrmDisabled);
			static ButtonUI vrmOnlyButton = ButtonUI(5);
			static std::string vrmOnlyMessage;
			if (vrmOnlyButton.Show("Vrm Only##btn", vrmOnlyMessage, false))
			{
				if (HotReload(std::string(), vrmPath, "duckstation")) { hotReloadMessage = successMessage; }
				else { hotReloadMessage = failMessage; }
			}
			ImGui::EndDisabled();
			if (vrmDisabled) { ImGui::SetItemTooltip("You must select the vrm path before hot reloading the vram."); }
		}
		ImGui::End();
	}

	if (!m_loaded) { return; }

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::MenuItem("Spawn")) { Windows::w_spawn = !Windows::w_spawn; }
		if (ImGui::MenuItem("Level")) { Windows::w_level = !Windows::w_level; }
		if (!m_materialToQuadblocks.empty() && ImGui::MenuItem("Material")) { Windows::w_material = !Windows::w_material; }
		if (ImGui::MenuItem("Anim Tex")) { Windows::w_animtex = !Windows::w_animtex; }
		if (ImGui::MenuItem("Quadblocks")) { Windows::w_quadblocks = !Windows::w_quadblocks; }
		if (ImGui::MenuItem("Checkpoints")) { Windows::w_checkpoints = !Windows::w_checkpoints; }
		if (ImGui::MenuItem("BSP Tree")) { Windows::w_bsp = !Windows::w_bsp; }
		if (ImGui::MenuItem("Renderer")) { Windows::w_renderer = !Windows::w_renderer; }
		if (ImGui::MenuItem("Ghosts")) { Windows::w_ghost = !Windows::w_ghost; }
		if (ImGui::MenuItem("Python")) { Windows::w_python = !Windows::w_python; }
		ImGui::EndMainMenuBar();
	}

	if (Windows::w_spawn)
	{
		if (ImGui::Begin("Spawn", &Windows::w_spawn))
		{
			for (size_t i = 0; i < NUM_DRIVERS; i++)
			{
				if (ImGui::TreeNode(("Driver " + std::to_string(i)).c_str()))
				{
					if (ImGui::Button(("Set from selection##" + std::to_string(i)).c_str()))
					{
						m_spawn[i].pos = m_rendererQueryPoint;
						GenerateRenderStartpointData(m_spawn);
					}
					ImGui::Text("Pos:"); ImGui::SameLine();
					bool changed = ImGui::InputFloat3("##pos", m_spawn[i].pos.Data());

					ImGui::Text("Rot:"); ImGui::SameLine();
					if (ImGui::InputFloat3("##rot", m_spawn[i].rot.Data()))
					{
						changed = true;
						m_spawn[i].rot.x = Clamp(m_spawn[i].rot.x, -360.0f, 360.0f);
						m_spawn[i].rot.y = Clamp(m_spawn[i].rot.y, -360.0f, 360.0f);
						m_spawn[i].rot.z = Clamp(m_spawn[i].rot.z, -360.0f, 360.0f);
					};
					if (changed) { GenerateRenderStartpointData(m_spawn); }
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

	if (Windows::w_level)
	{
		if (ImGui::Begin("Level", &Windows::w_level))
		{
			if (ImGui::TreeNode("Flags"))
			{
				UIFlagCheckbox(m_configFlags, LevConfigFlags::ENABLE_SKYBOX_GRADIENT, "Enable Skybox Gradient");
				UIFlagCheckbox(m_configFlags, LevConfigFlags::MASK_GRAB_UNDERWATER, "Mask Grab Underwater");
				UIFlagCheckbox(m_configFlags, LevConfigFlags::ANIMATE_WATER_VERTEX, "Animate Water Vertex");
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Sky Gradient"))
			{
				for (size_t i = 0; i < NUM_GRADIENT; i++)
				{
					if (ImGui::TreeNode(("Gradient " + std::to_string(i)).c_str()))
					{
						ImGui::Text("From:"); ImGui::SameLine(); ImGui::InputFloat("##pos_from", &m_skyGradient[i].posFrom);
						ImGui::Text("To:  "); ImGui::SameLine(); ImGui::InputFloat("##pos_to", &m_skyGradient[i].posTo);
						ImGui::Text("From:"); ImGui::SameLine();
						float colorFrom[3] = {m_skyGradient[i].colorFrom.Red(), m_skyGradient[i].colorFrom.Green(), m_skyGradient[i].colorFrom.Blue()};
						if (ImGui::ColorEdit3("##color_from", colorFrom))
						{
							m_skyGradient[i].colorFrom = Color(static_cast<float>(colorFrom[0]), colorFrom[1], colorFrom[2]);
						}
						ImGui::Text("To:  "); ImGui::SameLine();
						float colorTo[3] = {m_skyGradient[i].colorTo.Red(), m_skyGradient[i].colorTo.Green(), m_skyGradient[i].colorTo.Blue()};
						if (ImGui::ColorEdit3("##color_to", colorTo))
						{
							m_skyGradient[i].colorTo = Color(static_cast<float>(colorTo[0]), colorTo[1], colorTo[2]);
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Clear Color"))
			{
				float clearColor[3] = {m_clearColor.Red(), m_clearColor.Green(), m_clearColor.Blue()};
				if (ImGui::ColorEdit3("##color", clearColor))
				{
					m_clearColor = Color(static_cast<float>(clearColor[0]), clearColor[1], clearColor[2]);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Stars"))
			{
				ImGui::InputScalar("Number of Stars", ImGuiDataType_U16, &m_stars.numStars);

				ImGui::InputScalar("Seed", ImGuiDataType_U16, &m_stars.seed);
				ImGui::SetItemTooltip("Controls the random spread distance from the horizon.\n");

				ImGui::InputScalar("Z Depth", ImGuiDataType_U16, &m_stars.zDepth);
				ImGui::SetItemTooltip("Distance from screen (OT). Default is 1022.\nSkybox is drawn at 1023.");

				ImGui::Checkbox("Spread Stars Below Horizon", &m_stars.spread);
				ImGui::SetItemTooltip("When disabled, stars appear only above the horizon.\nWhen enabled, stars will also appear below the horizon.");

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Minimap"))
			{
				if (m_minimapConfig.RenderUI(m_quadblocks) && GuiRenderSettings::showMinimapBounds)
				{
					GenerateRenderMinimapBoundsData();
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	if (Windows::w_material)
	{
		if (ImGui::Begin("Material", &Windows::w_material))
		{
			for (const auto& [material, quadblockIndexes] : m_materialToQuadblocks)
			{
				if (ImGui::TreeNode(material.c_str()))
				{
					if (ImGui::TreeNode("Quadblocks"))
					{
						constexpr size_t QUADS_PER_LINE = 10;
						for (size_t i = 0; i < quadblockIndexes.size(); i++)
						{
							ImGui::Text((m_quadblocks[quadblockIndexes[i]].GetName() + ", ").c_str());
							if (((i + 1) % QUADS_PER_LINE) == 0 || i == quadblockIndexes.size() - 1) { continue; }
							ImGui::SameLine();
						}
						ImGui::TreePop();
					}

					m_propTerrain.RenderUI(material, quadblockIndexes, m_quadblocks);
					m_propQuadFlags.RenderUI(material, quadblockIndexes, m_quadblocks);
					m_propDoubleSided.RenderUI(material, quadblockIndexes, m_quadblocks);
					m_propCheckpoints.RenderUI(material, quadblockIndexes, m_quadblocks);
					m_propCheckpointPathable.RenderUI(material, quadblockIndexes, m_quadblocks);
					if (m_propTurboPads.RenderUI(material, quadblockIndexes, m_quadblocks))
					{
						for (size_t index : quadblockIndexes) { ManageTurbopad(m_quadblocks[index]); }
						if (m_bsp.IsValid())
						{
							m_bsp.Clear();
							GenerateRenderBspData(m_bsp);
						}
					}
					m_propSpeedImpact.RenderUI(material, quadblockIndexes, m_quadblocks);

					if (m_materialToTexture.contains(material))
					{
						m_materialToTexture[material].RenderUI(quadblockIndexes, m_quadblocks, [&]() { this->UpdateAnimationRenderData(); });
					}

					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

	if (!Windows::w_material) { RestoreMaterials(this); }

	if (Windows::w_animtex)
	{
		if (ImGui::Begin("Animated Textures", &Windows::w_animtex))
		{
			static std::string animTexQuerry;
			std::vector<std::string> animTexNames;
			for (const AnimTexture& currAnimTex : m_animTextures)
			{
				animTexNames.push_back(currAnimTex.GetName());
			}
			ImGui::InputTextWithHint("Search##", "Search Query...", &animTexQuerry);
			static std::string errorLoadingAnim;
			if (ImGui::Button("Load"))
			{
				auto selection = pfd::open_file("Animated Texture", m_parentPath.string(), {"Animated Texture Files", "*.obj"}, pfd::opt::force_path).result();
				if (!selection.empty())
				{
					const std::filesystem::path& animTexPath = selection.front();
					AnimTexture animTex = AnimTexture(animTexPath, animTexNames);
					if (!animTex.IsEmpty()) { m_animTextures.push_back(animTex); errorLoadingAnim.clear(); }
					else { errorLoadingAnim = "Error loading " + animTexPath.string(); }
				}
			}
			if (!errorLoadingAnim.empty()) { ImGui::Text(errorLoadingAnim.c_str()); }
			size_t remIndex = 0;
			std::vector<size_t> remAnimTexIndex;
			std::vector<AnimTexture> newTextures;
			for (AnimTexture& tex : m_animTextures)
			{
				if (!tex.RenderUI(animTexNames, m_quadblocks, m_materialToQuadblocks, animTexQuerry, newTextures))
				{
					remAnimTexIndex.push_back(remIndex);
				}
				remIndex++;
			}
			for (int i = static_cast<int>(remAnimTexIndex.size()) - 1; i >= 0; i--)
			{
				m_animTextures.erase(m_animTextures.begin() + remAnimTexIndex[i]);
			}
			for (const AnimTexture& newTex : newTextures)
			{
				bool foundEquivalent = false;
				for (AnimTexture& tex : m_animTextures)
				{
					if (newTex.IsEquivalent(tex))
					{
						const std::vector<size_t>& newIndexes = newTex.GetQuadblockIndexes();
						for (size_t index : newIndexes) { tex.AddQuadblockIndex(index); }
						foundEquivalent = true;
						break;
					}
				}
				if (!foundEquivalent) { m_animTextures.push_back(newTex); }
			}
		}
		ImGui::End();
	}

	static std::string quadblockQuery;
	if (Windows::w_quadblocks)
	{
		bool resetBsp = false;
		if (ImGui::Begin("Quadblocks", &Windows::w_quadblocks))
		{
			ImGui::InputTextWithHint("Search", "Search Quadblocks...", &quadblockQuery);
			for (Quadblock& quadblock : m_quadblocks)
			{
				if (!quadblock.GetHide() && Matches(quadblock.GetName(), quadblockQuery))
				{
					if (quadblock.RenderUI(m_checkpoints.size() - 1, resetBsp))
					{
						ManageTurbopad(quadblock);
					}
				}
			}
		}
		ImGui::End();
		if (resetBsp && m_bsp.IsValid())
		{
			m_bsp.Clear();
			GenerateRenderBspData(m_bsp);
		}
	}

	if (!quadblockQuery.empty() && !Windows::w_quadblocks) { quadblockQuery.clear(); }

	static std::string checkpointQuery;
	if (Windows::w_checkpoints)
	{
		if (ImGui::Begin("Checkpoints", &Windows::w_checkpoints))
		{
			ImGui::InputTextWithHint("Search##", "Search Quadblocks...", &checkpointQuery);
			if (ImGui::TreeNode("Checkpoints"))
			{
				std::vector<int> checkpointsDelete;
				for (int i = 0; i < m_checkpoints.size(); i++)
				{
					m_checkpoints[i].RenderUI(m_checkpoints.size(), m_quadblocks);
					if (m_checkpoints[i].GetDelete()) { checkpointsDelete.push_back(i); }
				}
				if (!checkpointsDelete.empty())
				{
					for (int i = static_cast<int>(checkpointsDelete.size()) - 1; i >= 0; i--)
					{
						m_checkpoints.erase(m_checkpoints.begin() + checkpointsDelete[i]);
					}
					for (int i = 0; i < m_checkpoints.size(); i++)
					{
						m_checkpoints[i].RemoveInvalidCheckpoints(checkpointsDelete);
						m_checkpoints[i].UpdateInvalidCheckpoints(checkpointsDelete);
						m_checkpoints[i].SetIndex(i);
					}
				}
				if (ImGui::Button("Add Checkpoint"))
				{
					m_checkpoints.emplace_back(static_cast<int>(m_checkpoints.size()));
				}
				ImGui::TreePop();
			}
		}
		if (ImGui::TreeNode("Generate"))
		{
			for (size_t i = 0; i < m_checkpointPaths.size(); i++)
			{
				bool insertAbove = false;
				bool removePath = false;
				Path& path = m_checkpointPaths[i];
				const std::string pathTitle = "Path " + std::to_string(path.GetIndex());
				path.RenderUI(pathTitle, m_quadblocks, checkpointQuery, insertAbove, removePath, m_rendererSelectedQuadblockIndexes, true);
				if (insertAbove)
				{
					m_checkpointPaths.insert(m_checkpointPaths.begin() + path.GetIndex(), Path());
					for (size_t j = 0; j < m_checkpointPaths.size(); j++) { m_checkpointPaths[j].SetIndex(j); }
				}
				if (removePath)
				{
					m_checkpointPaths.erase(m_checkpointPaths.begin() + path.GetIndex());
					for (size_t j = 0; j < m_checkpointPaths.size(); j++) { m_checkpointPaths[j].SetIndex(j); }
				}
			}

			if (ImGui::Button("Create Path"))
			{
				m_checkpointPaths.push_back(Path(m_checkpointPaths.size()));
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete Path"))
			{
				m_checkpointPaths.pop_back();
			}

			bool ready = !m_checkpointPaths.empty();
			for (const Path& path : m_checkpointPaths)
			{
				if (!path.IsReady()) { ready = false; break; }
			}
			ImGui::BeginDisabled(!ready);
			static ButtonUI generateButton;
			if (generateButton.Show("Generate", "Checkpoints successfully generated.", false))
			{
				GenerateCheckpoints();
			}
			ImGui::EndDisabled();
			ImGui::TreePop();
		}
		ImGui::End();
	}

	if (!checkpointQuery.empty() && !Windows::w_checkpoints) { checkpointQuery.clear(); }

	if (Windows::w_bsp)
	{
		if (ImGui::Begin("BSP Tree", &Windows::w_bsp))
		{
			if (!m_bsp.IsEmpty()) { m_bsp.RenderUI(m_quadblocks); }

			static std::string buttonMessage;
			static ButtonUI generateBSPButton = ButtonUI();
			if (ImGui::TreeNode("Advanced"))
			{
				if (ImGui::InputFloat("Max Leaf Axis Length", &m_maxLeafAxisLength)) { m_maxLeafAxisLength = std::max(m_maxLeafAxisLength, 0.0f); }
				ImGui::SetItemTooltip("Lower values improve rendering performance, but increases file size and slows down vis tree generation.");
				if (ImGui::InputFloat("Far Clip Distance", &m_distanceFarClip)) { m_distanceFarClip = std::max(m_distanceFarClip, 0.0f); }
				ImGui::SetItemTooltip("Maximum drawing distance. Lower values improve performance and speed up the vis tree generation.");
				ImGui::Checkbox("Generate Vis Tree", &m_genVisTree);
				ImGui::SetItemTooltip("Generating the vis tree may take several minutes, but the gameplay will be more performant.");
				ImGui::TreePop();
			}
			if (generateBSPButton.Show("Generate", buttonMessage, false))
			{
				if (GenerateBSP()) { buttonMessage = "Successfully generated the BSP tree."; }
				else { buttonMessage = "Failed generating the BSP tree."; }
			}
		}
		ImGui::End();
	}

	if (Windows::w_ghost)
	{
		if (ImGui::Begin("Ghost", &Windows::w_ghost))
		{
			static ButtonUI saveGhostButton(20);
			static std::string saveGhostFeedback;
			if (saveGhostButton.Show("Save Ghost", saveGhostFeedback, false))
			{
				saveGhostFeedback = "Failed retrieving ghost data from the emulator.\nMake sure that you have saved your ghost in-game\nbefore clicking this button.";

				std::string filename = "ghost";
				auto selection = pfd::save_file("CTR Ghost File", filename.c_str(), {"Ghost File (*.ctrghost)", "*.ctrghost"}).result();
				if (!selection.empty())
				{
					const std::filesystem::path path = selection + ".ctrghost";
					if (SaveGhostData("duckstation", path)) { saveGhostFeedback = "Ghost file successfully saved."; }
				}
			}

			auto ConvertTime = [](uint32_t time)
				{
					constexpr uint32_t ms = 32;
					constexpr uint32_t fps = 30;
					constexpr uint32_t second = ms * fps;
					constexpr uint32_t minute = 60 * second;

					uint32_t minutes = time / minute;
					time -= minutes * minute;

					uint32_t seconds = time / second;
					time -= seconds * second;

					uint32_t milis = (time * 1000) / second;
					return std::to_string(minutes) + ":" + std::to_string(seconds) + "." + std::to_string(milis);
				};

			static std::string tropyPath;
			static ButtonUI tropyPathButton(10);
			static std::string tropyImportFeedback;
			if (ImGui::TreeNode("Slot 1"))
			{
				ImGui::Text("Filename:"); ImGui::SameLine();
				ImGui::InputText("##tropyghost", &tropyPath, ImGuiInputTextFlags_ReadOnly); ImGui::SameLine();
				if (tropyPathButton.Show("...##tropypath", tropyImportFeedback, false))
				{
					tropyImportFeedback = "Error: invalid ghost file format.";
					auto selection = pfd::open_file("CTR Ghost File", m_parentPath.string(), {"CTR Ghost Files (*.ctrghost)", "*.ctrghost"}, pfd::opt::force_path).result();
					if (!selection.empty())
					{
						tropyPath = selection.front();
						if (SetGhostData(tropyPath, true)) { tropyImportFeedback = "Slot 1 ghost successfully set."; }
					}
				}

				if (!m_tropyGhost.empty())
				{
					uint16_t character = 0;
					uint32_t time = 0;
					memcpy(&character, &m_tropyGhost[6], sizeof(uint16_t));
					memcpy(&time, &m_tropyGhost[16], sizeof(uint32_t));

					std::string characterText = "Character: " + CTR_CHARACTERS[character];
					std::string timeText = "Time: " + ConvertTime(time);
					ImGui::Text(characterText.c_str());
					ImGui::Text(timeText.c_str());
				}
				ImGui::TreePop();
			}

			static std::string oxidePath;
			static ButtonUI oxidePathButton(10);
			static std::string oxideImportFeedback;
			if (ImGui::TreeNode("Slot 2"))
			{
				ImGui::Text("Filename:"); ImGui::SameLine();
				ImGui::InputText("##oxideghost", &oxidePath, ImGuiInputTextFlags_ReadOnly); ImGui::SameLine();
				if (oxidePathButton.Show("...##oxidepath", oxideImportFeedback, false))
				{
					oxideImportFeedback = "Error: invalid ghost file format.";
					auto selection = pfd::open_file("CTR Ghost File", m_parentPath.string(), {"CTR Ghost Files", "*.ctrghost"}, pfd::opt::force_path).result();
					if (!selection.empty())
					{
						oxidePath = selection.front();
						if (SetGhostData(oxidePath, false)) { oxideImportFeedback = "Slot 2 ghost successfully set"; }
					}
				}

				if (!m_oxideGhost.empty())
				{
					uint16_t character = 0;
					uint32_t time = 0;
					memcpy(&character, &m_oxideGhost[6], sizeof(uint16_t));
					memcpy(&time, &m_oxideGhost[16], sizeof(uint32_t));

					std::string characterText = "Character: " + CTR_CHARACTERS[character];
					std::string timeText = "Time: " + ConvertTime(time);
					ImGui::Text(characterText.c_str());
					ImGui::Text(timeText.c_str());
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	if (Windows::w_renderer)
	{
		if (ImGui::Begin("Renderer", &Windows::w_renderer))
		{
			static std::unordered_map<ImGuiKey, std::string> keyOptions;
			if (keyOptions.empty())
			{
				keyOptions.reserve(ImGuiKey_NamedKey_END - ImGuiKey_NamedKey_BEGIN);
				for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
				{
					std::string label = ImGui::GetKeyName(static_cast<ImGuiKey>(key));
					keyOptions.insert({static_cast<ImGuiKey>(key), label});
				}
				keyOptions.insert({static_cast<ImGuiKey>(ImGuiKey_ModShift), "Shift"});
				keyOptions.insert({static_cast<ImGuiKey>(ImGuiKey_ModCtrl), "Ctrl"});
				keyOptions.insert({static_cast<ImGuiKey>(ImGuiKey_ModAlt), "Alt"});
				keyOptions.insert({static_cast<ImGuiKey>(ImGuiKey_ModSuper), "Super"});
			}

			if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Shader:");
				ImGui::SameLine();
				ImGui::Combo("##Shader", &GuiRenderSettings::renderType, GuiRenderSettings::renderTypeLabels.data(), static_cast<int>(GuiRenderSettings::renderTypeLabels.size()));
				ImGui::Text("Filter:");
				ImGui::SameLine();
				ImGui::Checkbox("##Filter", &GuiRenderSettings::filterActive);
				ImGui::SameLine();
				ImGui::Text("Default Color:");
				ImGui::SameLine();
				float filterColor[3] = {GuiRenderSettings::defaultFilterColor.Red(), GuiRenderSettings::defaultFilterColor.Green(), GuiRenderSettings::defaultFilterColor.Blue()};
				if (ImGui::ColorEdit3("##FilterColor", filterColor, ImGuiColorEditFlags_NoInputs))
				{
					Color newFilterColor = Color(filterColor[0], filterColor[1], filterColor[2]);
					for (Quadblock& quadblock : m_quadblocks)
					{
						if (quadblock.GetFilterColor() != GuiRenderSettings::defaultFilterColor) { continue; }
						quadblock.SetFilterColor(newFilterColor);
						UpdateFilterRenderData(quadblock);
					}
					GuiRenderSettings::defaultFilterColor = newFilterColor;
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset Filter")) { ResetFilter(); }
				ImGui::Text("Flags:");
				if (ImGui::BeginTable("Renderer Flags", 2, ImGuiTableFlags_SizingStretchSame))
				{
					constexpr unsigned REND_FLAGS_NONE = 0;
					constexpr unsigned REND_FLAGS_COLUMN_0 = 1;
					constexpr unsigned REND_FLAGS_COLUMN_1 = 2;
					auto checkboxPair = [](const char* leftLabel, bool* leftValue, const char* rightLabel, bool* rightValue) -> unsigned
						{
							unsigned ret = REND_FLAGS_NONE;
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							if (leftValue && ImGui::Checkbox(leftLabel, leftValue)) { ret |= REND_FLAGS_COLUMN_0; }
							ImGui::TableSetColumnIndex(1);
							if (rightValue && ImGui::Checkbox(rightLabel, rightValue)) { ret |= REND_FLAGS_COLUMN_1; }
							return ret;
						};

					checkboxPair("Show Low LOD", &GuiRenderSettings::showLowLOD, "Show Wireframe", &GuiRenderSettings::showWireframe);
					checkboxPair("Show Backfaces", &GuiRenderSettings::showBackfaces, "Show Level Verts", &GuiRenderSettings::showLevVerts);
					unsigned cpStartPoints = checkboxPair("Show Checkpoints", &GuiRenderSettings::showCheckpoints, "Show Starting Positions", &GuiRenderSettings::showStartpoints);
					if (cpStartPoints & REND_FLAGS_COLUMN_1) { GenerateRenderStartpointData(m_spawn); }
					checkboxPair("Show BSP", &GuiRenderSettings::showBspRectTree, "Show Vis Tree", &GuiRenderSettings::showVisTree);
					unsigned minimapBoundsChanged = checkboxPair("Show Minimap Bounds", &GuiRenderSettings::showMinimapBounds, "", nullptr);
					if (minimapBoundsChanged & REND_FLAGS_COLUMN_0) { GenerateRenderMinimapBoundsData(); }

					ImGui::EndTable();
				}

				ImGui::Text("BSP Depth:");
				ImGui::BeginDisabled(!GuiRenderSettings::showBspRectTree);
				if (ImGui::BeginTable("BSP Depth", 2, ImGuiTableFlags_SizingStretchSame))
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::SliderInt("Top", &GuiRenderSettings::bspTreeTopDepth, 0, GuiRenderSettings::bspTreeMaxDepth)) //top changed
					{
						GuiRenderSettings::bspTreeBottomDepth = std::max(GuiRenderSettings::bspTreeBottomDepth, GuiRenderSettings::bspTreeTopDepth);
						GenerateRenderBspData(m_bsp);
					}
					ImGui::TableSetColumnIndex(1);
					if (ImGui::SliderInt("Bottom", &GuiRenderSettings::bspTreeBottomDepth, 0, GuiRenderSettings::bspTreeMaxDepth)) //bottom changed
					{
						GuiRenderSettings::bspTreeTopDepth = std::min(GuiRenderSettings::bspTreeTopDepth, GuiRenderSettings::bspTreeBottomDepth);
						GenerateRenderBspData(m_bsp);
					}
					ImGui::EndTable();
				}
				ImGui::EndDisabled();

				ImGui::Text("Camera:");
				if (ImGui::BeginTable("Renderer Inputs", 2, ImGuiTableFlags_SizingStretchSame))
				{
					auto inputPair = [](const char* leftLabel, float& leftValue, float leftMin, float leftMax,
						const char* rightLabel, float& rightValue, float rightMin, float rightMax)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							if (leftLabel) { if (ImGui::InputFloat(leftLabel, &leftValue)) { leftValue = Clamp(leftValue, leftMin, leftMax); } }
							else { ImGui::Dummy(ImVec2(0.0f, 0.0f)); }
							ImGui::TableSetColumnIndex(1);
							if (rightLabel) { if (ImGui::InputFloat(rightLabel, &rightValue)) { rightValue = Clamp(rightValue, rightMin, rightMax); } }
							else { ImGui::Dummy(ImVec2(0.0f, 0.0f)); }
						};

					inputPair("Move Mult", GuiRenderSettings::camMoveMult, 0.0f, std::numeric_limits<float>::max(),
						"Rotate Mult", GuiRenderSettings::camRotateMult, 0.0f, std::numeric_limits<float>::max());
					inputPair("Zoom Mult", GuiRenderSettings::camZoomMult, 0.0f, std::numeric_limits<float>::max(),
						"Sprint Mult", GuiRenderSettings::camSprintMult, 0.0f, std::numeric_limits<float>::max());
					float dummy;
					inputPair("FOV", GuiRenderSettings::camFovDeg, 5.0f, 150.0f, nullptr, dummy, 0.0f, 0.0f);
					ImGui::EndTable();
				}

				ImGui::Text("Camera Bindings:");
				auto DrawKeyRow = [](const char* label, int& keyValue)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(label);
						ImGui::TableSetColumnIndex(1);
						ImGuiKey currentKey = static_cast<ImGuiKey>(keyValue);
						std::string preview = keyOptions[currentKey];
						std::string comboId = std::string("##") + label;
						if (ImGui::BeginCombo(comboId.c_str(), preview.c_str()))
						{
							for (const auto& entry : keyOptions)
							{
								bool selected = (entry.first == currentKey);
								if (ImGui::Selectable(entry.second.c_str(), selected))
								{
									keyValue = entry.first;
								}
								if (selected) { ImGui::SetItemDefaultFocus(); }
							}
							ImGui::EndCombo();
						}
					};

				auto DrawMouseRow = [](const char* label, int& buttonValue)
					{
						constexpr std::pair<int, const char*> mouseOptions[] = {
							{ImGuiMouseButton_Left, "Left"},
							{ImGuiMouseButton_Right, "Right"},
							{ImGuiMouseButton_Middle, "Middle"},
						};

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(label);
						ImGui::TableSetColumnIndex(1);
						std::string comboId = std::string("##") + label;
						if (ImGui::BeginCombo(comboId.c_str(), mouseOptions[buttonValue].second))
						{
							for (const auto& entry : mouseOptions)
							{
								bool selected = (entry.first == buttonValue);
								if (ImGui::Selectable(entry.second, selected))
								{
									buttonValue = entry.first;
								}
								if (selected) { ImGui::SetItemDefaultFocus(); }
							}
							ImGui::EndCombo();
						}
					};

				if (ImGui::BeginTable("Camera Bindings Table", 2, ImGuiTableFlags_SizingStretchSame))
				{
					DrawMouseRow("Orbit/Drag Mouse Button", GuiRenderSettings::camOrbitMouseButton);
					DrawKeyRow("Forward", GuiRenderSettings::camKeyForward);
					DrawKeyRow("Back", GuiRenderSettings::camKeyBack);
					DrawKeyRow("Left", GuiRenderSettings::camKeyLeft);
					DrawKeyRow("Right", GuiRenderSettings::camKeyRight);
					DrawKeyRow("Up", GuiRenderSettings::camKeyUp);
					DrawKeyRow("Down", GuiRenderSettings::camKeyDown);
					DrawKeyRow("Sprint", GuiRenderSettings::camKeySprint);
					ImGui::EndTable();
				}
				ImGui::TreePop();
			}

			ImGui::Separator();
			ImGui::Checkbox("Show Selected Quadblock Info", &GuiRenderSettings::showSelectedQuadblockInfo);
			ImGui::Separator();
			ImGui::NewLine();

			static size_t prevSelectedQuadblock = REND_NO_SELECTED_QUADBLOCK;
			if (!m_rendererSelectedQuadblockIndexes.empty() && GuiRenderSettings::showSelectedQuadblockInfo)
			{
				size_t currentIndex = m_rendererSelectedQuadblockIndexes.back();
				if (currentIndex < m_quadblocks.size())
				{
					Quadblock& quadblock = m_quadblocks[currentIndex];
					bool resetBsp = false;
					if (prevSelectedQuadblock != currentIndex)
					{
						prevSelectedQuadblock = currentIndex;
						ImGui::SetNextItemOpen(true);
					}
					if (quadblock.RenderUI(m_checkpoints.size() - 1, resetBsp))
					{
						ManageTurbopad(quadblock);
					}
					if (resetBsp && m_bsp.IsValid())
					{
						m_bsp.Clear();
						GenerateRenderBspData(m_bsp);
					}
				}
			}
		}
		ImGui::End();
	}

	if (Windows::w_python)
	{
		ImGui::SetNextWindowSize(ImVec2(600.0f, 320.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Python", &Windows::w_python))
		{
			ImGui::Text("Script Editor");
			ImGui::SameLine();
			if (ImGui::Button("Open .py"))
			{
				auto selection = pfd::open_file("Open Python Script", m_parentPath.string(), {"Python Files", "*.py"}, pfd::opt::force_path).result();
				if (!selection.empty())
				{
					std::ifstream input(selection.front(), std::ios::binary);
					if (input)
					{
						std::ostringstream buffer;
						buffer << input.rdbuf();
						m_pythonScript = buffer.str();
					}
				}
			}
			ImGui::Separator();
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			const float editorHeight = avail.y * 0.7f;
			const float consoleHeight = std::max(0.0f, avail.y - editorHeight - ImGui::GetFrameHeightWithSpacing() * 2.0f);
			ImVec2 editorSize = ImVec2(avail.x, editorHeight);
			ImGui::InputTextMultiline("##python_editor", &m_pythonScript, editorSize, ImGuiInputTextFlags_AllowTabInput);

			if (ImGui::Button("Run"))
			{
				m_saveScript = true;
				m_pythonConsole.clear();
				std::string result = Script::ExecutePythonScript(*this, m_pythonScript);
				if (result.empty()) { result = "[No output]"; }
				if (!m_pythonConsole.empty() && m_pythonConsole.back() != '\n')
				{
					m_pythonConsole += '\n';
				}
				m_pythonConsole += result;
				if (m_pythonConsole.back() != '\n') { m_pythonConsole += '\n'; }
			}
			ImGui::SameLine();
			static bool displayHelper = true;
			if (ImGui::Button("Clear Console")) { m_pythonConsole.clear(); displayHelper = false; }
			ImGui::SameLine();
			if (ImGui::Button("Copy to Clipboard"))
			{
				if (!m_pythonConsole.empty())
				{
					ImGui::SetClipboardText(m_pythonConsole.c_str());
				}
			}

			ImGui::Separator();
			ImGui::Text("Console Output:");
			ImGui::BeginChild("##python_console", ImVec2(0.0f, consoleHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
			if (displayHelper && m_pythonConsole.empty())
			{
				ImGui::TextUnformatted("Console output will appear here.");
			}
			else
			{
				ImGui::TextUnformatted(m_pythonConsole.c_str());
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}

void Path::RenderUI(const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, bool& insertAbove, bool& removePath, const std::vector<size_t>& selectedIndexes, bool mainPath)
{
	auto QuadListUI = [this, &selectedIndexes](std::vector<size_t>& indexes, size_t& value, std::string& label, const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, ButtonUI& button)
		{
			if (ImGui::BeginChild(title.c_str(), {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX))
			{
				ImGui::Text(title.substr(0, title.find("##")).c_str());
				if (ImGui::TreeNode("Quad list:"))
				{
					std::vector<size_t> deleteList;
					for (size_t i = 0; i < indexes.size(); i++)
					{
						ImGui::Text(quadblocks[indexes[i]].GetName().c_str()); ImGui::SameLine();
						if (ImGui::Button(("Remove##" + title + std::to_string(i)).c_str()))
						{
							deleteList.push_back(i);
						}
					}
					if (!deleteList.empty())
					{
						for (int i = static_cast<int>(deleteList.size()) - 1; i >= 0; i--)
						{
							indexes.erase(indexes.begin() + deleteList[i]);
						}
					}
					ImGui::TreePop();
				}

				if (ImGui::BeginCombo(("##" + title).c_str(), label.c_str()))
				{
					for (size_t i = 0; i < quadblocks.size(); i++)
					{
						if (Matches(quadblocks[i].GetName(), searchQuery))
						{
							if (ImGui::Selectable(quadblocks[i].GetName().c_str()))
							{
								label = quadblocks[i].GetName();
								value = i;
							}
						}
					}
					ImGui::EndCombo();
				}

				auto AppendIndex = [&indexes](size_t value)
					{
						bool found = false;
						for (const size_t index : indexes)
						{
							if (index == value) { found = true; break; }
						}
						if (!found) { indexes.push_back(value); }
					};

				if (button.Show(("Add##" + title).c_str(), "Quadblock successfully\nadded to path.", false))
				{
					AppendIndex(value);
				}

				ImGui::SameLine();
				if (ImGui::Button(("Add Selected##" + title).c_str()))
				{
					for (size_t index : selectedIndexes) { AppendIndex(index); }
				}
			}
			ImGui::EndChild();
		};

	bool popColor = false;
	if (mainPath)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(m_color.Red(), m_color.Green(), m_color.Blue(), 1.0f));
		popColor = true;
	}
	if (ImGui::TreeNode(title.c_str()))
	{
		if (mainPath)
		{
			ImGui::Text("Color:"); ImGui::SameLine();
			float color[3] = {m_color.Red(), m_color.Green(), m_color.Blue()};
			if (ImGui::ColorEdit3(("##color" + title).c_str(), color))
			{
				m_color = Color(color[0], color[1], color[2]);
				if (m_left) { m_left->SetColor(m_color); }
				if (m_right) { m_right->SetColor(m_color); }
			}
			ImGui::PopStyleColor();
			popColor = false;
		}
		if (ImGui::BeginChild(("##" + title).c_str(), {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX))
		{
			bool dummyInsert, dummyRemove = false;
			if (m_left) { m_left->RenderUI("Left Path", quadblocks, searchQuery, dummyInsert, dummyRemove, selectedIndexes, false); }
			if (m_right) { m_right->RenderUI("Right Path", quadblocks, searchQuery, dummyInsert, dummyRemove, selectedIndexes, false); }

			static ButtonUI startButton = ButtonUI();
			static ButtonUI endButton = ButtonUI();
			static ButtonUI ignoreButton = ButtonUI();
			QuadListUI(m_quadIndexesStart, m_previewValueStart, m_previewLabelStart, "Start##" + title, quadblocks, searchQuery, startButton);
			ImGui::SameLine();
			QuadListUI(m_quadIndexesEnd, m_previewValueEnd, m_previewLabelEnd, "End##" + title, quadblocks, searchQuery, endButton);
			ImGui::SameLine();
			QuadListUI(m_quadIndexesIgnore, m_previewValueIgnore, m_previewLabelIgnore, "Ignore##" + title, quadblocks, searchQuery, ignoreButton);

			if (ImGui::Button("Add Left Path"))
			{
				if (!m_left)
				{
					m_left = new Path(m_index + 1);
					m_left->SetColor(m_color);
				}
			} ImGui::SameLine();
			ImGui::BeginDisabled(m_left == nullptr);
			if (ImGui::Button("Delete Left Path"))
			{
				if (m_left)
				{
					delete m_left;
					m_left = nullptr;
				}
			}
			ImGui::EndDisabled();

			if (ImGui::Button("Add Right Path"))
			{
				if (!m_right)
				{
					m_right = new Path(m_index + 2);
					m_right->SetColor(m_color);
				}
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(m_right == nullptr);
			if (ImGui::Button("Delete Right Path"))
			{
				if (m_right)
				{
					delete m_right;
					m_right = nullptr;
				}
			}
			ImGui::EndDisabled();
		}
		ImGui::EndChild();

		if (mainPath)
		{
			static ButtonUI insertAboveButton;
			static ButtonUI removePathButton;
			if (insertAboveButton.Show(("Insert Path Above##" + std::to_string(m_index)).c_str(), "You're editing the new path.", false)) { insertAbove = true; }
			if (removePathButton.Show(("Remove Current Path##" + std::to_string(m_index)).c_str(), "Path successfully deleted.", false)) { removePath = true; }
		}

		ImGui::TreePop();
	}
	if (popColor) { ImGui::PopStyleColor(); }
}

bool Quadblock::RenderUI(size_t checkpointCount, bool& resetBsp)
{
	bool ret = false;
	if (ImGui::TreeNode(m_name.c_str()))
	{
		if (ImGui::TreeNode("Vertices"))
		{
			for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
			{
				bool editedPos = false;
				m_p[i].RenderUI(i, editedPos);
				if (editedPos)
				{
					resetBsp = true;
					ComputeBoundingBox();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Bounding Box"))
		{
			m_bbox.RenderUI();
			ImGui::TreePop();
		}
		if (!m_texPath.empty() && ImGui::TreeNode("Texture"))
		{
			std::string texPath = m_texPath.string();
			ImGui::Text("Path:"); ImGui::SameLine();
			ImGui::BeginDisabled();
			ImGui::InputText("##texpath", &texPath, ImGuiInputTextFlags_ReadOnly);
			ImGui::EndDisabled();
			ImGui::Text("UVs:");
			for (size_t i = 0; i < NUM_FACES_QUADBLOCK + 1; i++)
			{
				std::string title = i == NUM_FACES_QUADBLOCK ? "Low Quad" : "Quad " + std::to_string(i);
				if (ImGui::TreeNode(title.c_str()))
				{
					ImGui::InputFloat2("Top left:", &m_uvs[i][0].x, "%.2f");
					ImGui::InputFloat2("Top right:", &m_uvs[i][1].x, "%.2f");
					ImGui::InputFloat2("Bottom left:", &m_uvs[i][2].x, "%.2f");
					ImGui::InputFloat2("Bottom right:", &m_uvs[i][3].x, "%.2f");
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Terrain"))
		{
			std::string terrainLabel;
			for (const auto& [label, terrain] : TerrainType::LABELS)
			{
				if (terrain == m_terrain) { terrainLabel = label; break; }
			}
			if (ImGui::BeginCombo("##terrain", terrainLabel.c_str()))
			{
				for (const auto& [label, terrain] : TerrainType::LABELS)
				{
					if (ImGui::Selectable(label.c_str()))
					{
						m_terrain = terrain;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Quad Flags"))
		{
			for (const auto& [label, flag] : QuadFlags::LABELS)
			{
				UIFlagCheckbox(m_flags, flag, label);
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Draw Flags"))
		{
			ImGui::Checkbox("Double Sided", &m_doubleSided);
			const std::vector<std::string> s_rotateFlip = {"None", "Rotate 90", "Rotate 180", "Rotate -90", "Flip + Rotate 90", "Flip + Rotate 180", "Flip + Rotate -90", "Flip"};
			const std::vector<std::string> s_faceDrawMode = {"Both", "Left", "Right", "None"};

			auto UISelectable = [](size_t quadIndex, const std::string& label, const std::vector<std::string>& options, uint32_t* data)
				{
					if (ImGui::BeginCombo((label + "##" + std::to_string(quadIndex)).c_str(), options[data[quadIndex]].c_str()))
					{
						for (size_t i = 0; i < options.size(); i++)
						{
							if (ImGui::Selectable(options[i].c_str()))
							{
								data[quadIndex] = static_cast<int>(i);
							}
						}
						ImGui::EndCombo();
					}
				};
			for (size_t i = 0; i < NUM_FACES_QUADBLOCK; i++)
			{
				ImGui::Text(("Face " + std::to_string(i)).c_str());
				UISelectable(i, "Rotate Flip", s_rotateFlip, m_faceRotateFlip);
				UISelectable(i, "Draw Mode", s_faceDrawMode, m_faceDrawMode);
			}

			ImGui::TreePop();
		}
		ImGui::Text("Downforce:");
		ImGui::SameLine();
		if (ImGui::InputInt("##downforceQuad", &m_downforce)) { m_downforce = Clamp(m_downforce, static_cast<int>(INT8_MIN), static_cast<int>(INT8_MAX)); }
		ImGui::Checkbox("Checkpoint", &m_checkpointStatus);
		ImGui::SameLine();
		ImGui::Checkbox("Checkpoint Pathable", &m_checkpointPathable);
		ImGui::Text("Checkpoint Index: ");
		ImGui::SameLine();
		if (ImGui::InputInt("##cp", &m_checkpointIndex)) { m_checkpointIndex = Clamp(m_checkpointIndex, -1, static_cast<int>(checkpointCount)); }
		ImGui::Text("Trigger:");
		if (ImGui::RadioButton("None", m_trigger == QuadblockTrigger::NONE))
		{
			m_trigger = QuadblockTrigger::NONE;
			m_flags = QuadFlags::DEFAULT;
			resetBsp = true;
			ret = true;
		} ImGui::SameLine();
		if (ImGui::RadioButton("Turbo Pad", m_trigger == QuadblockTrigger::TURBO_PAD))
		{
			m_trigger = QuadblockTrigger::TURBO_PAD;
			resetBsp = true;
			ret = true;
		} ImGui::SameLine();
		if (ImGui::RadioButton("Super Turbo Pad", m_trigger == QuadblockTrigger::SUPER_TURBO_PAD))
		{
			m_trigger = QuadblockTrigger::SUPER_TURBO_PAD;
			resetBsp = true;
			ret = true;
		}
		ImGui::TreePop();
	}
	return ret;
}

void Vertex::RenderUI(size_t index, bool& editedPos)
{
	if (ImGui::TreeNode(("Vertex " + std::to_string(index)).c_str()))
	{
		ImGui::Text("Pos: "); ImGui::SameLine();
		if (ImGui::InputFloat3("##pos", m_pos.Data())) { editedPos = true; }
		ImGui::Text("High:"); ImGui::SameLine();
		float colorHighData[3] = {m_colorHigh.Red(), m_colorHigh.Green(), m_colorHigh.Blue()};
		if (ImGui::ColorEdit3("##high", colorHighData))
		{
			m_colorHigh = Color(static_cast<float>(colorHighData[0]), colorHighData[1], colorHighData[2]);
		}
		ImGui::Text("Low: "); ImGui::SameLine();
		float colorLowData[3] = {m_colorLow.Red(), m_colorLow.Green(), m_colorLow.Blue()};
		if (ImGui::ColorEdit3("##low", colorLowData))
		{
			m_colorLow = Color(static_cast<float>(colorLowData[0]), colorLowData[1], colorLowData[2]);
		}
		ImGui::TreePop();
	}
}

void Texture::RenderUI(const std::vector<size_t>& quadblockIndexes, std::vector<Quadblock>& quadblocks, std::function<void(void)> refreshTextureStores)
{
	std::string texPath = GetPath().string();
	if (ImGui::TreeNode(("Texture##" + texPath).c_str()))
	{
		ImGui::Text("Path:"); ImGui::SameLine();
		ImGui::BeginDisabled();
		ImGui::InputText("##texpath", &texPath, ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();
		ImGui::SetItemTooltip(texPath.c_str());
		ImGui::SameLine();
		if (ImGui::Button("..."))
		{
			auto selection = pfd::open_file("Texture File", ".", {"Texture Files", "*.bmp, *.jpeg, *.jpg, *.png"}).result();
			if (!selection.empty())
			{
				const std::filesystem::path& newTexPath = selection.front();
				UpdateTexture(newTexPath);
				refreshTextureStores();
				for (const size_t index : quadblockIndexes) { quadblocks[index].SetTexPath(newTexPath); }
			}
		}
		if (IsEmpty()) { ImGui::TreePop(); return; }
		constexpr size_t NUM_BLEND_MODES = 4;
		const std::array<std::string, NUM_BLEND_MODES> BLEND_MODES = {"Half Transparent", "Additive", "Subtractive", "Additive Translucent"};
		uint16_t blendMode = GetBlendMode();
		ImGui::Text("Blend Mode:"); ImGui::SameLine();
		if (ImGui::BeginCombo("##blendmode", BLEND_MODES[blendMode].c_str()))
		{
			for (size_t i = 0; i < NUM_BLEND_MODES; i++)
			{
				if (ImGui::Selectable(BLEND_MODES[i].c_str()))
				{
					SetBlendMode(static_cast<uint16_t>(i));
				}
			}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}
}

void Texture::RenderUI()
{
	std::vector<size_t> dummyIndexes;
	std::vector<Quadblock> dummyQuadblocks;
	RenderUI(dummyIndexes, dummyQuadblocks, []() {});
}

bool AnimTexture::RenderUI(std::vector<std::string>& animTexNames, std::vector<Quadblock>& quadblocks, const std::unordered_map<std::string, std::vector<size_t>>& materialMap, const std::string& query, std::vector<AnimTexture>& newTextures)
{
	bool ret = true;
	if (ImGui::TreeNode(m_name.c_str()))
	{
		if (ImGui::TreeNode("Quadblocks"))
		{
			constexpr size_t QUADS_PER_LINE = 10;
			for (size_t i = 0; i < m_quadblockIndexes.size(); i++)
			{
				ImGui::Text((quadblocks[m_quadblockIndexes[i]].GetName() + ", ").c_str());
				if (((i + 1) % QUADS_PER_LINE) == 0 || i == m_quadblockIndexes.size() - 1) { continue; }
				ImGui::SameLine();
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Settings"))
		{
			size_t frames = m_frames.size();
			ImGui::Text(("Frames: " + std::to_string(frames)).c_str());
			ImGui::Text("Start at Frame:"); ImGui::SameLine();
			ImGui::SliderInt("##startat", &m_startAtFrame, 0, static_cast<int>(frames));
			float durationMS = (1.0f + static_cast<float>(m_duration)) / 30.0f;
			ImGui::Text("Duration per Frame:"); ImGui::SameLine();
			ImGui::InputInt("##duration", &m_duration);
			m_duration = std::max(m_duration, 0);
			std::stringstream ss;
			ss << std::fixed << std::setprecision(3) << durationMS;
			ImGui::Text(("Duration per Frame: " + ss.str() + "s").c_str());

			ImGui::Text("Enable manual rotation:"); ImGui::SameLine();
			ImGui::Checkbox("##manualrot", &m_manualOrientation);
			ImGui::SetItemTooltip("When manual rotation is disabled,\nthe editor will try to find the matching quadblock direction\nbased on the UV coordinates of the original quadblock.");
			ImGui::BeginDisabled(!m_manualOrientation);
			ImGui::Text("Rotation:"); ImGui::SameLine();
			if (ImGui::RadioButton("0 deg", m_rotation == 0))
			{
				RotateFrames(0 - m_rotation);
				m_rotation = 0;
			} ImGui::SameLine();
			if (ImGui::RadioButton("90 deg", m_rotation == 90))
			{
				RotateFrames(90 - m_rotation);
				m_rotation = 90;
			} ImGui::SameLine();
			if (ImGui::RadioButton("180 deg", m_rotation == 180))
			{
				RotateFrames(180 - m_rotation);
				m_rotation = 180;
			} ImGui::SameLine();
			if (ImGui::RadioButton("270 deg", m_rotation == 270))
			{
				RotateFrames(270 - m_rotation);
				m_rotation = 270;
			}
			ImGui::Text("Mirror:"); ImGui::SameLine();
			ImGui::Checkbox("Horizontal", &m_horMirror); ImGui::SameLine();
			ImGui::Checkbox("Vertical", &m_verMirror);
			ImGui::EndDisabled();
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Textures"))
		{
			for (Texture& tex : m_textures) { tex.RenderUI(); }
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Manage"))
		{
			ImGui::Text("Select Quadblock:");
			if (ImGui::BeginCombo("##quadcombo", m_previewQuadName.c_str()))
			{
				for (size_t i = 0; i < quadblocks.size(); i++)
				{
					const Quadblock& quadblock = quadblocks[i];
					if (!quadblock.GetHide() && Matches(quadblock.GetName(), query) && ImGui::Selectable(quadblock.GetName().c_str()))
					{
						m_previewQuadName = quadblock.GetName();
						m_previewQuadIndex = i;
					}
				}
				ImGui::EndCombo();
			}

			auto FindBestOrientation = [this](std::array<QuadUV, 5>& animUVs, const std::array<QuadUV, 5>& quadUVs) -> uint32_t
				{
					auto FindBestRotation = [this](std::array<QuadUV, 5>& animUVs, const std::array<QuadUV, 5>& quadUVs) -> std::tuple<uint32_t, float>
						{
							auto MeanSquareErrorUVs = [](const std::array<QuadUV, 5>& src, const std::array<QuadUV, 5>& tgt) -> float
								{
									float mse = 0.0f;
									for (size_t i = 0; i < 4; i++)
									{
										const QuadUV& srcUV = src[i];
										const QuadUV& tgtUV = tgt[i];
										for (size_t j = 0; j < 4; j++)
										{
											mse += ((srcUV[j].x - tgtUV[j].x) * (srcUV[j].x - tgtUV[j].x)) + ((srcUV[j].y - tgtUV[j].y) * (srcUV[j].y - tgtUV[j].y));
										}
									}
									return mse;
								};

							float bestMSE = std::numeric_limits<float>::max();
							uint32_t bestRotation = 0;
							for (size_t i = 0; i < 4; i++)
							{
								float mse = MeanSquareErrorUVs(quadUVs, animUVs);
								if (mse < bestMSE)
								{
									bestMSE = mse;
									bestRotation = static_cast<uint32_t>(i);
								}
								RotateQuadUV(animUVs);
							}

							return {bestRotation, bestMSE};
						};

					std::tuple<uint32_t, float> bestOrientation = FindBestRotation(animUVs, quadUVs);

					MirrorQuadUV(true, animUVs);
					std::tuple<uint32_t, float> currOrientation = FindBestRotation(animUVs, quadUVs);
					if (std::get<float>(currOrientation) < std::get<float>(bestOrientation))
					{
						std::get<float>(bestOrientation) = std::get<float>(currOrientation);
						std::get<uint32_t>(bestOrientation) = std::get<uint32_t>(currOrientation) | 4u;
					}
					MirrorQuadUV(true, animUVs);

					MirrorQuadUV(false, animUVs);
					currOrientation = FindBestRotation(animUVs, quadUVs);
					if (std::get<float>(currOrientation) < std::get<float>(bestOrientation))
					{
						std::get<float>(bestOrientation) = std::get<float>(currOrientation);
						std::get<uint32_t>(bestOrientation) = std::get<uint32_t>(currOrientation) | 8u;
					}
					MirrorQuadUV(false, animUVs);

					return std::get<uint32_t>(bestOrientation);
				};

			static ButtonUI applyQuadBtn;
			bool found = m_previewQuadName.empty();
			if (!found)
			{
				for (size_t index : m_quadblockIndexes) { if (index == m_previewQuadIndex) { found = true; break; } }
			}
			if (applyQuadBtn.Show("Add", "Animation successfully added to quadblock.", !found) && !found)
			{
				if (!m_manualOrientation)
				{
					const std::array<QuadUV, 5>& quadUVs = quadblocks[m_previewQuadIndex].GetUVs();
					std::array<QuadUV, 5>& animUVs = m_frames[m_startAtFrame].uvs;
					uint32_t bestOrientation = FindBestOrientation(animUVs, quadUVs);
					if (bestOrientation == 0) { m_quadblockIndexes.push_back(m_previewQuadIndex); }
					else
					{
						AnimTexture newTex = AnimTexture(m_path, animTexNames);
						newTex.CopyParameters(*this);

						bool horMirror = (bestOrientation & 4) == 4;
						if (horMirror)
						{
							newTex.MirrorFrames(true);
							newTex.m_horMirror = !m_horMirror;
						}

						bool verMirror = (bestOrientation & 8) == 8;
						if (verMirror)
						{
							newTex.MirrorFrames(false);
							newTex.m_verMirror = !m_verMirror;
						}

						uint32_t rotation = bestOrientation & 0b11;
						int totalRotation = static_cast<int>(rotation) * 90;
						newTex.RotateFrames(totalRotation);
						newTex.m_rotation = (m_rotation + totalRotation) % 360;

						newTex.m_quadblockIndexes.push_back(m_previewQuadIndex);
						newTex.m_previewQuadName = m_previewQuadName;
						newTex.m_previewQuadIndex = m_previewQuadIndex;
						newTextures.push_back(newTex);
					}
				}
				else { m_quadblockIndexes.push_back(m_previewQuadIndex); }
				quadblocks[m_previewQuadIndex].SetAnimated(true);
			}

			static ButtonUI remQuadBtn;
			if (remQuadBtn.Show("Remove##quadblock", "Animation successfully removed from quadblock.", false))
			{
				auto it = m_quadblockIndexes.begin();
				for (; it != m_quadblockIndexes.end(); it++)
				{
					if (*it == m_previewQuadIndex)
					{
						quadblocks[m_previewQuadIndex].SetAnimated(false);
						m_quadblockIndexes.erase(it);
						break;
					}
				}
			}

			ImGui::Text("Apply by Material:");
			if (ImGui::BeginCombo("##matcombo", m_previewMaterialName.c_str()))
			{
				for (const auto& [material, indexes] : materialMap)
				{
					if (Matches(material, query) && ImGui::Selectable(material.c_str()))
					{
						m_previewMaterialName = material;
					}
				}
				ImGui::EndCombo();
			}

			static ButtonUI applyMatBtn;
			found = m_previewMaterialName.empty();
			if (!found) { found = m_previewMaterialName == m_lastAppliedMaterialName; }
			if (applyMatBtn.Show("Apply", "Animation successfully applied to all material quadblocks", !found) && !found)
			{
				std::unordered_map<uint32_t, AnimTexture> newAnims = {};
				std::array<QuadUV, 5>& animUVs = m_frames[m_startAtFrame].uvs;
				for (const auto& [material, indexes] : materialMap)
				{
					if (m_previewMaterialName == material)
					{
						for (const size_t index : indexes)
						{
							if (!m_manualOrientation)
							{
								const std::array<QuadUV, 5>& quadUVs = quadblocks[index].GetUVs();
								uint32_t bestOrientation = FindBestOrientation(animUVs, quadUVs);
								if (bestOrientation == 0) { m_quadblockIndexes.push_back(index); }
								else
								{
									if (newAnims.contains(bestOrientation)) { newAnims[bestOrientation].m_quadblockIndexes.push_back(index); }
									else
									{
										AnimTexture newTex = AnimTexture(m_path, animTexNames);
										animTexNames.push_back(newTex.GetName());
										newTex.CopyParameters(*this);

										if (bestOrientation & 4)
										{
											newTex.MirrorFrames(true);
											newTex.m_horMirror = !m_horMirror;
										}

										if (bestOrientation & 8)
										{
											newTex.MirrorFrames(false);
											newTex.m_verMirror = !m_verMirror;
										}

										uint32_t rotation = bestOrientation & 0b11;
										int totalRotation = static_cast<int>(rotation) * 90;
										newTex.RotateFrames(totalRotation);
										newTex.m_rotation = (m_rotation + totalRotation) % 360;

										newTex.m_lastAppliedMaterialName = m_lastAppliedMaterialName;
										newTex.m_quadblockIndexes.push_back(index);
										newAnims[bestOrientation] = newTex;
									}
								}
							}
							else { m_quadblockIndexes.push_back(index); }
							quadblocks[index].SetAnimated(true);
						}
						if (!newAnims.empty())
						{
							for (const auto& [rot, newAnim] : newAnims)
							{
								newTextures.push_back(newAnim);
							}
						}
						m_lastAppliedMaterialName = m_previewMaterialName;
						break;
					}
				}
			}

			static ButtonUI remMatBtn;
			if (remMatBtn.Show("Remove##material", "Animation successfully removed from material.", false))
			{
				std::vector<std::vector<size_t>::iterator> remList;
				for (const auto& [material, indexes] : materialMap)
				{
					if (m_previewMaterialName == material)
					{
						auto it = m_quadblockIndexes.begin();
						for (; it != m_quadblockIndexes.end(); it++)
						{
							for (const size_t index : indexes)
							{
								if (*it == index)
								{
									quadblocks[index].SetAnimated(false);
									remList.push_back(it);
								}
							}
						}
						break;
					}
				}
				for (int i = static_cast<int>(remList.size()) - 1; i >= 0; i--)
				{
					m_quadblockIndexes.erase(remList[i]);
				}
			}

			ImGui::TreePop();
		}
		if (ImGui::Button("Delete Animated Texture"))
		{
			ret = false;
		}
		ImGui::TreePop();
	}
	return ret;
}
