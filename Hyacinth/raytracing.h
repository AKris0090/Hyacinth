#pragma once

#include "vulkan/vulkan.h"
#include "vkdeviceutils.h"
#include "vkmeshutils.h"
#include "gltfutils.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <vector>

struct AccelerationStructure
{
	VkAccelerationStructureKHR accel{};
	VkDeviceAddress            address{};
	VulkanBuffer               buffer;
};

namespace rt {
	extern PFN_vkCreateAccelerationStructureKHR CreateAS;
	extern PFN_vkCmdBuildAccelerationStructuresKHR BuildAS;
	extern PFN_vkGetAccelerationStructureBuildSizesKHR GetBuildSizes;
	extern PFN_vkGetAccelerationStructureDeviceAddressKHR GetASAddress;
	extern PFN_vkCreateRayTracingPipelinesKHR createPipeline;
	extern PFN_vkCmdTraceRaysKHR Trace;
	extern PFN_vkGetRayTracingShaderGroupHandlesKHR GetHandles;

	void initAccelerationStructureFunctions(VkDevice& device);
}

class rtHelper {
private:
	std::vector<AccelerationStructure>	m_blAccelStructures;

	void createAccelerationStructure(DeviceContext& ctx,
		VkAccelerationStructureTypeKHR asType,
		AccelerationStructure& accelStruct,
		VkAccelerationStructureGeometryKHR& asGeometry,
		VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,
		VkBuildAccelerationStructureFlagsKHR flags);

	void createBottomLevelAS(DeviceContext& ctx, SceneGraph& scene);
	void createTopLevelAS(DeviceContext& ctx, SceneGraph& scene);

public:
	AccelerationStructure				m_tlAccelStrucutre;
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;

	void setup(DeviceContext& ctx, SceneGraph& scene);

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
};