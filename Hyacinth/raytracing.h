#pragma once

#include "vulkan/vulkan.h"
#include "vkdeviceutils.h"
#include "vkmeshutils.h"
#include "vkmeshutils.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <vector>

namespace rt {
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
	extern std::vector<AccelerationStructure> bottomLevelStructures;

	void setRTProperties(VkPhysicalDeviceRayTracingPipelinePropertiesKHR newProps);
	void setASProperties(VkPhysicalDeviceAccelerationStructurePropertiesKHR newProps);
	void initAccelerationStructureFunctions(VkDevice& device);
}

class rtHelper {
public:
	AccelerationStructure m_tlAccelStrucutre;

	void setup();
	void createTopLevelAS();
	void shutdown();

	static void createAccelerationStructure(VkAccelerationStructureTypeKHR asType, AccelerationStructure& accelStruct, VkAccelerationStructureGeometryKHR& asGeometry, VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo, VkBuildAccelerationStructureFlagsKHR flags);
	static void nodeToAccelStructureGeometry(std::vector<glm::vec3> nodeVertices, std::vector<uint32_t>& nodeIndices, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo);
	static void createBottomLevelAS(AccelerationStructure& accelStructure, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices);
};