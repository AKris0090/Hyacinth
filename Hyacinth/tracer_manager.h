#pragma once

#include <deque>
#include "glm/glm.hpp"
#include "staticgameobject.h"

constexpr int MAX_TRACERS = 15.f;
constexpr float TRACER_TIME = 0.35f;

struct Tracer {
	float alpha;
	float currentTime;
	glm::mat4 matrix;
	HStaticGameObject* gameObject;
};

class TracerManager {
public:
	std::deque<Tracer> tracers;

	void addTracer(glm::mat4 worldMatrix, HMesh* meshRef) {
		Tracer t;
		t.alpha = 1.f;
		t.currentTime = 0.f;
		t.matrix = worldMatrix;
		t.gameObject = new HStaticGameObject(meshRef);

		tracers.push_back(t);
	}

	void updateTracers(float deltaTime) {
		int numP = 0;
		for (auto& t : tracers) {
			t.currentTime += deltaTime;
			if (t.currentTime > TRACER_TIME) {
				numP++;
			}
			t.alpha = 1.f - (t.currentTime / TRACER_TIME);
		}
		for (int i = 0; i < numP; i++) tracers.pop_front();
	}
};