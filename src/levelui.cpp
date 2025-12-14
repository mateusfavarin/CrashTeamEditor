#include "geo.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "path.h"
#include "quadblock.h"
#include "vertex.h"
#include "utils.h"
#include "renderer.h"
#include "gui_render_settings.h"
#include "mesh.h"
#include "texture.h"
#include "ui.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <portable-file-dialogs.h>

#include <string>
#include <chrono>
#include <functional>
#include <cmath>
#include <sstream>

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
					ImGui::Text("Pos:"); ImGui::SameLine(); ImGui::InputFloat3("##pos", m_spawn[i].pos.Data());
					ImGui::Text("Rot:"); ImGui::SameLine();
					if (ImGui::InputFloat3("##rot", m_spawn[i].rot.Data()))
					{
						m_spawn[i].rot.x = Clamp(m_spawn[i].rot.x, -360.0f, 360.0f);
						m_spawn[i].rot.y = Clamp(m_spawn[i].rot.y, -360.0f, 360.0f);
						m_spawn[i].rot.z = Clamp(m_spawn[i].rot.z, -360.0f, 360.0f);
					};
					GenerateRenderStartpointData(m_spawn);
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
					if (m_propTurboPads.RenderUI(material, quadblockIndexes, m_quadblocks))
					{
						for (size_t index : quadblockIndexes) { ManageTurbopad(m_quadblocks[index]); }
						if (m_bsp.Valid())
						{
							m_bsp.Clear();
							GenerateRenderBspData(m_bsp);
						}
					}
					m_propSpeedImpact.RenderUI(material, quadblockIndexes, m_quadblocks);

					if (m_materialToTexture.contains(material))
					{
						m_materialToTexture[material].RenderUI(quadblockIndexes, m_quadblocks, [&]() {
							this->RefreshTextureStores();
							this->GenerateRenderLevData();
							});
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
					if (!animTex.Empty()) { m_animTextures.push_back(animTex); errorLoadingAnim.clear(); }
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
				if (!quadblock.Hide() && Matches(quadblock.GetName(), quadblockQuery))
				{
					if (quadblock.RenderUI(m_checkpoints.size() - 1, resetBsp))
					{
						ManageTurbopad(quadblock);
					}
				}
			}
		}
		ImGui::End();
		if (resetBsp && m_bsp.Valid())
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
						m_checkpoints[i].UpdateIndex(i);
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
				path.RenderUI(pathTitle, m_quadblocks, checkpointQuery, true, insertAbove, removePath);
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
				if (!path.Ready()) { ready = false; break; }
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
			if (!m_bsp.Empty()) { m_bsp.RenderUI(m_quadblocks); }

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
				auto selection = pfd::save_file::save_file("CTR Ghost File", filename.c_str(), {"Ghost File (*.ctrghost)", "*.ctrghost"}).result();
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
		static Renderer rend = Renderer(800.0f, 400.0f);
		static bool initRend = true;
		constexpr float bottomPaneHeight = 200.0f;

		if (initRend) { ImGui::SetNextWindowSize({rend.GetWidth(), rend.GetHeight() + bottomPaneHeight}); initRend = false; }
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		if (ImGui::Begin("Renderer", &Windows::w_renderer, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImVec2 pos = ImGui::GetCursorScreenPos();
			ImVec2 size = ImGui::GetWindowSize();
			rend.RescaleFramebuffer(size.x, size.y - bottomPaneHeight);
			ImTextureID tex = static_cast<ImTextureID>(rend.GetTexBuffer());
			float rendWidth = rend.GetWidth();
			float rendHeight = rend.GetHeight();

			ImGui::GetWindowDrawList()->AddImage(tex,
				ImVec2(pos.x, pos.y),
				ImVec2(pos.x + rendWidth, pos.y + rendHeight),
				ImVec2(0, 1),
				ImVec2(1, 0));

			ImVec2 mousePos = ImGui::GetMousePos();
			struct {
				int x, y;
			} pixelCoord((int) (mousePos.x - pos.x), (int) (mousePos.y - pos.y));

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				if (pixelCoord.x >= 0 && pixelCoord.x < rendWidth && pixelCoord.y >= 0 && pixelCoord.y < rendHeight)
				{
					ViewportClickHandleBlockSelection(pixelCoord.x, pixelCoord.y, rend);
				}
			}

			std::vector<Model> modelsToRender;

			if (GuiRenderSettings::showLevel)
			{
				modelsToRender.push_back(m_levelModel);
				if (GuiRenderSettings::showLowLOD)
				{
					m_levelModel.SetMesh(&m_lowLODMesh);
					if (GuiRenderSettings::showLevVerts)
					{
						m_levelModel.SetMesh(&m_vertexLowLODMesh);
					}
				}
				else
				{
					m_levelModel.SetMesh(&m_highLODMesh);
					if (GuiRenderSettings::showLevVerts)
					{
						m_levelModel.SetMesh(&m_vertexHighLODMesh);
					}
				}
			}
			if (GuiRenderSettings::showBspRectTree)
			{
				modelsToRender.push_back(m_bspModel);
			}
			if (GuiRenderSettings::showCheckpoints)
			{
				modelsToRender.push_back(m_checkModel);
			}
			if (GuiRenderSettings::showStartpoints)
			{
				modelsToRender.push_back(m_spawnsModel);
			}
			modelsToRender.push_back(m_selectedBlockModel);
			modelsToRender.push_back(m_multipleSelectedQuads);

			rend.Render(modelsToRender);

			ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + rendHeight));
			if (ImGui::BeginChild("Renderer Settings", ImVec2(rendWidth, bottomPaneHeight), ImGuiChildFlags_Border))
			{
				static float rollingOneSecond = 0;
				static int FPS = -1;
				float fm = fmod(rollingOneSecond, 1.f);
				if (fm != rollingOneSecond && rollingOneSecond >= 1.f) //2nd condition prevents fps not updating if deltaTime exactly equals 1.f
				{
					FPS = static_cast<int>(1.f / rend.GetLastDeltaTime());
					rollingOneSecond = fm;
				}
				rollingOneSecond += rend.GetLastDeltaTime();
				if (ImGui::TreeNode("Options"))
				{
					if (ImGui::BeginTable("Renderer Settings Table", 2))
					{
						/*
							* *** Enable viewport resizing
							* Filter by material (highlight all quads with a specified subset of materials)
							* Filter by quad flags (higlight all quads with a specified subset of quadflags)
							* Filter by draw flags ""
							* Filter by terrain ""
							*
							* draw bsp as wireframe or as transparent objs
							*
							* draw (low lod) wireframe on top of the mesh as an option
							*
							* NOTE: resetBsp does not trigger when vertex color changes.
							*
							* Make mesh read live data.
							*
							* Editor features (edit in viewport blender style).
							*/
						static std::string camMoveMult = std::to_string(GuiRenderSettings::camMoveMult);
						static std::string camRotateMult = std::to_string(GuiRenderSettings::camRotateMult);
						static std::string camSprintMult = std::to_string(GuiRenderSettings::camSprintMult);
						static std::string camFOV = std::to_string(GuiRenderSettings::camFovDeg);

						static bool showCheckpoints = false;
						static bool	showStartingPositions = false;
						static bool	showBspRectTree = false;

						float textFieldWidth = std::max(rendWidth / 6.0f, 50.0f);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("FPS: %d", FPS);

						ImGui::Text(""
							"Camera Controls:\n"
							"\t* WASD to move in/out & pan\n"
							"\t* Arrow keys to rotate cam\n"
							"\t* Spacebar to move up, Shift to move down\n"
							"\t* Ctrl to \"Sprint\"");

						ImGui::Combo("Render", &GuiRenderSettings::renderType, GuiRenderSettings::renderTypeLabels.data(), static_cast<int>(GuiRenderSettings::renderTypeLabels.size()));

						ImGui::Checkbox("Show Low LOD", &GuiRenderSettings::showLowLOD);
						ImGui::Checkbox("Show Wireframe", &GuiRenderSettings::showWireframe);
						ImGui::Checkbox("Show Backfaces", &GuiRenderSettings::showBackfaces);
						ImGui::Checkbox("Show Level Verts", &GuiRenderSettings::showLevVerts);
						ImGui::Checkbox("Show Checkpoints", &GuiRenderSettings::showCheckpoints);
						ImGui::Checkbox("Show Starting Positions", &GuiRenderSettings::showStartpoints);
						ImGui::Checkbox("Show BSP Rect Tree", &GuiRenderSettings::showBspRectTree);
						ImGui::Checkbox("Show Vis Tree", &GuiRenderSettings::showVisTree);

						ImGui::PushItemWidth(textFieldWidth);
						if (ImGui::SliderInt("BSP Rect Tree top depth", &GuiRenderSettings::bspTreeTopDepth, 0, GuiRenderSettings::bspTreeMaxDepth)) //top changed
						{
							GuiRenderSettings::bspTreeBottomDepth = std::max(GuiRenderSettings::bspTreeBottomDepth, GuiRenderSettings::bspTreeTopDepth);
							GenerateRenderBspData(m_bsp);
						}
						if (ImGui::SliderInt("BSP Rect Tree bottom depth", &GuiRenderSettings::bspTreeBottomDepth, 0, GuiRenderSettings::bspTreeMaxDepth)) //bottom changed
						{
							GuiRenderSettings::bspTreeTopDepth = std::min(GuiRenderSettings::bspTreeTopDepth, GuiRenderSettings::bspTreeBottomDepth);
							GenerateRenderBspData(m_bsp);
						}

						/* TODO
						if (ImGui::BeginCombo("(NOT IMPL) Mask by Materials", "..."))
						{
							ImGui::Selectable("(NOT IMPL)");
							ImGui::EndCombo();
						}
						if (ImGui::BeginCombo("(NOT IMPL) Mask by Quad flags", "..."))
						{
							ImGui::Selectable("(NOT IMPL)");
							ImGui::EndCombo();
						}
						if (ImGui::BeginCombo("(NOT IMPL) Mask by Draw flags", "..."))
						{
							ImGui::Selectable("(NOT IMPL)");
							ImGui::EndCombo();
						}
						if (ImGui::BeginCombo("(NOT IMPL) Mask by Terrain", "..."))
						{
							ImGui::Selectable("(NOT IMPL)");
							ImGui::EndCombo();
						}
						*/

						auto textUI = [](const std::string& label, std::string& uiValue, float& renderSetting, float minThres = -std::numeric_limits<float>::max(), float maxThres = std::numeric_limits<float>::max())
							{
								float val;
								if (ImGui::InputText(label.c_str(), &uiValue) && ParseFloat(uiValue, val))
								{
									val = Clamp(val, minThres, maxThres);
									uiValue = std::to_string(val);
									renderSetting = val;
								}
							};

						textUI("Camera Move Multiplier", camMoveMult, GuiRenderSettings::camMoveMult, 0.01f);
						textUI("Camera Rotate Multiplier", camRotateMult, GuiRenderSettings::camRotateMult, 0.01f);
						textUI("Camera Sprint Multiplier", camSprintMult, GuiRenderSettings::camSprintMult, 1.0f);
						textUI("Camera FOV", camFOV, GuiRenderSettings::camFovDeg, 5.0f, 150.0f);
						ImGui::PopItemWidth();

						ImGui::TableSetColumnIndex(1);

						if (m_rendererSelectedQuadblockIndex != REND_NO_SELECTED_QUADBLOCK)
						{
							Quadblock& quadblock = m_quadblocks[m_rendererSelectedQuadblockIndex];
							bool resetBsp = false;
							if (quadblock.RenderUI(m_checkpoints.size() - 1, resetBsp))
							{
								ManageTurbopad(quadblock);
							}
							if (resetBsp && m_bsp.Valid())
							{
								m_bsp.Clear();
								GenerateRenderBspData(m_bsp);
							}
						}

						ImGui::EndTable();
					}
					ImGui::TreePop();
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}
}

