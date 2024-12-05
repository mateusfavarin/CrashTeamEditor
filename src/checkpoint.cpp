#include "checkpoint.h"
#include "psx_types.h"

#include <imgui.h>

static constexpr int NONE_CHECKPOINT_INDEX = std::numeric_limits<int>::max();
static const std::string DEFAULT_UI_CHECKBOX_LABEL = "None";

Checkpoint::Checkpoint(int index)
{
	m_index = index;
	m_pos = Vec3();
	m_distToFinish = 0.0f;
	m_up = NONE_CHECKPOINT_INDEX;
	m_down = NONE_CHECKPOINT_INDEX;
	m_left = NONE_CHECKPOINT_INDEX;
	m_right = NONE_CHECKPOINT_INDEX;
	m_delete = false;
	m_uiPosQuad = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkUp = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkDown = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkLeft = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkRight = DEFAULT_UI_CHECKBOX_LABEL;
}

void Checkpoint::UpdateIndex(int index)
{
	m_index = index;
}

bool Checkpoint::GetDelete() const
{
	return m_delete;
}

void Checkpoint::RemoveInvalidCheckpoints(const std::vector<int>& invalidIndexes)
{
	for (int invalidIndex : invalidIndexes)
	{
		if (m_up == invalidIndex) { m_up = NONE_CHECKPOINT_INDEX; }
		if (m_down == invalidIndex) { m_down = NONE_CHECKPOINT_INDEX; }
		if (m_left == invalidIndex) { m_left = NONE_CHECKPOINT_INDEX; }
		if (m_right == invalidIndex) { m_right = NONE_CHECKPOINT_INDEX; }
	}
}

void Checkpoint::UpdateInvalidCheckpoints(const std::vector<int>& invalidIndexes)
{
	for (int invalidIndex : invalidIndexes)
	{
		if (m_up != NONE_CHECKPOINT_INDEX && m_up > invalidIndex) { m_uiLinkUp = "Checkpoint " + std::to_string(--m_up); }
		if (m_down != NONE_CHECKPOINT_INDEX && m_down > invalidIndex) { m_uiLinkDown = "Checkpoint " + std::to_string(--m_down); }
		if (m_left != NONE_CHECKPOINT_INDEX && m_left > invalidIndex) { m_uiLinkLeft = "Checkpoint " + std::to_string(--m_left); }
		if (m_right != NONE_CHECKPOINT_INDEX && m_right > invalidIndex) { m_uiLinkRight = "Checkpoint " + std::to_string(--m_right); }
	}
}

std::vector<uint8_t> Checkpoint::Serialize() const
{
	PSX::Checkpoint checkpoint;
	std::vector<uint8_t> buffer(sizeof(checkpoint));
	checkpoint.pos = ConvertVec3(m_pos, FP_ONE_GEO);
	checkpoint.distToFinish = ConvertFloat(m_distToFinish, FP_ONE_GEO);
	checkpoint.linkUp = static_cast<uint8_t>(m_up);
	checkpoint.linkDown = static_cast<uint8_t>(m_down);
	checkpoint.linkLeft = static_cast<uint8_t>(m_left);
	checkpoint.linkRight = static_cast<uint8_t>(m_right);
	std::memcpy(buffer.data(), &checkpoint, sizeof(checkpoint));
	return buffer;
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
