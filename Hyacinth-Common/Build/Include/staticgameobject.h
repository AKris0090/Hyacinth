#pragma once

#include "light_loader.h"

class HStaticGameObject {
public:
	Transform transform;
	LightMesh* mesh;

	HStaticGameObject() {};
	HStaticGameObject(LightMesh* meshRef);
};