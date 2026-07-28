#pragma once

#include "vulkan/vulkan.h"
#include "vkdeviceutils.h"
#include "vkmeshutils.h"
#include "vkmeshutils.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <vector>

namespace rt {
	struct nodeAccelBuildPacket {
		VkDeviceAddress vertexAddress;
		VkDeviceAddress indexAddress;

		struct primAccel {
			uint32_t vertexOffset;
			uint32_t firstIndex;
			uint32_t numVertices;
			uint32_t numIndices;
		};
		std::vector<primAccel> prims;
	};

	extern PFN_vkCreateAccelerationStructureKHR				CreateAS;
	extern PFN_vkCmdBuildAccelerationStructuresKHR			BuildAS;
	extern PFN_vkGetAccelerationStructureBuildSizesKHR		GetBuildSizes;
	extern PFN_vkGetAccelerationStructureDeviceAddressKHR	GetASAddress;
	extern PFN_vkCreateRayTracingPipelinesKHR				CreatePipeline;
	extern PFN_vkCmdTraceRaysKHR							Trace;
	extern PFN_vkGetRayTracingShaderGroupHandlesKHR			GetHandles;
	extern PFN_vkDestroyAccelerationStructureKHR			DestroyAS;

	extern VkPhysicalDeviceRayTracingPipelinePropertiesKHR		s_rtProperties;
	extern VkPhysicalDeviceAccelerationStructurePropertiesKHR	s_asProperties;

	void setRTProperties(VkPhysicalDeviceRayTracingPipelinePropertiesKHR newProps);
	void setASProperties(VkPhysicalDeviceAccelerationStructurePropertiesKHR newProps);
	void initAccelerationStructureFunctions(VkDevice& device);
}

class rtHelper {
public:
	AccelerationStructure m_tlAccelStrucutre;
	std::vector<AccelerationStructure> bottomLevelStructures;

	void setup();
	void createTopLevelAS();
	void shutdown();

	static void createAccelerationStructure(VkAccelerationStructureTypeKHR asType, AccelerationStructure& accelStruct, std::vector<VkAccelerationStructureGeometryKHR>& asGeometry, std::vector<VkAccelerationStructureBuildRangeInfoKHR>& asBuildRangeInfo, VkBuildAccelerationStructureFlagsKHR flags);
	static void nodeToAccelStructureGeometry(rt::nodeAccelBuildPacket packet, std::vector<VkAccelerationStructureGeometryKHR>& geometry, std::vector<VkAccelerationStructureBuildRangeInfoKHR>& rangeInfo);
	static void createBottomLevelAS(AccelerationStructure& accelStructure, rt::nodeAccelBuildPacket packet);

	void generateBLASForMesh(LightMesh* meshRef, VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress);
};