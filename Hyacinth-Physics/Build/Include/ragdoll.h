#pragma once

#include "PxRigidDynamic.h"
#include <vector>

struct RagdollJoint {
	uint32_t nodeIndex = 0;
	physx::PxRigidDynamic* rdJoint;  
};

struct Ragdoll {
	std::vector<RagdollJoint> joints;
};