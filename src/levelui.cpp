#include "geo.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "path.h"
#include "quadblock.h"
#include "vertex.h"
#include "utils.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <string>
#include <chrono>

static constexpr size_t MAX_QUADBLOCKS_LEAF = 32;
static constexpr float MAX_LEAF_AXIS_LENGTH = 60.0f;
static constexpr long long BUTTON_UI_TIMEOUT_MESSAGE = 1; // seconds

class ButtonUI
{
public:
	ButtonUI();
	bool Show(const std::string& label, const std::string& message);

private:
	std::string m_labelTriggered;
	std::chrono::steady_clock::time_point m_messageTimeoutStart;
};

ButtonUI::ButtonUI()
{
	m_labelTriggered = std::string();
	m_messageTimeoutStart = std::chrono::steady_clock::time_point();
}

bool ButtonUI::Show(const std::string& label, const std::string& message)
{
	bool ret = false;
	if (ImGui::Button(label.c_str()))
	{
		m_labelTriggered = label;
		m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
		ret = true;
	}
	if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_messageTimeoutStart).count() < BUTTON_UI_TIMEOUT_MESSAGE
			&& m_labelTriggered == label)
	{
		ImGui::Text(message.c_str());
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

void BSP::RenderUI(size_t& index, const std::vector<Quadblock>& quadblocks)
{
	std::string title = GetType() + " " + std::to_string(index++);
	if (ImGui::TreeNode(title.c_str()))
	{
		if (IsBranch()) { ImGui::Text(("Axis:  " + GetAxis()).c_str()); }
		ImGui::Text(("Quads: " + std::to_string(m_quadblockIndexes.size())).c_str());
		if (ImGui::TreeNode("Quadblock List:"))
		{
			constexpr size_t QUADS_PER_LINE = 10;
			for (size_t i = 0; i < m_quadblockIndexes.size(); i++)
			{
				ImGui::Text((quadblocks[m_quadblockIndexes[i]].Name() + ", ").c_str());
				if (((i + 1) % QUADS_PER_LINE) == 0 || i == m_quadblockIndexes.size() - 1) { continue; }
				ImGui::SameLine();
			}
			ImGui::TreePop();
		}
		ImGui::Text("Bounding Box:");
		m_bbox.RenderUI();
		if (m_left) { m_left->RenderUI(++index, quadblocks); }
		if (m_right) { m_right->RenderUI(++index, quadblocks); }
		ImGui::TreePop();
	}
}

void Checkpoint::RenderUI(size_t numCheckpoints, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery)
{
	if (ImGui::TreeNode(("Checkpoint " + std::to_string(m_index)).c_str()))
	{
		ImGui::Text("Pos:       "); ImGui::SameLine(); ImGui::InputFloat3("##pos", m_pos.Data());
		ImGui::Text("Quad:      "); ImGui::SameLine();
		if (ImGui::BeginCombo("##quad", m_uiPosQuad.c_str()))
		{
			for (const Quadblock& quadblock : quadblocks)
			{
				if (searchQuery.empty() || quadblock.Name().find(searchQuery) != std::string::npos)
				{
					if (ImGui::Selectable(quadblock.Name().c_str()))
					{
						m_uiPosQuad = quadblock.Name();
						m_pos = quadblock.Center();
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("Update checkpoint position by selecting a specific quadblock.");

		ImGui::Text("Distance:  "); ImGui::SameLine();
		if (ImGui::InputFloat("##dist", &m_distToFinish)) { m_distToFinish = std::max(m_distToFinish, 0.0f); }
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
					if (ImGui::TreeNode(quadblock.c_str()))
					{
						ImGui::Text(errorMessage.c_str());
						ImGui::TreePop();
					}
				}
			}
		}
		ImGui::End();
	}

	if (m_name.empty()) { return; }

	static bool w_spawn = false;
	static bool w_level = false;
	static bool w_material = false;
	static bool w_quadblocks = false;
	static bool w_checkpoints = false;
	static bool w_bsp = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::MenuItem("Spawn")) { w_spawn = !w_spawn; }
		if (ImGui::MenuItem("Level")) { w_level = !w_level; }
		if (!m_materialToQuadblocks.empty() && ImGui::MenuItem("Material")) { w_material = !w_material; }
		if (ImGui::MenuItem("Quadblocks")) { w_quadblocks = !w_quadblocks; }
		if (ImGui::MenuItem("Checkpoints")) { w_checkpoints = !w_checkpoints; }
		if (ImGui::MenuItem("BSP Tree")) { w_bsp = !w_bsp; }
		ImGui::EndMainMenuBar();
	}

	if (w_spawn)
	{
		if (ImGui::Begin("Spawn", &w_spawn))
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
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

	if (w_level)
	{
		if (ImGui::Begin("Level", &w_level))
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
						ImGui::Text("From:"); ImGui::SameLine(); ImGui::ColorEdit3("##color_from", m_skyGradient[i].colorFrom.Data());
						ImGui::Text("To:  "); ImGui::SameLine(); ImGui::ColorEdit3("##color_to", m_skyGradient[i].colorTo.Data());
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Clear Color"))
			{
				ImGui::ColorEdit3("##color", m_clearColor.Data());
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	if (w_material)
	{
		if (ImGui::Begin("Material", &w_material))
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
							ImGui::Text((m_quadblocks[quadblockIndexes[i]].Name() + ", ").c_str());
							if (((i + 1) % QUADS_PER_LINE) == 0 || i == quadblockIndexes.size() - 1) { continue; }
							ImGui::SameLine();
						}
						ImGui::TreePop();
					}

					ImGui::Text("Terrain:"); ImGui::SameLine();
					if (ImGui::BeginCombo("##terrain", m_propTerrain.GetPreview(material).c_str()))
					{
						for (const auto& [label, terrain] : TerrainType::LABELS)
						{
							if (ImGui::Selectable(label.c_str()))
							{
								m_propTerrain.SetPreview(material, label);
							}
						}
						ImGui::EndCombo();
					} ImGui::SameLine();

					static ButtonUI terrainApplyButton = ButtonUI();
					if (terrainApplyButton.Show(("Apply##terrain" + material).c_str(), "Terrain type successfully updated."))
					{
						const std::string& terrain = m_propTerrain.GetPreview(material);
						for (const size_t index : quadblockIndexes)
						{
							m_quadblocks[index].SetTerrain(TerrainType::LABELS.at(terrain));
						}
						m_propTerrain.SetBackup(material, terrain);
					}

					if (ImGui::TreeNode("Quad Flags"))
					{
						for (const auto& [label, flag] : QuadFlags::LABELS)
						{
							UIFlagCheckbox(m_propQuadFlags.GetPreview(material), flag, label);
						}

						static ButtonUI quadFlagsApplyButton = ButtonUI();
						if (quadFlagsApplyButton.Show(("Apply##quadflags" + material).c_str(), "Quad flags successfully updated."))
						{
							uint16_t flag = m_propQuadFlags.GetPreview(material);
							for (const size_t index : quadblockIndexes)
							{
								Quadblock& quadblock = m_quadblocks[index];
								if (!quadblock.Hide() && quadblock.TurboPadIndex() == TURBO_PAD_INDEX_NONE)
								{
									m_quadblocks[index].SetFlag(flag);
								}
							}
							m_propQuadFlags.SetBackup(material, flag);
						}
						ImGui::TreePop();
					}

					if (ImGui::TreeNode("Draw Flags"))
					{
						ImGui::Checkbox("Double Sided", &m_propDoubleSided.GetPreview(material));

						static ButtonUI drawFlagsApplyButton = ButtonUI();
						if (drawFlagsApplyButton.Show(("Apply##drawflags" + material).c_str(), "Draw flags successfully updated."))
						{
							bool active = m_propDoubleSided.GetPreview(material);
							for (const size_t index : quadblockIndexes)
							{
								m_quadblocks[index].SetDrawDoubleSided(active);
							}
							m_propDoubleSided.SetBackup(material, active);
						}
						ImGui::TreePop();
					}

					ImGui::Checkbox("Checkpoint", &m_propCheckpoints.GetPreview(material));
					ImGui::SameLine();

					static ButtonUI checkpointApplyButton = ButtonUI();
					if (checkpointApplyButton.Show(("Apply##checkpoint" + material).c_str(), "Checkpoint status successfully updated."))
					{
						bool active = m_propCheckpoints.GetPreview(material);
						for (const size_t index : quadblockIndexes)
						{
							m_quadblocks[index].CheckpointStatus() = active;
						}
						m_propCheckpoints.SetBackup(material, active);
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

	if (!w_material) { RestoreMaterials(); }

	static std::string quadblockQuery;
	if (w_quadblocks)
	{
		bool resetBsp = false;
		if (ImGui::Begin("Quadblocks", &w_quadblocks))
		{
			ImGui::InputTextWithHint("Search", "Search Quadblocks...", &quadblockQuery);
			for (size_t j = 0; j < m_quadblocks.size(); j++)
			{
				Quadblock& quadblock = m_quadblocks[j];
				if (!quadblock.Hide() && Matches(quadblock.Name(), quadblockQuery))
				{
					if (quadblock.RenderUI(m_checkpoints.size() - 1, resetBsp))
					{
						bool stp = true;
						size_t turboPadIndex = TURBO_PAD_INDEX_NONE;
						switch (quadblock.Trigger())
						{
							case QuadblockTrigger::TURBO_PAD:
								stp = false;
							case QuadblockTrigger::SUPER_TURBO_PAD:
							{
								Quadblock turboPad = quadblock;
								turboPad.SetCheckpoint(-1);
								turboPad.SetCheckpointStatus(false);
								turboPad.SetName(quadblock.Name() + (stp ? "_stp" : "_tp"));
								turboPad.SetFlag(QuadFlags::TRIGGER_SCRIPT | QuadFlags::DEFAULT);
								turboPad.SetTerrain(stp ? TerrainType::SUPER_TURBO_PAD : TerrainType::TURBO_PAD);
								turboPad.SetTurboPadIndex(TURBO_PAD_INDEX_NONE);
								turboPad.SetHide(true);

								size_t index = m_quadblocks.size();
								turboPadIndex = quadblock.TurboPadIndex();
								quadblock.SetTurboPadIndex(index);
								m_quadblocks.push_back(turboPad);
								if (turboPadIndex == TURBO_PAD_INDEX_NONE) { break; }
							}
							case QuadblockTrigger::NONE:
							{
								bool clearTurboPadIndex = false;
								if (turboPadIndex == TURBO_PAD_INDEX_NONE)
								{
									clearTurboPadIndex = true;
									turboPadIndex = quadblock.TurboPadIndex();
								}
								if (turboPadIndex == TURBO_PAD_INDEX_NONE) { break; }

								for (Quadblock& quad : m_quadblocks)
								{
									size_t index = quad.TurboPadIndex();
									if (index > turboPadIndex) { quad.SetTurboPadIndex(index - 1); }
								}

								if (clearTurboPadIndex) { quadblock.SetTurboPadIndex(TURBO_PAD_INDEX_NONE); }
								m_quadblocks.erase(m_quadblocks.begin() + turboPadIndex);
								break;
							}
						}
					}
				}
			}
		}
		ImGui::End();
		if (resetBsp && m_bsp.Valid())
		{
			m_bsp.Clear();
			m_showLogWindow = true;
			m_logMessage = "Modifying quadblock position or turbo pad state automatically resets the BSP tree.";
		}
	}

	if (!quadblockQuery.empty() && !w_quadblocks) { quadblockQuery.clear(); }

	static std::string checkpointQuery;
	if (w_checkpoints)
	{
		if (ImGui::Begin("Checkpoints", &w_checkpoints))
		{
			ImGui::InputTextWithHint("Search##", "Search Quadblocks...", &checkpointQuery);
			if (ImGui::TreeNode("Checkpoints"))
			{
				std::vector<int> checkpointsDelete;
				for (int i = 0; i < m_checkpoints.size(); i++)
				{
					m_checkpoints[i].RenderUI(m_checkpoints.size(), m_quadblocks, checkpointQuery);
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
			for (Path& path : m_checkpointPaths)
			{
				path.RenderUI("Path " + std::to_string(path.Index()), m_quadblocks, checkpointQuery);
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
			if (ImGui::Button("Generate"))
			{
				size_t checkpointIndex = 0;
				std::vector<size_t> linkNodeIndexes;
				std::vector<std::vector<Checkpoint>> pathCheckpoints;
				for (Path& path : m_checkpointPaths)
				{
					pathCheckpoints.push_back(path.GeneratePath(checkpointIndex, m_quadblocks));
					checkpointIndex += pathCheckpoints.back().size();
					linkNodeIndexes.push_back(path.Start());
					linkNodeIndexes.push_back(path.End());
				}
				m_checkpoints.clear();
				for (const std::vector<Checkpoint>& checkpoints : pathCheckpoints)
				{
					for (const Checkpoint& checkpoint : checkpoints)
					{
						m_checkpoints.push_back(checkpoint);
					}
				}

				int lastPathIndex = static_cast<int>(m_checkpointPaths.size()) - 1;
				const Checkpoint* currStartCheckpoint = &m_checkpoints[m_checkpointPaths[lastPathIndex].Start()];
				for (int i = lastPathIndex - 1; i >= 0; i--)
				{
					m_checkpointPaths[i].UpdateDist(currStartCheckpoint->DistFinish(), currStartCheckpoint->Pos(), m_checkpoints);
					currStartCheckpoint = &m_checkpoints[m_checkpointPaths[i].Start()];
				}

				for (size_t i = 0; i < linkNodeIndexes.size(); i++)
				{
					Checkpoint& node = m_checkpoints[linkNodeIndexes[i]];
					if (i % 2 == 0)
					{
						size_t linkDown = (i == 0) ? linkNodeIndexes.size() - 1 : i - 1;
						node.UpdateDown(static_cast<int>(linkNodeIndexes[linkDown]));
					}
					else
					{
						size_t linkUp = (i + 1) % linkNodeIndexes.size();
						node.UpdateUp(static_cast<int>(linkNodeIndexes[linkUp]));
					}
				}
			}
			ImGui::EndDisabled();
			ImGui::TreePop();
		}
		ImGui::End();
	}

	if (!checkpointQuery.empty() && !w_checkpoints) { checkpointQuery.clear(); }

	if (w_bsp)
	{
		if (ImGui::Begin("BSP Tree", &w_bsp))
		{
			if (!m_bsp.Empty())
			{
				size_t bspIndex = 0;
				m_bsp.RenderUI(bspIndex, m_quadblocks);
			}

			static std::string buttonMessage;
			static ButtonUI generateBSPButton = ButtonUI();
			if (generateBSPButton.Show("Generate", buttonMessage))
			{
				std::vector<size_t> quadIndexes;
				for (size_t i = 0; i < m_quadblocks.size(); i++) { quadIndexes.push_back(i); }
				m_bsp.Clear();
				m_bsp.SetQuadblockIndexes(quadIndexes);
				m_bsp.Generate(m_quadblocks, MAX_QUADBLOCKS_LEAF, MAX_LEAF_AXIS_LENGTH);
				if (m_bsp.Valid()) { buttonMessage = "Successfully generated the BSP tree."; }
				else
				{
					m_bsp.Clear();
					buttonMessage = "Failed generating the BSP tree.";
				}
			}
		}
		ImGui::End();
	}
}

void Path::RenderUI(const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery)
{
    auto QuadListUI = [this](std::vector<size_t>& indexes, size_t& value, std::string& label, const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, ButtonUI& button)
    {
        if (ImGui::BeginChild(title.c_str(), {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX))
        {
            ImGui::Text(title.c_str());
            if (ImGui::TreeNode("Quad list:"))
            {
                std::vector<size_t> deleteList;
                for (size_t i = 0; i < indexes.size(); i++)
                {
                    ImGui::Text(quadblocks[indexes[i]].Name().c_str()); ImGui::SameLine();
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
                    if (Matches(quadblocks[i].Name(), searchQuery))
                    {
                        if (ImGui::Selectable(quadblocks[i].Name().c_str()))
                        {
                            label = quadblocks[i].Name();
                            value = i;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            if (button.Show(("Add##" + title).c_str(), "Quadblock successfully\nadded to path."))
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
            if (m_left) { m_left->RenderUI("Left Path", quadblocks, searchQuery); }
            if (m_right) { m_right->RenderUI("Right Path", quadblocks, searchQuery); }

						static ButtonUI startButton = ButtonUI();
						static ButtonUI endButton = ButtonUI();
						static ButtonUI ignoreButton = ButtonUI();
            QuadListUI(m_quadIndexesStart, m_previewValueStart, m_previewLabelStart, "Start", quadblocks, searchQuery, startButton);
            ImGui::SameLine();
            QuadListUI(m_quadIndexesEnd, m_previewValueEnd, m_previewLabelEnd, "End", quadblocks, searchQuery, endButton);
            ImGui::SameLine();
            QuadListUI(m_quadIndexesIgnore, m_previewValueIgnore, m_previewLabelIgnore, "Ignore", quadblocks, searchQuery, ignoreButton);

            if (ImGui::Button("Add Left Path "))
            {
                if (!m_left) { m_left = new Path(m_index + 1); }
            } ImGui::SameLine();
            ImGui::BeginDisabled(m_left == nullptr);
            if (ImGui::Button("Delete Left Path "))
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
		if (ImGui::TreeNode("Draw Flags"))
		{
			ImGui::Checkbox("Double Sided", &m_doubleSided);
			ImGui::TreePop();
		}
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
			m_flags = QuadFlags::INVISIBLE_TRIGGER | QuadFlags::DEFAULT;
			resetBsp = true;
			ret = true;
		} ImGui::SameLine();
		if (ImGui::RadioButton("Super Turbo Pad", m_trigger == QuadblockTrigger::SUPER_TURBO_PAD))
		{
			m_trigger = QuadblockTrigger::SUPER_TURBO_PAD;
			m_flags = QuadFlags::INVISIBLE_TRIGGER | QuadFlags::DEFAULT;
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
		ImGui::ColorEdit3("##high", m_colorHigh.Data());
		ImGui::Text("Low: "); ImGui::SameLine();
		ImGui::ColorEdit3("##low", m_colorLow.Data());
		ImGui::TreePop();
	}
}