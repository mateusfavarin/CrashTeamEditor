#pragma once

#include "math.h"
#include "quadblock.h"

#include <string>
#include <vector>

static constexpr int NONE_CHECKPOINT_INDEX = std::numeric_limits<int>::max();
static const std::string DEFAULT_UI_CHECKBOX_LABEL = "None";

class Checkpoint
{
public:
	Checkpoint(int index);
	Checkpoint(int index, const Vec3& pos, const std::string& quadName);
	int Index() const;
	float DistFinish() const;
	const Vec3& Pos() const;
	int Up() const;
	int Down() const;
	int Left() const;
	int Right() const;
	void UpdateDistFinish(float dist);
	void UpdateIndex(int index);
	void UpdateUp(int up);
	void UpdateDown(int down);
	void UpdateLeft(int left);
	void UpdateRight(int right);
	bool GetDelete() const;
	void RemoveInvalidCheckpoints(const std::vector<int>& invalidIndexes);
	void UpdateInvalidCheckpoints(const std::vector<int>& invalidIndexes);
	std::vector<uint8_t> Serialize() const;
	void RenderUI(size_t numCheckpoints, const std::vector<Quadblock>& quadblocks);

private:
	int m_index;
	Vec3 m_pos;
	float m_distToFinish;
	int m_up;
	int m_down;
	int m_left;
	int m_right;
	bool m_delete;
	std::string m_uiPosQuad;
	std::string m_uiLinkUp;
	std::string m_uiLinkDown;
	std::string m_uiLinkLeft;
	std::string m_uiLinkRight;
};