#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/string_cast.hpp"

struct Transform {
	glm::vec3 position{ 0.f, 0.f, 0.f };
	glm::quat rotation{ 1.f, 0.f, 0.f, 0.f };
	glm::vec3 scale{ 1.f, 1.f, 1.f };

	glm::vec3 forward = { 1.f, 0.f, 0.f };
	glm::vec3 right = { 0.f, 0.f, 0.f };
	glm::vec3 up = { 0.f, 1.f, 0.f };

	float pitch = 0.f, yaw = 0.f;

	bool dirty = 0;

	glm::mat4 getMatrix() const {
		glm::mat4 ret = glm::translate(glm::mat4(1.0f), position);
		ret *= glm::toMat4(rotation);
		ret = glm::scale(ret, scale);
		return ret;
	}

	Transform() {};
	Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) : position(position), rotation(rotation), scale(scale) {}
};