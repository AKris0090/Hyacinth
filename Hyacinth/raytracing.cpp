#include "raytracing.h"

namespace rt {
    PFN_vkCreateAccelerationStructureKHR CreateAS = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR BuildAS = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR GetBuildSizes = nullptr;
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
}

// this should translate a gltfNode to a geometry structure
static void primitiveToGeometry(gltfNode* node, VkAccelerationStructureGeometryKHR& geometry, VkAccelerationStructureBuildRangeInfoKHR& rangeInfo)
{
    const auto triangleCount = static_cast<uint32_t>(node->indices.size() / 3U);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = node->nodeVertexBuffer.gpuAddress },
        .vertexStride = sizeof(Vertex),
        .maxVertex = static_cast<uint32_t>(node->vertices.size()),
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = node->nodeIndexBuffer.gpuAddress },
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
    VulkanBuffer scratchBuffer = vkdeviceutils::createBuffer(*ctx.device, *ctx.allocator, scratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VMA_MEMORY_USAGE_AUTO, 0);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .size = asBuildSize.accelerationStructureSize, 
        .type = asType
    };

    VK_CHECK(rt::CreateAS(*ctx.device, &createInfo, nullptr, &accelStruct.accel));

    vkdeviceutils::executeSingleTimeCommands(ctx, [&](VkCommandBuffer& cmd) {
        asBuildInfo.dstAccelerationStructure = accelStruct.accel;
        asBuildInfo.scratchData.deviceAddress = scratchBuffer.gpuAddress;

        VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &asBuildRangeInfo;
        rt::BuildAS(cmd, 1, &asBuildInfo, &pBuildRangeInfo);
    });

    vkdeviceutils::destroyBuffer(*ctx.allocator, scratchBuffer);
}

// this should loop over all of the nodes in the scenegraph and create their accelerations structures
void rtHelper::createBottomLevelAS(SceneGraph& scene) {
    m_blAccelStructures.resize(scene.drawCommands.size());

    std::cout << "building bottom-level accel structures" << std::endl;
}

// this creates a single TLAS instance per BLAS (gltfNode). Matrix is also stored in gltfNode
void rtHelper::createTopLevelAS(SceneGraph& scene) {
    auto toTransformMatrixKHR = [](const glm::mat4& m) {
        VkTransformMatrixKHR t;
        memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
        return t;
        };

    // Prepare instance data for TLAS
    std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
    tlasInstances.reserve(1);

    uint32_t meshIndex = 0;
    for(const auto& obj : scene.objects) {
        for (const auto& node : obj.nodes) {
            VkAccelerationStructureInstanceKHR asInstance{};
            asInstance.transform = toTransformMatrixKHR(node.get()->worldTransform);
            asInstance.instanceCustomIndex = meshIndex;                       // gl_InstanceCustomIndexEXT
            // asInstance.accelerationStructureReference = m_blasAccel[instance.meshIndex].address;  // Will be set in Phase 3
            asInstance.instanceShaderBindingTableRecordOffset = 0;
            asInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
            asInstance.mask = 0xFF;
            tlasInstances.emplace_back(asInstance);
            meshIndex++;
        }
    }

    std::cout << "building top-level accel structures" << std::endl;
}

void rtHelper::setup(DeviceContext& ctx, SceneGraph& scene) {
    rt::initAccelerationStructureFunctions(*ctx.device);
    createBottomLevelAS(scene);
    createTopLevelAS(scene);
}