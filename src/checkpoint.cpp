#include "checkpoint.h"
#include "psx_types.h"

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
