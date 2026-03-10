#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "transform.h"
#include "vkdeviceutils.h"
#include "input.h"

constexpr float PI = 3.14159265359f;

constexpr float BASE_MOVE_SPEED = 3.5f;
constexpr float BASE_LOOK_SPEED = 70.f;

struct CameraFrustumPlanes {
    glm::vec4 planes[6];
};

struct CameraUniformProperties {
	VulkanBuffer camFrustumUniformBuffer;
	VkDescriptorSet m_frustumPlaneUniformSet;
};

class Camera {
private:
	float m_moveSpeed = BASE_MOVE_SPEED, m_lookSpeed = BASE_LOOK_SPEED;
    void setViewMatrix();
    void setProjectionMatrix();
    
public:
    bool m_dirtyProj, m_dirtyView, m_dirtyMovement;
    Transform m_transform;
    CameraFrustumPlanes m_frustumPlanes;
    // CameraUniformProperties m_frustumProperties[MAX_FRAMES_IN_FLIGHT];

    float m_aspectRatio, m_FOV , m_zNear, m_zFar, m_yaw = 0.f, m_pitch = 0.f;

    glm::vec3 m_forward = { 0.f, 0.f, -1.f };
    glm::vec3 m_right;
    glm::vec3 m_up = { 0.f, 1.f, 0.f };

    glm::mat4 m_proj;
    glm::mat4 m_view;

    Camera() {};
    Camera(float aspect, float fov, float nearC, float farC);
    void update(float deltaTime, bool moveMouse, int imageIndex);

    static void GetFrustumPlanes(glm::vec4* planes, glm::mat4 matrix);
};