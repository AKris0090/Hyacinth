#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "transform.h"
#include "vkdeviceutils.h"
#include "hyacinth_network.h"
#include "input.h"

constexpr float PI = 3.14159265359f;

constexpr float BASE_MOVE_SPEED = 3.5f;
constexpr float BASE_LOOK_SPEED = 70.f;

constexpr float BASE_FOV = 90.f;
constexpr float SPRINT_FOV = 98.f;
constexpr float FOV_LERP_LENGTH = 0.125f;

struct CameraFrustumPlanes {
    glm::vec4 planes[6];
};

class FOVModifier {
private:
    bool prevSprint = false;
    float lerpFOVFrom = 0.f;
    float lerpFOVTo = 0.f;
    float FOVlerpTimer = FOV_LERP_LENGTH;

    void startFOVLerpUp() {
        lerpFOVFrom = 0.f;
        lerpFOVTo = SPRINT_FOV - BASE_FOV;

        FOVlerpTimer = (fovLerpModifier / ((glm::max)(lerpFOVFrom, 0.001f))) * FOV_LERP_LENGTH;
    }

    void startFOVLerpDown() {
        lerpFOVFrom = SPRINT_FOV - BASE_FOV;
        lerpFOVTo = 0.f;

        FOVlerpTimer = (1.f - (fovLerpModifier / (lerpFOVFrom))) * FOV_LERP_LENGTH;
    }

public:
    float fovLerpModifier = 0.f;

    void updateModifier(float deltaTime);
    void updateSprintFOV(bool sprint);
};

constexpr float BASE_CROUCH = 1.85f;
constexpr float SLIDE_CROUCH = 0.925f;
constexpr float CROUCH_LERP_LENGTH = 0.1f;

class CrouchModifier {
private:
    bool prevCrouch = false;
    float lerpFrom = 1.85f;
    float lerpTo = 1.85f;
    float lerpTimer = CROUCH_LERP_LENGTH;

    void startCrouchLerp() {
        lerpFrom = BASE_CROUCH;
        lerpTo = SLIDE_CROUCH;

        lerpTimer = ((lerpFrom - crouchLerpMod) / (lerpFrom - lerpTo)) * CROUCH_LERP_LENGTH;
    }

    void undoCrouchLerp() {
        lerpFrom = SLIDE_CROUCH;
        lerpTo = BASE_CROUCH;

        lerpTimer = ((crouchLerpMod - lerpFrom) / (lerpTo - lerpFrom)) * CROUCH_LERP_LENGTH;
    }

public:
    float crouchLerpMod = BASE_CROUCH;

    void updateModifier(float deltaTime);
    void updateCrouchHeight(bool crouch);
};

class Camera {
private:
	float m_moveSpeed = BASE_MOVE_SPEED, m_lookSpeed = BASE_LOOK_SPEED;

    void setViewMatrix(Transform& t);
    void setProjectionMatrix();
    
public:
    bool m_dirtyProj, m_dirtyView, m_dirtyMovement;
    Transform m_transform;
    float prevYaw, prevPitch;
    CameraFrustumPlanes m_frustumPlanes;
    FOVModifier fovMod;
    CrouchModifier crouchMod;
    
    Transform m_flyTransform;

    float m_aspectRatio, m_FOV, m_zNear, m_zFar;

    glm::mat4 m_proj;
    glm::mat4 m_view;

    Camera() {};
    Camera(float aspect, float fov, float nearC, float farC);
    void update(bool flycam, float deltaTime);

    void updateFlyCamera(const ClientUpdatePacket& p, float deltaTime, float lookSpeed, float camSpeed);
    static void GetFrustumPlanes(glm::vec4* planes, glm::mat4 matrix);
};