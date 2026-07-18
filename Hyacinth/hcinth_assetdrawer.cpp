#include "hcinth_assetdrawer.h"

HMesh* HAssetDrawer::getStaticMeshRef(std::string meshName) {
	auto found = meshes.find(meshName);
	if (found == meshes.end()) {
		throw std::runtime_error("yo TWIN that mesh does NOT EXIST");
	}
	else {
		return &found->second;
	}
}

HSkinnedMesh* HAssetDrawer::getAnimatedMeshRef(std::string meshName) {
	auto found = skinnedMeshes.find(meshName);
	if (found == skinnedMeshes.end()) {
		throw std::runtime_error("yo TWIN that skinned mesh does NOT EXIST");
	}
	else {
		return &found->second;
	}
}

void HAssetDrawer::uploadBuffersToGPU() {
	// vertex buffer
	size_t vertexBufferSize = sizeof(Vertex) * vertices.size();
	g_vertexBuffer = vkdeviceutils::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "vertex_buffer");
	vkdeviceutils::uploadToBuffer(g_vertexBuffer, vertexBufferSize, vertices.data());

	// index buffer
	size_t indexBufferSize = sizeof(uint32_t) * indices.size();
	g_indexBuffer = vkdeviceutils::createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "index_buffer");
	vkdeviceutils::uploadToBuffer(g_indexBuffer, indexBufferSize, indices.data());

	// material info buffer
	size_t materialDataBufferSize = sizeof(MaterialInstance) * materials.size();
	materialInfoBuffer = vkdeviceutils::createBuffer(materialDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0, "material_info_ssbo");
	vkdeviceutils::uploadToBuffer(materialInfoBuffer, materialDataBufferSize, materials.data());
}

void HAssetDrawer::bindTextures(VkDescriptorSet& textureSet) {
	uint32_t textureOffset = 0;
	for (auto& tex : textures) {
		vkdescriptorutils::queueWriteImage(textureSet, 0, textureOffset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		textureOffset++;
	}
}

void HAssetDrawer::createAddTextureFromFile(std::string filepath, VkFormat format) {
	VulkanImage texImage{};
	stbi_uc* pixels = nullptr;
	int texWidth, texHeight, texChannels;
	pixels = stbi_load(filepath.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		throw std::runtime_error("failed to load dummy image " + filepath + "!");
	}
	texImage.extent.width = texWidth;
	texImage.extent.height = texHeight;

	VkExtent3D imageExtents{};
	imageExtents.width = texImage.extent.width;
	imageExtents.height = texImage.extent.height;
	imageExtents.depth = 1;

	texImage = vkimageutils::createTextureImage((void*)pixels, imageExtents, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
	vkimageutils::createImageSampler(texImage);
	textures.push_back(texImage);

	stbi_image_free(pixels);
}

void HAssetDrawer::addDummyTextures() {
	for (const auto& [path, format] : DUMMY_TEX_PATHS) {
		createAddTextureFromFile(path, format);
	}
}

void HAssetDrawer::addUITextures() {
	for (const auto& [path, format] : UI_TEXTURE_PATHS) {
		createAddTextureFromFile(path, format);
	}
	for (const auto& [path, format] : WORLD_UI_TEXTURE_PATHS) {
		createAddTextureFromFile(path, format);
	}
}

HAssetDrawer::HAssetDrawer() {
	FullscreenQuad::addFullscreenQuad(vertices, indices);
	UnitCube::addUnitCube(vertices, indices);
}

void HAssetDrawer::shutdown() {
	vkdeviceutils::destroyBuffer(g_vertexBuffer);
	vkdeviceutils::destroyBuffer(g_indexBuffer);
	vkdeviceutils::destroyBuffer(materialInfoBuffer);

	for (auto& t : textures) {
		vkimageutils::destroyImage(t);
	}
}