#include "camera.h"

#include "gui_render_settings.h"
#include "utils.h"

#include "gtc/matrix_transform.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

Camera::Camera()
	: m_position(0.f, 0.f, 3.f)
	, m_front(0.f, 0.f, -1.f)
	, m_target(0.f, 0.f, 0.f)
	, m_distance(3.0f)
	, m_pitch(0.f)
	, m_yaw(-90.f)
	, m_initialized(false)
{
}

void Camera::Initialize()
{
	glm::vec3 dir = m_target - m_position;
	float len = glm::length(dir);
	if (len > 0.0f)
	{
		dir = glm::normalize(dir);
		m_pitch = glm::degrees(asin(dir.y));
		m_yaw = glm::degrees(atan2(dir.z, dir.x));
		m_distance = len;
		m_front = dir;
	}
	m_initialized = true;
}

void Camera::Update(bool allowShortcuts, float deltaTime)
{
	if (!m_initialized) { Initialize(); }
	const glm::vec3 camUp(0.f, 1.f, 0.f);
	m_matrix = glm::lookAt(m_position, m_position + m_front, camUp);

	if (allowShortcuts)
	{
		ImGuiIO& io = ImGui::GetIO();
		const ImGuiKey sprintKey = static_cast<ImGuiKey>(GuiRenderSettings::camKeySprint);
		const bool shiftDown = ImGui::IsKeyDown(sprintKey);

		if (ImGui::IsMouseDown(GuiRenderSettings::camOrbitMouseButton) && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
		{
			float orbitSpeed = 0.2f * GuiRenderSettings::camRotateMult;
			m_yaw += io.MouseDelta.x * orbitSpeed;
			m_pitch -= io.MouseDelta.y * orbitSpeed;
			m_pitch = Clamp(m_pitch, -89.0f, 89.0f);
		}

		if (io.MouseWheel != 0.0f)
		{
			const float wheelZoom = 0.15f * m_distance * GuiRenderSettings::camZoomMult;
			m_distance -= io.MouseWheel * wheelZoom;
		}

		float moveSpeed = 15.f * deltaTime * GuiRenderSettings::camMoveMult;
		if (shiftDown) { moveSpeed *= GuiRenderSettings::camSprintMult; }

		glm::vec3 forwardPlanar = glm::normalize(glm::vec3(cos(glm::radians(m_yaw)), 0.0f, sin(glm::radians(m_yaw))));
		glm::vec3 right = glm::normalize(glm::cross(forwardPlanar, glm::vec3(0.f, 1.f, 0.f)));
		glm::vec3 movement(0.0f);

		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(GuiRenderSettings::camKeyForward))) { movement -= forwardPlanar; }
		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(GuiRenderSettings::camKeyBack))) { movement += forwardPlanar; }
		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(GuiRenderSettings::camKeyRight))) { movement -= right; }
		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(GuiRenderSettings::camKeyLeft))) { movement += right; }
		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(GuiRenderSettings::camKeyUp))) { movement += glm::vec3(0.f, 1.f, 0.f); }
		if (ImGui::IsKeyDown(static_cast<ImGuiKey>(GuiRenderSettings::camKeyDown))) { movement -= glm::vec3(0.f, 1.f, 0.f); }

		if (movement.x != 0.0f || movement.y != 0.0f || movement.z != 0.0f)
		{
			m_target += glm::normalize(movement) * moveSpeed;
		}
	}

	m_distance = std::max(m_distance, 0.1f);

	glm::vec3 offset;
	offset.x = cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw));
	offset.y = sin(glm::radians(m_pitch));
	offset.z = cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw));
	m_position = m_target + offset * m_distance;
	m_front = glm::normalize(m_target - m_position);
}

const glm::vec3& Camera::GetPosition() const
{
	return m_position;
}

const glm::vec3& Camera::GetFront() const
{
	return m_front;
}

const glm::vec3& Camera::GetTarget() const
{
	return m_target;
}

const glm::mat4& Camera::GetViewMatrix() const
{
	return m_matrix;
}

float Camera::GetDistance() const
{
	return m_distance;
}
