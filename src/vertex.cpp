#include "vertex.h"
#include "psx_types.h"

#include <imgui.h>

#include <string>

Vertex::Vertex()
{
	m_pos = Vec3();
	m_flags = VertexFlags::NONE;
	m_colorHigh = Color(255u, 255u, 255u);
	m_colorLow = Color(255u, 255u, 255u);
	m_editedPos = false;
}

Vertex::Vertex(const Point& point)
{
	m_pos = point.pos;
	m_flags = VertexFlags::NONE;
	m_colorHigh = point.color;
	m_colorLow = point.color;
	m_editedPos = false;
}

Vertex::Vertex(const Vec3& pos)
{
	m_pos = pos;
	m_flags = VertexFlags::NONE;
	m_colorHigh = Color(255u, 255u, 255u);
	m_colorLow = Color(255u, 255u, 255u);
	m_editedPos = false;
}

Vertex::Vertex(const Vec3& pos, const Color& color)
{
	m_pos = pos;
	m_flags = VertexFlags::NONE;
	m_colorHigh = color;
	m_colorLow = color;
	m_editedPos = false;
}

bool Vertex::IsEdited() const
{
	return m_editedPos;
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

std::vector<uint8_t> Vertex::Serialize() const
{
	PSX::Vertex v = {};
	std::vector<uint8_t> buffer(sizeof(v));
	v.pos = ConvertVec3(m_pos, FP_ONE_GEO);
	v.flags = VertexFlags::NONE;
	v.colorHi = ConvertColor(m_colorHigh);
	v.colorLo = ConvertColor(m_colorLow);
	std::memcpy(buffer.data(), &v, sizeof(v));
	return buffer;
}
