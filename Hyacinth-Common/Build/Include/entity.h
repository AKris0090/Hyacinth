#pragma once

#include <stdint.h>
#include "transform.h"

constexpr inline float CAM_LOOK_SPEED = 35.f;
constexpr inline float MOVE_SPEED = 0.05f;
constexpr inline int MAX_AMMO = 10;

enum WEAPON_STATE {
	PISTOL_EQUIP,
	PISTOL_IDLE,
	PISTOL_SHOOT,
	PISTOL_RELOAD,
	GRENADE_EQUIP,
	GRENADE_IDLE,
	GRENADE_THROW,

	NULL_STATE,
};

struct PistolController {
	float timeBetweenShots = 0.15f;
	float currentShotTimer = 0.f;

	float equipSpeed = 1.49985f;
	float equipTimer = 0.f;

	int currentAmmo = 10;

	float reloadLength = 2.833f;
	float reloadTimer = 0.f;

	float shootLength = 0.233f;
	float shootTimer = 0.f;

	void equipPistol(WEAPON_STATE& stateOut) {
		reloadTimer = 0.f;
		shootTimer = 0.f;
		equipTimer = 0.f;
		currentShotTimer = 0.f;
		stateOut = PISTOL_EQUIP;
	}

	// returns true if shooting is allowed this frame, false otherwise
	bool updateShooting(float deltaTime, bool lmbDown, bool rkeyDown, WEAPON_STATE& stateOut) {
		// equipping
		if (stateOut == PISTOL_EQUIP) {
			equipTimer += deltaTime;
			if (equipTimer >= equipSpeed) {
				currentShotTimer = timeBetweenShots;
				stateOut = PISTOL_IDLE;
			}
			return false;
		}

		currentShotTimer += deltaTime;

		// reloading
		if (stateOut == PISTOL_RELOAD) {
			reloadTimer += deltaTime;
			if (reloadTimer >= reloadLength) {
				currentAmmo = MAX_AMMO;
				currentShotTimer = timeBetweenShots;
				stateOut = PISTOL_IDLE;
			}
		}
		else if (stateOut == PISTOL_SHOOT) {
			shootTimer += deltaTime;
			if (shootTimer > shootLength) {
				currentShotTimer = 0.f;
				stateOut = PISTOL_IDLE;
			}
		}
		else {
			if (currentShotTimer >= timeBetweenShots) {
				if (currentAmmo == 0) {
					reloadTimer = 0.f;
					stateOut = PISTOL_RELOAD;
				}
				else {
					if (lmbDown) {
						currentAmmo--;
						shootTimer = 0.f;
						stateOut = PISTOL_SHOOT;
						return true;
					}
					if (rkeyDown && currentAmmo < MAX_AMMO) {
						reloadTimer = 0.f;
						stateOut = PISTOL_RELOAD;
					}
					currentShotTimer = timeBetweenShots;
				}
			}
		}

		return false;
	}
};

struct GrenadeController {
	float equipSpeed = 0.833f;
	float equipTimer = 0.f;

	float throwSpeed = 0.333f;
	float throwTimer = 0.f;

	void equipGrenade(WEAPON_STATE& stateOut) {
		equipTimer = 0.f;
		throwTimer = 0.f;
		stateOut = GRENADE_EQUIP;
	}

	bool updateShooting(float deltaTime, bool lmbDown, WEAPON_STATE& stateOut, bool& switchOut) {
		if (stateOut == GRENADE_EQUIP) {
			equipTimer += deltaTime;
			if (equipTimer >= equipSpeed) {
				stateOut = GRENADE_IDLE;
			}
			return false;
		}

		if (stateOut == GRENADE_IDLE && lmbDown) {
			stateOut = GRENADE_THROW;
			throwTimer = 0.f;
			return true;
		}
		if (stateOut == GRENADE_THROW) {
			throwTimer += deltaTime;
			if (throwTimer >= throwSpeed) {
				switchOut = true;
			}
		}

		return false;
	}
};

constexpr inline float CAM_RECOIL_TIME = 0.4f;
constexpr inline float CAM_RECOIL_AMOUNT = 6.5f;

