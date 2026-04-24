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
	bool shotAck = false;
};

struct PhysicsEnt {
	float yVel = 0.f;
	bool isGrounded = true;

	float yPosAddVelocity(float yPos, float dT) {
		return (yPos + (yVel * dT));
	}
};

// left-right strafe
struct BotBehavior {
	bool active = false;

	enum BEHAVIOR {
		STRAFE_LEFT,
		STRAFE_RIGHT
	};
	float strafeTimer = 0.f;
	float strafeDuration = 3.f;
	BEHAVIOR currentBehavior = BEHAVIOR::STRAFE_LEFT;

	int8_t update(float dT) {
		strafeTimer += dT;
		if (strafeTimer >= strafeDuration) {
			strafeTimer = fmod(strafeTimer, strafeDuration);
			if (currentBehavior == BEHAVIOR::STRAFE_LEFT) {
				currentBehavior = BEHAVIOR::STRAFE_RIGHT;
			}
			else {
				currentBehavior = BEHAVIOR::STRAFE_LEFT;
			}
		}

		if (currentBehavior == BEHAVIOR::STRAFE_LEFT) {
			return -1;
		}
		else {
			return 1;
		}
	}
};