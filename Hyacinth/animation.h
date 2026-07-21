#pragma once

#include "entity.h"

enum TURN_ANIM_STATE {
	NEEDS_TURN_LEFT,
	NEEDS_TURN_RIGHT,
	TURNING,
	IDLE
};

enum CURRENT_PLAYER_MOTION_STATE {
	MOVING, 
	STILL
};

static float yawFromQuaternion(glm::quat q) {
	return glm::degrees(atan2(
		2.0f * (q.w * q.y + q.x * q.z),
		1.0f - 2.0f * (q.y * q.y + q.x * q.x)
	));
};