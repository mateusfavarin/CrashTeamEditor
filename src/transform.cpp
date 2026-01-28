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
	m_parentMatrix = glm::mat4(1.0f);
	UpdateModelMatrix();
}

void Transform::SetPosition(const Vec3& pos)
{
	m_position.x = pos.x;
	m_position.y = pos.y;
	m_position.z = pos.z;
	UpdateModelMatrix();
}

void Transform::SetScale(const Vec3& scale)
{
	m_scale.x = scale.x;
	m_scale.y = scale.y;
	m_scale.z = scale.z;
	UpdateModelMatrix();
}

void Transform::SetScale(float scale)
{
	m_scale.x = scale;
	m_scale.y = scale;
	m_scale.z = scale;
	UpdateModelMatrix();
}

void Transform::MultScale(float factor)
{
	m_scale.x *= factor;
	m_scale.y *= factor;
	m_scale.z *= factor;
	UpdateModelMatrix();
}

void Transform::SetRotation(const Vec3& rotation)
{
	m_rotation = glm::quat(glm::radians(glm::vec3(rotation.x, rotation.y, rotation.z)));
	UpdateModelMatrix();
}

void Transform::SetParentMatrix(const glm::mat4& matrix)
{
	m_parentMatrix = matrix;
	UpdateModelMatrix();
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

Vec3 Transform::GetWorldPosition() const
{
	const glm::mat4& model = GetModelMatrix();
	const glm::vec3 translation(model[3].x, model[3].y, model[3].z);
	return Vec3(translation.x, translation.y, translation.z);
}

Vec3 Transform::GetWorldScale() const
{
	const glm::mat4& model = GetModelMatrix();
	const glm::vec3 scale(
		glm::length(glm::vec3(model[0].x, model[0].y, model[0].z)),
		glm::length(glm::vec3(model[1].x, model[1].y, model[1].z)),
		glm::length(glm::vec3(model[2].x, model[2].y, model[2].z)));
	return Vec3(scale.x, scale.y, scale.z);
}

Vec3 Transform::GetWorldRotation() const
{
	const glm::mat4& model = GetModelMatrix();
	const glm::vec3 scale(
		glm::length(glm::vec3(model[0].x, model[0].y, model[0].z)),
		glm::length(glm::vec3(model[1].x, model[1].y, model[1].z)),
		glm::length(glm::vec3(model[2].x, model[2].y, model[2].z)));

	if (scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f)
	{
		glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(m_rotation));
		return Vec3(eulerAngles.x, eulerAngles.y, eulerAngles.z);
	}

	glm::mat3 rotationMatrix;
	rotationMatrix[0] = glm::vec3(model[0].x, model[0].y, model[0].z) / scale.x;
	rotationMatrix[1] = glm::vec3(model[1].x, model[1].y, model[1].z) / scale.y;
	rotationMatrix[2] = glm::vec3(model[2].x, model[2].y, model[2].z) / scale.z;

	const glm::quat rotation = glm::quat_cast(rotationMatrix);
	const glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(rotation));
	return Vec3(eulerAngles.x, eulerAngles.y, eulerAngles.z);
}

const glm::mat4& Transform::GetModelMatrix() const
{
	return m_matrix;
}

void Transform::UpdateModelMatrix()
{
	glm::mat4 model = glm::mat4(1.0f); // scale * rotate * translate
	model = glm::translate(model, m_position);
	model *= glm::mat4_cast(m_rotation);
	model = glm::scale(model, m_scale);
	m_matrix = m_parentMatrix * model;
}