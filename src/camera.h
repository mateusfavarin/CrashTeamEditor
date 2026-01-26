#pragma once

#include "glm.hpp"

struct ImGuiIO;

class Camera
{
public:
	Camera();
	void Update(bool allowShortcuts, float deltaTime);
	const glm::vec3& GetPosition() const;
	const glm::vec3& GetFront() const;
	const glm::vec3& GetTarget() const;
	const glm::mat4& GetViewMatrix() const;
	float GetDistance() const;
	glm::mat4 BuildBillboardMatrix(const glm::vec3& position, const glm::vec3& scale,
		const glm::vec3& eulerRotation = glm::vec3(0.0f)) const;

	void SetPosition(const glm::vec3& pos) { m_position = pos; }
	void SetTarget(const glm::vec3& target) { m_target = target; }
	void SetPitch(float pitch) { m_pitch = pitch; }
	void SetYaw(float yaw) { m_yaw = yaw; }
	void SetDistance(float distance) { m_distance = distance; }

private:
	void Initialize();

	glm::vec3 m_position;
	glm::vec3 m_front;
	glm::vec3 m_target;
	glm::mat4 m_matrix;
	float m_distance;
	float m_pitch;
	float m_yaw;
};
