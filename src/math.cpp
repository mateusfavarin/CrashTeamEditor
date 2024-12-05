#include "math.h"

#include <imgui.h>

Quad::Quad(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3)
{
	p[0] = p0; p[1] = p1; p[2] = p2; p[3] = p3;
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
