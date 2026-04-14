#pragma once

#include <stdint.h>
#include "transform.h"

constexpr float CAM_LOOK_SPEED = 35.f;
constexpr float MOVE_SPEED = 0.05f;

struct Entity {
	uint32_t id;
	float moveSpeed = MOVE_SPEED;
	float camSpeed = CAM_LOOK_SPEED;
	Transform transform;
};