struct CamRecoil {
	float recoilTimer = 0.f;

	void startRecoil() {
		recoilTimer = 0.f;
	}

	// returns pitch addition
	float updateRecoil(float deltaTime) {
		if (recoilTimer > CAM_RECOIL_TIME) {
			return 0.f;
		}
		recoilTimer += deltaTime;
		if (recoilTimer > CAM_RECOIL_TIME) {
			recoilTimer = CAM_RECOIL_TIME;
		}
		return CAM_RECOIL_AMOUNT * (1.f - (recoilTimer / CAM_RECOIL_TIME));
	}
};

enum EQUIPPED_WEAPON {
	PISTOL,
	GRENADE
};

enum ENTITY_TYPE : uint32_t {
	E_PLAYER = 0,
	E_GRENADE = 1
};

enum PLAYER_STATUS : uint32_t {
	NON_PLAYER = 0,
	REGULAR = 1,
	FLASHED = 2,
};

constexpr inline float FLASH_TIMER = 2.5f;
constexpr inline float FLASH_FADEOUT = 0.5f;

struct Entity {
	uint32_t id;
	ENTITY_TYPE type;
	float moveSpeed = MOVE_SPEED;
	float camSpeed = CAM_LOOK_SPEED;
	Transform transform;
	bool isMoving = false;
	bool shotAck = false;
	bool shot = false;
	float health = 1.f;
	float flashPercentage = 0.f;
	float flashTimer = FLASH_TIMER + FLASH_FADEOUT;
	float flashNDCX, flashNDCY;
	glm::vec3 hitPos;

	bool updated = false;

	EQUIPPED_WEAPON currentWeapon;
	PLAYER_STATUS currentStatus;
	WEAPON_STATE currentState = NULL_STATE;

	GrenadeController grenadeController;
	PistolController pistolController;
	CamRecoil recoil;

	void startFlash(float ndcX, float ndcY) {
		flashPercentage = 1.f;
		flashNDCX = ndcX;
		flashNDCY = ndcY;
		flashTimer = 0.f;
	}

	void updateFlash(float deltaTime) {
		flashTimer += deltaTime;
		if (flashTimer < FLASH_TIMER) {
			flashPercentage = 1.f;
			return;
		}
		else if (flashTimer > (FLASH_TIMER + FLASH_FADEOUT)) {
			flashTimer = FLASH_TIMER + FLASH_FADEOUT;
		}

		flashPercentage = (1.f - ((flashTimer - FLASH_TIMER) / (FLASH_FADEOUT)));
	}

	void takeDamage() {
		health -= 0.1f;
		if (health < 0.f) {
			health = 0.f;
		}
	}

	void updateWeaponState(float deltaTime, bool pistolEquipKeyDown, bool grenadeEquipKeyDown, bool lmbDown, bool rKeyDown, bool& shootingOut, EQUIPPED_WEAPON& weaponOut) {
		if (currentState == PISTOL_IDLE || currentState == GRENADE_IDLE || currentState == GRENADE_EQUIP || currentState == PISTOL_EQUIP) {
			if (pistolEquipKeyDown && currentWeapon == GRENADE) {
				currentWeapon = PISTOL;
				pistolController.equipPistol(currentState);
			}
			else if (grenadeEquipKeyDown && currentWeapon == PISTOL) {
				currentWeapon = GRENADE;
				grenadeController.equipGrenade(currentState);
			}
		}
		
		if (currentWeapon == PISTOL) {
			shootingOut = pistolController.updateShooting(deltaTime, lmbDown, rKeyDown, currentState);
		}
		else if (currentWeapon == GRENADE) {
			bool needSwitch = false;
			shootingOut = grenadeController.updateShooting(deltaTime, lmbDown, currentState, needSwitch);
			if (needSwitch) {
				currentWeapon = PISTOL;
				pistolController.equipPistol(currentState);
			}
		}

		weaponOut = currentWeapon;
	}

	Entity() {
		currentWeapon = PISTOL;
		pistolController.equipPistol(currentState);
	}
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