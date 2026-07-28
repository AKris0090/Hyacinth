#include "gltfutils.h"
#include "vkimageutils.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tangentHelper.h"
#include <unordered_set>

#include "tiny_gltf.h"

AABB getBoundingBox(std::vector<Vertex>& vertices) {
    AABB bounds;
    bounds.min = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
    bounds.max = glm::vec4(glm::vec3(vertices[0].pos), 1.f);
    for (const auto& v : vertices) {
        bounds.grow(glm::vec4(v.pos.x, v.pos.y, v.pos.z, 1.f));
    }
    return bounds;
}

VulkanImage loadTexture(tinygltf::Model* model, VkFormat format, uint32_t imageIndex) {
    tinygltf::Image& curImage = model->images[imageIndex];
    VulkanImage texImage{};
    std::vector<unsigned char> rgba;
    rgba.resize(curImage.width * curImage.height * 4);

    switch (curImage.component) {
        case 4:
            memcpy(rgba.data(), curImage.image.data(), rgba.size());
            break;
        case 3:
        {
            std::vector<unsigned char> rgb;
            rgb.resize(curImage.width * curImage.height * 3);
            memcpy(rgb.data(), curImage.image.data(), rgb.size());
            for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
                rgba[j * 4] = rgb[j * 3];
                rgba[j * 4 + 1] = rgb[j * 3 + 1];
                rgba[j * 4 + 2] = rgb[j * 3 + 2];
                rgba[j * 4 + 3] = 255;
            }
            break;
        }
        case 1:
        {
            std::vector<unsigned char> r;
            r.resize(curImage.width * curImage.height);
            memcpy(r.data(), curImage.image.data(), r.size());
            for (size_t j = 0; j < (size_t)curImage.width * curImage.height; j++) {
                rgba[j * 4] = rgba[j * 4 + 1] = rgba[j * 4 + 2] = rgba[j * 4 + 3] = r[j];
            }
            break;
        }
    }
    VkExtent3D imageExtents{};
    imageExtents.width = curImage.width;
    imageExtents.height = curImage.height;
    imageExtents.depth = 1;
    texImage = vkimageutils::createTextureImage(rgba.data(), imageExtents, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    vkimageutils::createImageSampler(texImage);

    if (curImage.name.empty()) {
        curImage.name = "image_" + curImage.uri;
    }
    std::cout << "created image: " << curImage.name << std::endl;

    return texImage;
}