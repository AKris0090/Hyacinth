#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SDL3/SDL.h"
#include "ecshelpers.h"
#include "input.h"
#include <iostream>

constexpr float PI = 3.14159265359f;

constexpr float BASE_MOVE_SPEED = 3.5f;
constexpr float BASE_LOOK_SPEED = 70.f;

class FPSCam {
private:
	float moveSpeed = 3.5f, lookSpeed = 70.f;
    std::vector<glm::vec4> untransformedPlanes;
    std::vector<glm::vec4> frustumCorners;

public:

    Transform transform;

    struct UniformPlanes {
        glm::vec4 planes[6];
    } m_frustumPlanes;

    struct CameraProps {
        float aspectRatio = 16.f / 9.f;
        float FOV = 90.f;
        float nearClip = 0.1f;
        float farClip = 40.0f;

        glm::vec3 forward = { 0.f, 0.f, -1.f };
        glm::vec3 right;
        glm::vec3 up = { 0.f, 1.f, 0.f };

        float yaw = 0.f, pitch = 0.f;

        glm::mat4 proj;
        glm::mat4 view;
    } m_props;

    bool dirtyProj;
    bool dirtyView;

	void update(float deltaTime);
    static void getFrustumPlanes(glm::vec4* planes, glm::mat4 matrix);

    FPSCam() {
        moveSpeed = BASE_MOVE_SPEED;
        lookSpeed = BASE_LOOK_SPEED;
        m_props.aspectRatio = 16.f / 9.f;
        m_props.FOV = 90.f;
        m_props.nearClip = 0.01f;
        m_props.farClip = 100.f;
        dirtyProj = true;
        dirtyView = true;

        update(0.f);
    };

    FPSCam(float aspect, float fov, float nearC, float farC) {
        moveSpeed = BASE_MOVE_SPEED;
        lookSpeed = BASE_LOOK_SPEED;
        m_props.aspectRatio = aspect;
        m_props.FOV = fov;
        m_props.nearClip = nearC;
        m_props.farClip = farC;
        dirtyProj = true;
        dirtyView = true;

        update(0.f);
    }
};