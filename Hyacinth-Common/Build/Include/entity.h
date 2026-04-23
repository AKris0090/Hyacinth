#pragma once

#include <stdint.h>
#include "transform.h"

constexpr float CAM_LOOK_SPEED = 35.f;
constexpr float MOVE_SPEED = 0.0285f;

struct Entity {
	uint32_t id;
	float moveSpeed = MOVE_SPEED;
	float camSpeed = CAM_LOOK_SPEED;
	Transform transform;
	bool isMoving = false;
};

struct PhysicsEnt {
	float yVel = 0.f;
	bool isGrounded = true;

	float yPosAddVelocity(float yPos, float dT) {
		return (yPos + (yVel * dT));
	}
};