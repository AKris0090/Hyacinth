#pragma once

#include "vkimageutils.h"
#include "vkdescriptorutils.h"
#include "hcinth_animatedmesh.h"
#include "hcinth_mesh.h"

#include "fullscreen_quad.h"
#include "unit_cube.h"

#include "stb_image.h"

constexpr int DUMMY_NORMAL_TEX_INDEX = 0;
constexpr int DUMMY_METALROUGH_TEX_INDEX = 1;
constexpr int DUMMY_COLOR_TEX_INDEX = 2;

const std::vector<std::pair<std::string, VkFormat>> DUMMY_TEX_PATHS = {
	{"./shaders/dummyNormal.png", VK_FORMAT_R8G8B8A8_UNORM },
	{ "./shaders/dummyMetallicRoughness.png", VK_FORMAT_R8G8B8A8_UNORM },
	{ "./shaders/dummyColor.png", VK_FORMAT_R8G8B8A8_SRGB }
};

static std::string getFilePathExtension(const std::string& FileName) {
	if (FileName.find_last_of(".") != std::string::npos)
		return FileName.substr(FileName.find_last_of(".") + 1);
	return "";
}

// need mesh represnetation object to hold a global ID and a base vertex offset into the global vertex buffer

// KEY IDEA: gltfutils should only be for loading objects into memory. Scene should only be for objects and renderItems, objects should have a mesh reference ( either static or skinned )
// if object has a static mesh, then check if the mesh ID is already in static mesh renderItems. If present, add to instance count and throw in the transform matrix into buffer                                        

// one object per renderable instance in the scene

struct MaterialInstance {
	uint32_t baseColorIndex;
	uint32_t normalIndex;
	uint32_t metallicRoughnessIndex;
	float alphaCutoff = 0.5f;
};

class HAssetDrawer {
public:
	std::vector<Vertex>		vertices;
	VulkanBuffer			g_vertexBuffer;

	std::vector<uint32_t>	indices;
	VulkanBuffer			g_indexBuffer;

	uint32_t meshCount = 0;
	std::unordered_map<std::string, HMesh> meshes;
	std::unordered_map<std::string, HSkinnedMesh> skinnedMeshes;

	std::vector<MaterialInstance> materials;
	VulkanBuffer materialInfoBuffer;

	std::vector<VulkanImage> textures;

	HMesh* getStaticMeshRef(std::string meshName);
	HSkinnedMesh* getAnimatedMeshRef(std::string meshName);

	void uploadBuffersToGPU();
	void createAddTextureFromFile(std::string filepath, VkFormat format);
	void addDummyTextures();
	void bindTextures(VkDescriptorSet& textureSet);

	HAssetDrawer();

	~HAssetDrawer() {
		if (g_vertexBuffer.buffer) vkdeviceutils::destroyBuffer(g_vertexBuffer);
		if (g_indexBuffer.buffer) vkdeviceutils::destroyBuffer(g_indexBuffer);
		if (materialInfoBuffer.buffer) vkdeviceutils::destroyBuffer(materialInfoBuffer);

		for (auto& t : textures) {
			vkimageutils::destroyImage(t);
		}
	}
};