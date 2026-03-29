#include "fpcam.h"

enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };

void Camera::GetFrustumPlanes(glm::vec4* planes, glm::mat4 matrix) {
	glm::mat4 transposed = glm::transpose(matrix);

    planes[LEFT]    = glm::normalize(transposed[BOTTOM] + transposed[LEFT]);
	planes[RIGHT]   = glm::normalize(transposed[BOTTOM] - transposed[LEFT]);
    planes[TOP]     = glm::normalize(transposed[BOTTOM] - transposed[RIGHT]);
    planes[BOTTOM]  = glm::normalize(transposed[BOTTOM] + transposed[RIGHT]);
    planes[BACK]    = glm::normalize(transposed[BOTTOM] + transposed[TOP]);
    planes[FRONT]   = glm::normalize(transposed[BOTTOM] - transposed[TOP]);
}

void Camera::setViewMatrix() {
    m_view = glm::lookAt(m_transform.position, m_transform.position + m_transform.forward, m_transform.up);
    m_dirtyView = false;
}

void Camera::setProjectionMatrix() {
    glm::mat4 proj = glm::perspective(glm::radians(m_FOV), m_aspectRatio, m_zNear, m_zFar);
    proj[1][1] *= -1;
    m_proj = proj;
    m_dirtyProj = false;
}

void Camera::update(float deltaTime, bool moveMouse) {
    // if (!moveMouse) {
    //     return;
    // }
    // 
    // auto [dx, dy] = InputManager::getMouseMotion();
    // 
    // if (glm::abs(dx) > 0.f || glm::abs(dy) > 0.f) {
    //     float mouseX = dx * m_lookSpeed * deltaTime;
    //     float mouseY = dy * m_lookSpeed * deltaTime;
    // 
    //     m_transform.yaw += mouseX;
    //     m_transform.pitch -= mouseY;
    // 
    //     if (m_transform.yaw > 360.f)  m_transform.yaw -= 360.f;
    //     if (m_transform.yaw < -360.f) m_transform.yaw += 360.f;
    //     m_transform.pitch = glm::clamp(m_transform.pitch, -89.9f, 89.9f);
    // 
    //     glm::quat qYaw = glm::angleAxis(glm::radians(m_transform.yaw), glm::vec3(0, 1, 0));
    //     glm::quat qPitch = glm::angleAxis(glm::radians(m_transform.pitch), glm::vec3(0, 0, 1));
    // 
    //     m_transform.rotation = qYaw * qPitch;
    //     m_transform.forward = glm::normalize(glm::vec3(
    //         cos(glm::radians(m_transform.yaw)) * cos(glm::radians(m_transform.pitch)),
    //         sin(glm::radians(m_transform.pitch)),
    //         sin(glm::radians(m_transform.yaw)) * cos(glm::radians(m_transform.pitch))
    //     ));
    //     m_transform.right = glm::normalize(glm::cross(m_transform.forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    //     m_transform.up = glm::normalize(glm::cross(m_transform.right, m_transform.forward));
    // 
    //     m_dirtyView = true;
    // }
    // 
    // glm::vec3 localDisplacement{ 0.0f, 0.0f, 0.0f };
    // if (InputManager::forwardKeyDown())    localDisplacement += m_transform.forward;
    // if (InputManager::backwardKeyDown())   localDisplacement -= m_transform.forward;
    // if (InputManager::rightKeyDown())      localDisplacement += m_transform.right;
    // if (InputManager::leftKeyDown())       localDisplacement -= m_transform.right;
    // if (InputManager::upKeyDown())         localDisplacement += m_transform.up;
    // if (InputManager::downKeyDown())       localDisplacement -= m_transform.up;
    // 
    // if (glm::length(localDisplacement) > 0) {
    //     m_transform.position += glm::normalize(localDisplacement) * m_moveSpeed * deltaTime;
    //     m_dirtyView = true;
    //     m_dirtyMovement = true;
    // }

    setProjectionMatrix();

    setViewMatrix();
    GetFrustumPlanes(m_frustumPlanes.planes, m_proj * m_view);
}

Camera::Camera(float aspect, float fov, float nearC, float farC) {
    m_moveSpeed = BASE_MOVE_SPEED;
    m_lookSpeed = BASE_LOOK_SPEED;
    m_aspectRatio = aspect;
    m_FOV = fov;
    m_zNear = nearC;
    m_zFar = farC;
    m_dirtyProj = true;
    m_dirtyView = true;

    update(1.f, true);
}