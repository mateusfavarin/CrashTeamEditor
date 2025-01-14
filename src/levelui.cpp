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

static constexpr size_t MAX_QUADBLOCKS_LEAF = 32;
static constexpr float MAX_LEAF_AXIS_LENGTH = 60.0f;

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
		if (ImGui::Begin("Log", &m_showLogWindow, ImGuiWindowFlags_AlwaysAutoResize))
		{
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
	static bool w_renderer = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::MenuItem("Spawn")) { w_spawn = !w_spawn; }
		if (ImGui::MenuItem("Level")) { w_level = !w_level; }
		if (!m_materialToQuadblocks.empty() && ImGui::MenuItem("Material")) { w_material = !w_material; }
		if (ImGui::MenuItem("Quadblocks")) { w_quadblocks = !w_quadblocks; }
		if (ImGui::MenuItem("Checkpoints")) { w_checkpoints = !w_checkpoints; }
		if (ImGui::MenuItem("BSP Tree")) { w_bsp = !w_bsp; }
		if (ImGui::MenuItem("Renderer")) { w_renderer = !w_renderer; }
		ImGui::EndMainMenuBar();
	}

	if (w_spawn)
	{
		if (ImGui::Begin("Spawn", &w_spawn, ImGuiWindowFlags_AlwaysAutoResize))
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
		if (ImGui::Begin("Level", &w_level, ImGuiWindowFlags_AlwaysAutoResize))
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

	static bool resetTerrainPreview = false;
	static bool resetQuadflagPreview = false;
	static bool resetDoubleSidedPreview = false;
	if (w_material)
	{
		if (ImGui::Begin("Material", &w_material, ImGuiWindowFlags_AlwaysAutoResize))
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
					if (ImGui::BeginCombo("##terrain", m_materialTerrainPreview[material].c_str()))
					{
						for (const auto& [label, terrain] : TerrainType::LABELS)
						{
							if (ImGui::Selectable(label.c_str()))
							{
								m_materialTerrainPreview[material] = label;
								resetTerrainPreview = true;
							}
						}
						ImGui::EndCombo();
					} ImGui::SameLine();
					if (ImGui::Button("Apply"))
					{
						const std::string& terrain = m_materialTerrainPreview[material];
						for (const size_t index : quadblockIndexes)
						{
							m_quadblocks[index].SetTerrain(TerrainType::LABELS.at(terrain));
						}
						m_materialTerrainBackup[material] = terrain;
					}
					if (ImGui::TreeNode("Quad Flags"))
					{
						for (const auto& [label, flag] : QuadFlags::LABELS)
						{
							if (UIFlagCheckbox(m_materialQuadflagsPreview[material], flag, label))
							{
								resetQuadflagPreview = true;
							}
						}
						if (ImGui::Button("Apply"))
						{
							uint16_t flag = m_materialQuadflagsPreview[material];
							for (const size_t index : quadblockIndexes)
							{
								m_quadblocks[index].SetFlag(flag);
							}
							m_materialQuadflagsBackup[material] = flag;
						}
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("Draw Flags"))
					{
						if (ImGui::Checkbox("Double Sided", &m_materialDoubleSidedPreview[material]))
						{
							resetDoubleSidedPreview = true;
						}
						if (ImGui::Button("Apply"))
						{
							bool active = m_materialDoubleSidedPreview[material];
							for (const size_t index : quadblockIndexes)
							{
								m_quadblocks[index].SetDrawDoubleSided(active);
							}
							m_materialDoubleSidedBackup[material] = active;
						}
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

	if (resetTerrainPreview && !w_material)
	{
		for (const auto& [material, quadblockIndexes] : m_materialToQuadblocks)
		{
			m_materialTerrainPreview[material] = m_materialTerrainBackup[material];
		}
		resetTerrainPreview = false;
	}

	if (resetQuadflagPreview && !w_material)
	{
		for (const auto& [material, quadblockIndexes] : m_materialToQuadblocks)
		{
			m_materialQuadflagsPreview[material] = m_materialQuadflagsBackup[material];
		}
		resetQuadflagPreview = false;
	}

	if (resetDoubleSidedPreview && !w_material)
	{
		for (const auto& [material, quadblockIndexes] : m_materialToQuadblocks)
		{
			m_materialDoubleSidedPreview[material] = m_materialDoubleSidedBackup[material];
		}
		resetDoubleSidedPreview = false;
	}

	static std::string quadblockQuery;
	if (w_quadblocks)
	{
		if (ImGui::Begin("Quadblocks", &w_quadblocks, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::InputTextWithHint("Search", "Search Quadblocks...", &quadblockQuery);
			for (Quadblock& quadblock : m_quadblocks)
			{
				if (Matches(quadblock.Name(), quadblockQuery))
				{
					quadblock.RenderUI(m_checkpoints.size() - 1);
				}
			}
		}
		ImGui::End();
	}

	if (!quadblockQuery.empty() && !w_quadblocks) { quadblockQuery.clear(); }

	static std::string checkpointQuery;
	if (w_checkpoints)
	{
		if (ImGui::Begin("Checkpoints", &w_checkpoints, ImGuiWindowFlags_AlwaysAutoResize))
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
		if (ImGui::Begin("BSP Tree", &w_bsp, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (!m_bsp.Empty())
			{
				size_t bspIndex = 0;
				m_bsp.RenderUI(bspIndex, m_quadblocks);
			}
			if (ImGui::Button("Generate"))
			{
				std::vector<size_t> quadIndexes;
				for (size_t i = 0; i < m_quadblocks.size(); i++) { quadIndexes.push_back(i); }
				m_bsp.Clear();
				m_bsp.SetQuadblockIndexes(quadIndexes);
				m_bsp.Generate(m_quadblocks, MAX_QUADBLOCKS_LEAF, MAX_LEAF_AXIS_LENGTH);
				if (m_bsp.Valid()) { m_bspStatusMessage = "Successfully generated the BSP tree."; }
				else
				{
					m_bsp.Clear();
					m_bspStatusMessage = "Failed generating the BSP tree.";
				}
			}
			if (!m_bspStatusMessage.empty()) { ImGui::Text(m_bspStatusMessage.c_str()); }
		}
		ImGui::End();
	}

	if (w_renderer)
	{
		//if (ImGui::Begin("Renderer", &w_renderer, ImGuiWindowsFlags_))
		ImGui::BeginChild("Renderer");

		ImGui::EndChild();
	}
}

void Path::RenderUI(const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery)
{
    auto QuadListUI = [this](std::vector<size_t>& indexes, size_t& value, std::string& label, const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery)
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

            if (ImGui::Button(("Add##" + title).c_str()))
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
            QuadListUI(m_quadIndexesStart, m_previewValueStart, m_previewLabelStart, "Start", quadblocks, searchQuery);
            ImGui::SameLine();
            QuadListUI(m_quadIndexesEnd, m_previewValueEnd, m_previewLabelEnd, "End", quadblocks, searchQuery);
            ImGui::SameLine();
            QuadListUI(m_quadIndexesIgnore, m_previewValueIgnore, m_previewLabelIgnore, "Ignore", quadblocks, searchQuery);

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

void Quadblock::RenderUI(size_t checkpointCount)
{
	if (ImGui::TreeNode(m_name.c_str()))
	{
		if (ImGui::TreeNode("Vertices"))
		{
			for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
			{
				m_p[i].RenderUI(i);
				if (m_p[i].IsEdited()) { ComputeBoundingBox(); }
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
		ImGui::Text("Checkpoint Index: ");
		ImGui::SameLine();
		if (ImGui::InputInt("##cp", &m_checkpointIndex)) { m_checkpointIndex = Clamp(m_checkpointIndex, -1, static_cast<int>(checkpointCount)); }
		ImGui::TreePop();
	}
}

void Vertex::RenderUI(size_t index)
{
	m_editedPos = false;
	if (ImGui::TreeNode(("Vertex " + std::to_string(index)).c_str()))
	{
		ImGui::Text("Pos: "); ImGui::SameLine();
		if (ImGui::InputFloat3("##pos", m_pos.Data())) { m_editedPos = true; }
		ImGui::Text("High:"); ImGui::SameLine();
		ImGui::ColorEdit3("##high", m_colorHigh.Data());
		ImGui::Text("Low: "); ImGui::SameLine();
		ImGui::ColorEdit3("##low", m_colorLow.Data());
		ImGui::TreePop();
	}
}