#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SDL3/SDL.h"

#define PI 3.14159265359f

struct FPSCam {
	glm::vec3 position{0.f, 0.f, 0.f};
	float pitch = 0.f, yaw = 0.f;
	float aspectRatio;
	float FOV;

	glm::mat4 getViewMatrix() const {
		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		front = glm::normalize(front);
		return glm::lookAt(position, position + front, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::mat4 getProjectionMatrix(float aspectRatio) const {
		glm::mat4 proj = glm::perspective(glm::radians(90.0f), aspectRatio, 0.1f, 1000.0f);
		proj[1][1] *= -1;
		return proj;
	}

	void processSDL(SDL_Event& event) {
		if (event.type == SDL_EVENT_MOUSE_MOTION) {
			yaw += ((float)event.motion.xrel * 5.f) * (PI / 180.0f);
			pitch -= ((float)event.motion.yrel * 5.f) * (PI / 180.0f);
		}
		if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
			bool isDown = (event.type == SDL_EVENT_KEY_DOWN);
			float cameraSpeed = 0.1f;

			SDL_Keycode key = event.key.scancode;

			if (isDown) {
				if (key == SDL_SCANCODE_W) {
					position += cameraSpeed * glm::vec3(0.0f, 0.0f, -1.0f);
				}
				if (key == SDL_SCANCODE_S) {
					position -= cameraSpeed * glm::vec3(0.0f, 0.0f, -1.0f);
				}
				if (key == SDL_SCANCODE_A) {
					position -= glm::normalize(glm::cross(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
				}
				if (key == SDL_SCANCODE_D) {
					position += glm::normalize(glm::cross(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f))) * cameraSpeed;
				}
			}
		}
	}
};