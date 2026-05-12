#pragma once

#include <stdint.h>
#include "transform.h"

constexpr float CAM_LOOK_SPEED = 35.f;
constexpr float MOVE_SPEED = 2.85f;// 0.0285f;
constexpr int MAX_AMMO = 10;

enum FIRSTPERSON_STATE {
	IDLE_PISTOL,
	SHOOTING,
	RELOADING
};

struct WeaponController {
	float timeBetweenShots = 0.25f;
	float currentShotTimer = 0.f;

	int currentAmmo = 10;

	float reloadLength = 1.25f;
	float reloadTimer = 0.f;

	float shootLength = 0.25f;
	float shootTimer = 0.f;

	FIRSTPERSON_STATE state;

	// returns true if shooting is allowed this frame, false otherwise
	bool updateShooting(float deltaTime, bool lmbDown) {
		currentShotTimer += deltaTime;

		if (state == RELOADING) {
			reloadTimer += deltaTime;
			if (reloadTimer > reloadLength) {
				currentAmmo = MAX_AMMO;
				currentShotTimer = timeBetweenShots;
				state = IDLE_PISTOL;
			}
		}
		else if (state == SHOOTING) {
			shootTimer += deltaTime;
			if (shootTimer > shootLength) {
				currentShotTimer = 0.f;
				state = IDLE_PISTOL;
			}
		}
		else {
			if (currentShotTimer >= timeBetweenShots) {
				if (currentAmmo == 0) {
					reloadTimer = 0.f;
					state = RELOADING;
				}
				else {
					if (lmbDown) {
						currentAmmo--;
						shootTimer = 0.f;
						state = SHOOTING;
						return true;
					}
					currentShotTimer = timeBetweenShots;
				}
			}
		}

		return false;
	}
};

struct Entity {
	uint32_t id;
	float moveSpeed = MOVE_SPEED;
	float camSpeed = CAM_LOOK_SPEED;
	Transform transform;
	bool isMoving = false;
	bool shotAck = false;

	WeaponController pistolController;
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