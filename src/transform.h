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
	void SetRotation(const Vec3& rotation);
	Vec3 GetPosition() const;
	Vec3 GetScale() const;
	Vec3 GetRotation() const;
	glm::mat4 CalculateModelMatrix() const;

protected:
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
};
