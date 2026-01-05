#pragma once

#include <chrono>
#include <glm/glm.hpp>

namespace Time {
	void updateTime();
	std::chrono::time_point<std::chrono::system_clock> getCurrentTime();
	float getDeltaTime();
	void setInitialTime();
};