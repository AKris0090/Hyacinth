#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SDL3/SDL.h"
#include "ecshelpers.h"
#include "input.h"

constexpr float PI = 3.14159265359f;

struct FPSCam {
	float pitch = 0.f, yaw = 0.f, moveSpeed = 3.5f, lookSpeed = 45.f;
	float aspectRatio;
	float FOV;

    transform transform;
    glm::vec3 forward, right, up = { 0.f, 1.f, 0.f };

	glm::mat4 getViewMatrix() const {
		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		front = glm::normalize(front);
		return glm::lookAt(transform.position, transform.position + front, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::mat4 getProjectionMatrix() const {
		glm::mat4 proj = glm::perspective(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);
		proj[1][1] *= -1;
		return proj;
	}

    void update(float deltaTime) {
        std::pair<float, float> mouseMotion = Input::getMouseMotion();
        yaw += mouseMotion.first * lookSpeed * deltaTime;
        pitch -= mouseMotion.second * lookSpeed * deltaTime;

        pitch = glm::clamp(pitch, -89.9f, 89.9f);

        forward = glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
		));
		right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		up = glm::normalize(glm::cross(right, forward));

        glm::vec3 localDisplacement{ 0.0f, 0.0f, 0.0f };
        if (Input::forwardKeyDown())    localDisplacement += forward;
        if (Input::backwardKeyDown())   localDisplacement -= forward;
        if (Input::rightKeyDown())      localDisplacement += right;
        if (Input::leftKeyDown())       localDisplacement -= right;
        if (Input::upKeyDown())         localDisplacement += up;
        if (Input::downKeyDown())       localDisplacement -= up;

        if (glm::length(localDisplacement) > 0) {
            transform.position += glm::normalize(localDisplacement) * moveSpeed * deltaTime;
        }
    }
};