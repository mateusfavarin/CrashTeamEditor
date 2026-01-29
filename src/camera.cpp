#include "camera.h"

#include "gui_render_settings.h"
#include "utils.h"

#include "gtc/matrix_transform.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include "transform.h"

Camera::Camera()
	: m_position(0.f, 0.f, 3.f)
	, m_front(0.f, 0.f, -1.f)
	, m_target(0.f, 0.f, 0.f)
	, m_distance(3.0f)
	, m_pitch(0.f)
	, m_yaw(-90.f)
{
	Initialize();
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
}

void Camera::Update(bool allowShortcuts, float deltaTime)
{
	if (allowShortcuts)
	{
		ImGuiIO& io = ImGui::GetIO();
		const ImGuiKey sprintKey = static_cast<ImGuiKey>(GuiRenderSettings::camKeySprint);
		const bool cameraSprintDown = ImGui::IsKeyDown(sprintKey);

		if (ImGui::IsMouseDown(GuiRenderSettings::camOrbitMouseButton) && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
		{
			if (cameraSprintDown)
			{
				float panSpeed = 0.001f * m_distance * GuiRenderSettings::camMoveMult;
				glm::vec3 right = glm::normalize(glm::cross(m_front, glm::vec3(0.f, 1.f, 0.f)));
				glm::vec3 up = glm::normalize(glm::cross(right, m_front));
				m_target -= right * io.MouseDelta.x * panSpeed;
				m_target += up * io.MouseDelta.y * panSpeed;
			}
			else
			{
				float orbitSpeed = 0.2f * GuiRenderSettings::camRotateMult;
				m_yaw += io.MouseDelta.x * orbitSpeed;
				m_pitch += io.MouseDelta.y * orbitSpeed;
				m_pitch = Clamp(m_pitch, -89.9f, 89.9f);
			}
		}

		if (io.MouseWheel != 0.0f)
		{
			const float wheelZoom = 0.15f * m_distance * GuiRenderSettings::camZoomMult;
			m_distance -= io.MouseWheel * wheelZoom;
		}

		float moveSpeed = 15.f * deltaTime * GuiRenderSettings::camMoveMult;
		if (cameraSprintDown) { moveSpeed *= GuiRenderSettings::camSprintMult; }

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

	glm::vec3 offset = {
		cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw)),
		sin(glm::radians(m_pitch)),
		cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw))
	};
	m_position = m_target + offset * m_distance;
	m_front = glm::normalize(m_target - m_position);

	const glm::vec3 camUp(0.f, 1.f, 0.f);
	m_matrix = glm::lookAt(m_position, m_position + m_front, camUp);
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

glm::mat4 Camera::BuildBillboardMatrix(const glm::vec3& position, const glm::vec3& scale) const
{
	glm::vec3 forward = m_position - position;
	const float forwardLenSq = glm::dot(forward, forward);
	if (forwardLenSq < 0.000001f) { forward = glm::vec3(0.0f, 0.0f, 1.0f); }
	else { forward = glm::normalize(forward); }

	glm::vec3 up(0.0f, 1.0f, 0.0f);
	glm::vec3 right = glm::cross(up, forward);
	const float rightLenSq = glm::dot(right, right);
	if (rightLenSq < 0.000001f)
	{
		up = glm::vec3(0.0f, 0.0f, 1.0f);
		right = glm::cross(up, forward);
	}
	right = glm::normalize(right);
	up = glm::normalize(glm::cross(forward, right));

	glm::mat4 billboardRotation(1.0f);
	billboardRotation[0] = glm::vec4(right, 0.0f);
	billboardRotation[1] = glm::vec4(up, 0.0f);
	billboardRotation[2] = glm::vec4(forward, 0.0f);

	glm::mat4 model(1.0f);
	model = glm::translate(model, position);
	model *= billboardRotation;
	model = glm::scale(model, scale);
	return model;
}
