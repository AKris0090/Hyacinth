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
    m_view = glm::lookAt(m_transform.position, m_transform.position + m_forward, m_up);
    m_dirtyView = false;
}

void Camera::setProjectionMatrix() {
    glm::mat4 proj = glm::perspective(glm::radians(m_FOV), m_aspectRatio, m_zNear, m_zFar);
    proj[1][1] *= -1;
    m_proj = proj;
    m_dirtyProj = false;
}

void Camera::update(float deltaTime, bool moveMouse, int imageIndex) {
    if (!moveMouse) {
        return;
    }

    auto [dx, dy] = InputManager::getMouseMotion();

    if (glm::abs(dx) > 0.f || glm::abs(dy) > 0.f) {
        float mouseX = dx * m_lookSpeed * deltaTime;
        float mouseY = dy * m_lookSpeed * deltaTime;

        m_yaw += mouseX;
        m_pitch -= mouseY;

        if (m_yaw > 360.f)  m_yaw -= 360.f;
        if (m_yaw < -360.f) m_yaw += 360.f;
        m_pitch = glm::clamp(m_pitch, -89.9f, 89.9f);

        glm::quat qYaw = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0, 1, 0));
        glm::quat qPitch = glm::angleAxis(glm::radians(m_pitch), glm::vec3(1, 0, 0));

        m_transform.rotation = qYaw * qPitch;
        m_forward = glm::normalize(glm::vec3(
            cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch)),
            sin(glm::radians(m_pitch)),
            sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch))
        ));
        m_right = glm::normalize(glm::cross(m_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_up = glm::normalize(glm::cross(m_right, m_forward));

        m_dirtyView = true;
    }

    glm::vec3 localDisplacement{ 0.0f, 0.0f, 0.0f };
    if (InputManager::forwardKeyDown())    localDisplacement += m_forward;
    if (InputManager::backwardKeyDown())   localDisplacement -= m_forward;
    if (InputManager::rightKeyDown())      localDisplacement += m_right;
    if (InputManager::leftKeyDown())       localDisplacement -= m_right;
    if (InputManager::upKeyDown())         localDisplacement += m_up;
    if (InputManager::downKeyDown())       localDisplacement -= m_up;

    if (glm::length(localDisplacement) > 0) {
        m_transform.position += glm::normalize(localDisplacement) * m_moveSpeed * deltaTime;
        m_dirtyView = true;
        m_dirtyMovement = true;
    }

    if (m_dirtyProj) {
        setProjectionMatrix();
	}

    if (m_dirtyView) {
        setViewMatrix();
        GetFrustumPlanes(m_frustumPlanes.planes, m_proj * m_view);
    }
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

    update(1.f, true, 0);
}