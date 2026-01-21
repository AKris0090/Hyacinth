#include "raytracing.h"

namespace rt {
    PFN_vkCreateAccelerationStructureKHR CreateAS = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR BuildAS = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR GetBuildSizes = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR GetASAddress = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR createPipeline = nullptr;
    PFN_vkCmdTraceRaysKHR Trace = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR GetHandles = nullptr;
}

void rt::initAccelerationStructureFunctions(VkDevice& device) {
    rt::CreateAS = (PFN_vkCreateAccelerationStructureKHR)
        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    if (!rt::CreateAS) throw std::runtime_error("Failed to load vkCreateAccelerationStructureKHR");

    rt::BuildAS = (PFN_vkCmdBuildAccelerationStructuresKHR)
        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    if (!rt::BuildAS) throw std::runtime_error("Failed to load vkCmdBuildAccelerationStructuresKHR");

    rt::GetBuildSizes = (PFN_vkGetAccelerationStructureBuildSizesKHR)
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    if (!rt::GetBuildSizes) throw std::runtime_error("Failed to load vkGetAccelerationStructureBuildSizesKHR");

    rt::GetASAddress = (PFN_vkGetAccelerationStructureDeviceAddressKHR)
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    if (!rt::GetASAddress) throw std::runtime_error("Failed to load vkGetAccelerationStructureDeviceAddressKHR");

    rt::createPipeline = (PFN_vkCreateRayTracingPipelinesKHR)
        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    if (!rt::createPipeline) throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");

    rt::Trace = (PFN_vkCmdTraceRaysKHR)
        vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
    if (!rt::Trace) throw std::runtime_error("Failed to load vkCmdTraceRaysKHR");

    rt::GetHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    if (!rt::GetHandles) throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR");
}

// this should translate a gltfNode to a geometry structure
static void primitiveToGeometry(gltfNode* node, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
{
    const auto triangleCount = static_cast<uint32_t>(node->indices.size() / 3U);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = node->accelStructureVertexBuffer.gpuAddress },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = static_cast<uint32_t>(node->vertices.size()) - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = node->accelStructureIndexBuffer.gpuAddress },
    };

    geometry = VkAccelerationStructureGeometryKHR{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR | VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    rangeInfo = VkAccelerationStructureBuildRangeInfoKHR{ .primitiveCount = triangleCount };
}

void rtHelper::createAccelerationStructure(DeviceContext & ctx,
                                           VkAccelerationStructureTypeKHR asType,
                                           AccelerationStructure& accelStruct,
                                           VkAccelerationStructureGeometryKHR& asGeometry,
                                           VkAccelerationStructureBuildRangeInfoKHR& asBuildRangeInfo,
                                           VkBuildAccelerationStructureFlagsKHR flags) {
    auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

    VkAccelerationStructureBuildGeometryInfoKHR asBuildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = asType,
        .flags = flags,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &asGeometry,
    };
    std::vector<uint32_t> maxPrimCount(1);
    maxPrimCount[0] = asBuildRangeInfo.primitiveCount;

    VkAccelerationStructureBuildSizesInfoKHR asBuildSize{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    rt::GetBuildSizes(*ctx.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asBuildInfo,
        maxPrimCount.data(), &asBuildSize);

    VkDeviceSize scratchSize = alignUp(asBuildSize.buildScratchSize, m_asProperties.minAccelerationStructureScratchOffsetAlignment);
    VulkanBuffer scratchBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, scratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0, m_asProperties.minAccelerationStructureScratchOffsetAlignment);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .size = asBuildSize.accelerationStructureSize,
        .type = asType
    };
    accelStruct.buffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, createInfo.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    createInfo.buffer = accelStruct.buffer.buffer;
    VK_CHECK(rt::CreateAS(*ctx.device, &createInfo, nullptr, &accelStruct.accel));
    VkAccelerationStructureDeviceAddressInfoKHR info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = accelStruct.accel;
    accelStruct.address = rt::GetASAddress(*ctx.device, &info);

    vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
        asBuildInfo.dstAccelerationStructure = accelStruct.accel;
        asBuildInfo.scratchData.deviceAddress = scratchBuffer.gpuAddress;

        VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &asBuildRangeInfo;
        rt::BuildAS(cmd, 1, &asBuildInfo, &pBuildRangeInfo);
    });

    vkdeviceutils::destroyBuffer(*ctx.allocator, scratchBuffer);
}

// this should loop over all of the nodes in the scenegraph and create their accelerations structures
void rtHelper::createBottomLevelAS(DeviceContext& ctx, SceneGraph& scene) {
    m_blAccelStructures.resize(scene.numNodes);
    std::cout << "building bottom-level accel structures" << std::endl;

    uint32_t id = 0;
    for(const auto& obj : scene.objects) {
        for (const auto& node : obj.nodes) {
            VkAccelerationStructureGeometryKHR       asGeometry{};
            VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
            primitiveToGeometry(node.get(), asGeometry, asBuildRangeInfo);
            createAccelerationStructure(ctx, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, m_blAccelStructures[id], asGeometry, asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
            id++;
        }
    }
}

// this creates a single TLAS instance per BLAS (gltfNode). Matrix is also stored in gltfNode
void rtHelper::createTopLevelAS(DeviceContext& ctx, SceneGraph& scene) {
    auto toTransformMatrixKHR = [](const glm::mat4& m) {
        VkTransformMatrixKHR t;
        memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
        return t;
        };

    // Prepare instance data for TLAS
    std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
    tlasInstances.reserve(scene.numNodes);

    uint32_t meshIndex = 0;
    for(const auto& obj : scene.objects) {
        for (const auto& node : obj.nodes) {
            VkAccelerationStructureInstanceKHR asInstance{};
            asInstance.transform = toTransformMatrixKHR(node.get()->worldTransform);
            asInstance.instanceCustomIndex = meshIndex;                       // gl_InstanceCustomIndexEXT
            asInstance.accelerationStructureReference = m_blAccelStructures[meshIndex].address;
            asInstance.instanceShaderBindingTableRecordOffset = 0;
            asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
            asInstance.mask = 0xFF;
            tlasInstances.emplace_back(asInstance);
            meshIndex++;
        }
    }

    std::cout << "building top-level accel structures" << std::endl;

    VulkanBuffer tlasInstanceBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0);
    vkdeviceutils::uploadToBuffer(ctx, tlasInstanceBuffer, tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR), tlasInstances.data());
    {
        VkAccelerationStructureGeometryKHR       asGeometry{};
        VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};

        // Convert the instance information to acceleration structure geometry, similar to primitiveToGeometry()
        VkAccelerationStructureGeometryInstancesDataKHR geometryInstances{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                                                                          .data = {.deviceAddress = tlasInstanceBuffer.gpuAddress} };
        asGeometry = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
                            .geometry = {.instances = geometryInstances} };
        asBuildRangeInfo = { .primitiveCount = scene.numNodes };

        createAccelerationStructure(ctx, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, m_tlAccelStrucutre, asGeometry, asBuildRangeInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    }
    vkdeviceutils::destroyBuffer(*ctx.allocator, tlasInstanceBuffer);
}

void rtHelper::setup(DeviceContext& ctx, SceneGraph& scene) {
    rt::initAccelerationStructureFunctions(*ctx.device);
    createBottomLevelAS(ctx, scene);
    createTopLevelAS(ctx, scene);
}