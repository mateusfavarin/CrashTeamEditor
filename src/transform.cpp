#include "transform.h"

Transform::Transform()
{
	Clear();
}

void Transform::Clear()
{
	m_position = glm::vec3(0.0f, 0.0f, 0.0f);
	m_scale = glm::vec3(1.0f, 1.0f, 1.0f);
	m_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void Transform::SetPosition(const Vec3& pos)
{
	m_position.x = pos.x;
	m_position.y = pos.y;
	m_position.z = pos.z;
}

void Transform::SetScale(const Vec3& scale)
{
	m_scale.x = scale.x;
	m_scale.y = scale.y;
	m_scale.z = scale.z;
}

void Transform::SetScale(float scale)
{
	m_scale.x = scale;
	m_scale.y = scale;
	m_scale.z = scale;
}

void Transform::SetRotation(const Vec3& rotation)
{
	m_rotation = glm::quat(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z)));
}

Vec3 Transform::GetPosition() const
{
	return Vec3(m_position.x, m_position.y, m_position.z);
}

Vec3 Transform::GetScale() const
{
	return Vec3(m_scale.x, m_scale.y, m_scale.z);
}

Vec3 Transform::GetRotation() const
{
	glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(m_rotation));
	return Vec3(eulerAngles.x, eulerAngles.y, eulerAngles.z);
}

glm::mat4 Transform::CalculateModelMatrix() const
{
	glm::mat4 model = glm::mat4(1.0f); // scale * rotate * translate
	model = glm::translate(model, m_position);
	model *= static_cast<glm::mat<4, 4, float, glm::packed_highp>>(m_rotation);
	model = glm::scale(model, m_scale);
	return model;
}
