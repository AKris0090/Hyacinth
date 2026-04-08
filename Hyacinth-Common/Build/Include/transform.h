#pragma once

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/string_cast.hpp"
#include "glm/gtx/compatibility.hpp"

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

	glm::mat4 getPositionMatrix() const {
		return glm::translate(glm::mat4(1.0f), position);
	}

	glm::mat4 getRotationMatrix() const {
		return glm::toMat4(rotation);
	}

	void setRotationPitchYaw() {
		forward = glm::normalize(glm::vec3(
			cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
			sin(glm::radians(pitch)),
			sin(glm::radians(yaw)) * cos(glm::radians(pitch))
		));
		right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
		up = glm::normalize(glm::cross(right, forward));
	}

	Transform lerpTo(const Transform& other, float delta) {
		Transform t;
		t.position = glm::mix(position, other.position, delta);
		t.rotation = glm::slerp(rotation, other.rotation, delta);
		t.scale = glm::mix(scale, other.scale, delta);

		glm::vec3 fwd = t.rotation * glm::vec3(1.f, 0.f, 0.f);

		t.pitch = glm::degrees(glm::asin(fwd.y));
		t.yaw = glm::degrees(glm::atan2(fwd.z, fwd.x));

		t.setRotationPitchYaw();

		return t;
	}

	Transform() {};
	Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) : position(position), rotation(rotation), scale(scale) {}
};