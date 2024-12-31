#include "checkpoint.h"
#include "psx_types.h"

#include <cstring>

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

Checkpoint::Checkpoint(int index, const Vec3& pos, const std::string& quadName)
{
	m_index = index;
	m_pos = pos;
	m_distToFinish = 0.0f;
	m_up = NONE_CHECKPOINT_INDEX;
	m_down = NONE_CHECKPOINT_INDEX;
	m_left = NONE_CHECKPOINT_INDEX;
	m_right = NONE_CHECKPOINT_INDEX;
	m_delete = false;
	m_uiPosQuad = quadName;
	m_uiLinkUp = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkDown = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkLeft = DEFAULT_UI_CHECKBOX_LABEL;
	m_uiLinkRight = DEFAULT_UI_CHECKBOX_LABEL;
}

int Checkpoint::Index() const
{
	return m_index;
}

float Checkpoint::DistFinish() const
{
	return m_distToFinish;
}

const Vec3& Checkpoint::Pos() const
{
	return m_pos;
}

int Checkpoint::Up() const
{
	return m_up;
}

int Checkpoint::Down() const
{
	return m_down;
}

int Checkpoint::Left() const
{
	return m_left;
}

int Checkpoint::Right() const
{
	return m_right;
}

void Checkpoint::UpdateDistFinish(float dist)
{
	m_distToFinish = dist;
}

void Checkpoint::UpdateIndex(int index)
{
	m_index = index;
}

void Checkpoint::UpdateUp(int up)
{
	m_up = up;
	m_uiLinkUp = "Checkpoint " + std::to_string(m_up);
}

void Checkpoint::UpdateDown(int down)
{
	m_down = down;
	m_uiLinkDown = "Checkpoint " + std::to_string(m_down);
}

void Checkpoint::UpdateLeft(int left)
{
	m_left = left;
	m_uiLinkLeft = "Checkpoint " + std::to_string(m_left);
}

void Checkpoint::UpdateRight(int right)
{
	m_right = right;
	m_uiLinkRight = "Checkpoint " + std::to_string(m_right);
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
	checkpoint.distToFinish = ConvertFloat(m_distToFinish, FP_ONE_CP);
	checkpoint.linkUp = static_cast<uint8_t>(m_up);
	checkpoint.linkDown = static_cast<uint8_t>(m_down);
	checkpoint.linkLeft = static_cast<uint8_t>(m_left);
	checkpoint.linkRight = static_cast<uint8_t>(m_right);
	std::memcpy(buffer.data(), &checkpoint, sizeof(checkpoint));
	return buffer;
}
