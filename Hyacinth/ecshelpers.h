#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct transform {
	glm::vec3 position{0.f, 0.f, 0.f};
	glm::quat rotation{ 0.f, 0.f, 0.f, 1.f };
	glm::vec3 scale{ 1.f, 1.f, 1.f };

	glm::mat4 getMatrix() const {
		glm::mat4 ret = glm::translate(glm::mat4(1.0f), position);
		ret *= glm::toMat4(rotation);
		ret = glm::scale(ret, scale);
		return ret;
	}
};