void Path::RenderUI(const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, bool drawPathBtn, bool& insertAbove, bool& removePath)
{
	auto QuadListUI = [this](std::vector<size_t>& indexes, size_t& value, std::string& label, const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, ButtonUI& button)
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

				if (button.Show(("Add##" + title).c_str(), "Quadblock successfully\nadded to path.", false))
				{
					bool found = false;
					for (const size_t index : indexes)
					{
						if (index == value) { found = true; break; }
					}
					if (!found) { indexes.push_back(value); }
				}
			}
			ImGui::EndChild();
		};

	if (ImGui::TreeNode(title.c_str()))
	{
		if (ImGui::BeginChild(("##" + title).c_str(), {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX))
		{
			bool dummyInsert, dummyRemove = false;
			if (m_left) { m_left->RenderUI("Left Path", quadblocks, searchQuery, false, dummyInsert, dummyRemove); }
			if (m_right) { m_right->RenderUI("Right Path", quadblocks, searchQuery, false, dummyInsert, dummyRemove); }

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
				if (!m_left) { m_left = new Path(m_index + 1); }
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
				if (!m_right) { m_right = new Path(m_index + 2); }
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

		if (drawPathBtn)
		{
			static ButtonUI insertAboveButton;
			static ButtonUI removePathButton;
			if (insertAboveButton.Show(("Insert Path Above##" + std::to_string(m_index)).c_str(), "You're editing the new path.", false)) { insertAbove = true; }
			if (removePathButton.Show(("Remove Current Path##" + std::to_string(m_index)).c_str(), "Path successfully deleted.", false)) { removePath = true; }
		}

		ImGui::TreePop();
	}
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
			ImGui::TreePop();
		}
		ImGui::Text("Downforce:");
		ImGui::SameLine();
		if (ImGui::InputInt("##downforceQuad", &m_downforce)) { m_downforce = Clamp(m_downforce, static_cast<int>(INT8_MIN), static_cast<int>(INT8_MAX)); }
		ImGui::Checkbox("Checkpoint", &m_checkpointStatus);
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
		if (Empty()) { ImGui::TreePop(); return; }
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
					if (!quadblock.Hide() && Matches(quadblock.GetName(), query) && ImGui::Selectable(quadblock.GetName().c_str()))
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
