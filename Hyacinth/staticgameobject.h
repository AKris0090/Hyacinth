#pragma once

#include "hcinth_mesh.h"

class HStaticGameObject {
public:
	Transform transform;
	HMesh* mesh;

	HStaticGameObject() {};
	HStaticGameObject(HMesh* meshRef);
};