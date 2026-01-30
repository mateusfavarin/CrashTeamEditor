#pragma once

#include "geo.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/quaternion.hpp"

class Transform
{
public:
	Transform();
	void Clear();
	void SetPosition(const Vec3& pos);
	void SetScale(const Vec3& scale);
	void SetScale(float scale);
	void MultScale(float factor);
	void SetRotation(const Vec3& rotation);
	Vec3 GetPosition() const;
	Vec3 GetScale() const;
	Vec3 GetRotation() const;
	Vec3 GetWorldPosition() const;
	Vec3 GetWorldScale() const;
	Vec3 GetWorldRotation() const;
	const glm::mat4& GetModelMatrix() const;

private:
	void SetParentMatrix(const glm::mat4& matrix);
	void UpdateModelMatrix();

protected:
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
	glm::mat4 m_parentMatrix;
	glm::mat4 m_matrix;

	friend class Renderer;
};
