#pragma once

#include <deque>
#include "glm/glm.hpp"

constexpr int MAX_TRACERS = 15.f;
constexpr float TRACER_TIME = 0.35f;

struct Tracer {
	float alpha;
	float currentTime;
	glm::mat4 worldMat;
};

class TracerManager {
public:
	std::deque<Tracer> tracers;

	void addTracer(glm::mat4 worldMatrix) {
		Tracer t;
		t.alpha = 1.f;
		t.currentTime = 0.f;
		t.worldMat = worldMatrix;

		tracers.push_back(t);
	}

	void updateTracers(float deltaTime) {
		int numP = 0;
		for (auto& t : tracers) {
			t.currentTime += deltaTime;
			if (t.currentTime > TRACER_TIME) {
				numP++;
			}
			t.alpha = t.currentTime / TRACER_TIME;
		}
		for (int i = 0; i < numP; i++) tracers.pop_front();
	}
};