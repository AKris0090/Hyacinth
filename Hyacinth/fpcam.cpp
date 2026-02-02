#include "fpcam.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };

void FPSCam::getFrustumPlanes() {
    glm::mat4 matrix = m_props.proj * m_props.view;
	glm::mat4 transposed = glm::transpose(matrix);

    m_frustumPlanes.planes[LEFT]    = glm::normalize(transposed[BOTTOM] + transposed[LEFT]);
	m_frustumPlanes.planes[RIGHT]   = glm::normalize(transposed[BOTTOM] - transposed[LEFT]);
    m_frustumPlanes.planes[TOP]     = glm::normalize(transposed[BOTTOM] - transposed[RIGHT]);
    m_frustumPlanes.planes[BOTTOM]  = glm::normalize(transposed[BOTTOM] + transposed[RIGHT]);
    m_frustumPlanes.planes[BACK]    = glm::normalize(transposed[BOTTOM] + transposed[TOP]);
    m_frustumPlanes.planes[FRONT]   = glm::normalize(transposed[BOTTOM] - transposed[TOP]);
}

glm::mat4 getViewMatrix(FPSCam::CameraProps& props, Transform& t) {
    return glm::lookAt(t.position, t.position + props.forward, props.up);
}

glm::mat4 getProjectionMatrix(FPSCam::CameraProps& props) {
    glm::mat4 proj = glm::perspectiveZO(glm::radians(props.FOV), props.aspectRatio, props.nearClip, props.farClip);
    proj[1][1] *= -1;
    return proj;
}

void FPSCam::update(float deltaTime) {
    auto [dx, dy] = Input::getMouseMotion();

    if (glm::abs(dx) > 0.f || glm::abs(dy) > 0.f) {
        float mouseX = dx * lookSpeed * deltaTime;
        float mouseY = dy * lookSpeed * deltaTime;

        m_props.yaw += mouseX;
        m_props.pitch -= mouseY;

        if (m_props.yaw > 360.f)  m_props.yaw -= 360.f;
        if (m_props.yaw < -360.f) m_props.yaw += 360.f;
        m_props.pitch = glm::clamp(m_props.pitch, -89.9f, 89.9f);

        glm::quat qYaw = glm::angleAxis(glm::radians(m_props.yaw), glm::vec3(0, 1, 0));
        glm::quat qPitch = glm::angleAxis(glm::radians(m_props.pitch), glm::vec3(1, 0, 0));

        transform.rotation = qYaw * qPitch;
        m_props.forward = glm::normalize(glm::vec3(
            cos(glm::radians(m_props.yaw)) * cos(glm::radians(m_props.pitch)),
            sin(glm::radians(m_props.pitch)),
            sin(glm::radians(m_props.yaw)) * cos(glm::radians(m_props.pitch))
        ));
        m_props.right = glm::normalize(glm::cross(m_props.forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_props.up = glm::normalize(glm::cross(m_props.right, m_props.forward));

        dirtyView = true;
    }

    glm::vec3 localDisplacement{ 0.0f, 0.0f, 0.0f };
    if (Input::forwardKeyDown())    localDisplacement += m_props.forward;
    if (Input::backwardKeyDown())   localDisplacement -= m_props.forward;
    if (Input::rightKeyDown())      localDisplacement += m_props.right;
    if (Input::leftKeyDown())       localDisplacement -= m_props.right;
    if (Input::upKeyDown())         localDisplacement += m_props.up;
    if (Input::downKeyDown())       localDisplacement -= m_props.up;

    if (glm::length(localDisplacement) > 0) {
        transform.position += glm::normalize(localDisplacement) * moveSpeed * deltaTime;
        dirtyView = true;
    }

    if (dirtyProj) {
        m_props.proj = getProjectionMatrix(m_props);
        dirtyProj = false;
    }
    if (dirtyView) {
        m_props.view = getViewMatrix(m_props, transform);
		getFrustumPlanes();

        dirtyView = false;
    }
}