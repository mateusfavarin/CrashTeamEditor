#include "math.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "quadblock.h"
#include "vertex.h"

#include <imgui.h>

static constexpr size_t MAX_QUADBLOCKS_LEAF = 32;
static constexpr float MAX_LEAF_AXIS_LENGTH = 60.0f;

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
				if (ImGui::Selectable(quadblock.Name().c_str()))
				{
					m_uiPosQuad = quadblock.Name();
					m_pos = quadblock.Center();
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
				auto FlagOptionsUI = [](uint32_t& config, const uint32_t flag, const std::string& title)
					{
						bool active = config & flag;
						if (ImGui::Checkbox(title.c_str(), &active))
						{
							if (active) { config |= flag; }
							else { config &= ~flag; }
						}
					};

				FlagOptionsUI(m_configFlags, LevConfigFlags::ENABLE_SKYBOX_GRADIENT, "Enable Skybox Gradient");
				FlagOptionsUI(m_configFlags, LevConfigFlags::MASK_GRAB_UNDERWATER, "Mask Grab Underwater");
				FlagOptionsUI(m_configFlags, LevConfigFlags::ANIMATE_WATER_VERTEX, "Animate Water Vertex");
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

	static bool resetMaterialPreview = false;
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
								resetMaterialPreview = true;
							}
						}
						ImGui::EndCombo();
					} ImGui::SameLine();
					if (ImGui::Button("Apply"))
					{
						for (const size_t index : quadblockIndexes)
						{
							m_quadblocks[index].SetTerrain(TerrainType::LABELS.at(m_materialTerrainPreview[material]));
						}
						m_materialTerrainBackup[material] = m_materialTerrainPreview[material];
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}

	if (resetMaterialPreview && !w_material)
	{
		for (const auto& [material, quadblockIndexes] : m_materialToQuadblocks)
		{
			m_materialTerrainPreview[material] = m_materialTerrainBackup[material];
		}
		resetMaterialPreview = false;
	}

	if (w_quadblocks)
	{
		if (ImGui::Begin("Quadblocks", &w_quadblocks, ImGuiWindowFlags_AlwaysAutoResize))
		{
			for (Quadblock& quadblock : m_quadblocks) { quadblock.RenderUI(m_checkpoints.size() - 1); }
		}
		ImGui::End();
	}

	if (w_checkpoints)
	{
		if (ImGui::Begin("Checkpoints", &w_checkpoints, ImGuiWindowFlags_AlwaysAutoResize))
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
		}
		ImGui::End();
	}

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