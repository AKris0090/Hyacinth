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

void Camera::setViewMatrix(Transform& t) { // pitchadditional is for camera recoil
    glm::vec3 forwad = glm::normalize(glm::vec3(
        cos(glm::radians(t.yaw)) * cos(glm::radians(t.pitch + t.pitchAdditional)),
        sin(glm::radians(t.pitch + t.pitchAdditional)),
        sin(glm::radians(t.yaw)) * cos(glm::radians(t.pitch + t.pitchAdditional))
    ));

    m_view = glm::lookAt(t.position, t.position + forwad, t.up);
    m_dirtyView = false;
}

void Camera::setProjectionMatrix() {
    glm::mat4 proj = glm::perspective(glm::radians(m_FOV), m_aspectRatio, m_zNear, m_zFar);
    proj[1][1] *= -1;
    m_proj = proj;
    m_dirtyProj = false;
}

void Camera::update(bool whichType) { // true is flycam, false is player cam
    setProjectionMatrix();

    setViewMatrix(whichType ? m_flyTransform : m_transform);
    GetFrustumPlanes(m_frustumPlanes.planes, m_proj * m_view);
}

void Camera::updateFlyCamera(const ClientUpdatePacket& p, float deltaTime, float lookSpeed, float camSpeed) {
    // update look direction
    float mouseX = p.pitch * lookSpeed * deltaTime;
    float mouseY = p.yaw * lookSpeed * deltaTime;

    m_flyTransform.yaw += mouseX;
    m_flyTransform.pitch -= mouseY;

    if (m_flyTransform.yaw > 360.f)  m_flyTransform.yaw -= 360.f;
    if (m_flyTransform.yaw < -360.f) m_flyTransform.yaw += 360.f;

    m_flyTransform.pitch = glm::clamp(m_flyTransform.pitch, -89.9f, 89.9f);
    m_flyTransform.setRotationPitchYaw();

    // update motion
    glm::vec3 localDisplacement{ 0.0f, 0.0f, 0.0f };

    localDisplacement += static_cast<float>(p.movementFB) * m_flyTransform.forward;
    localDisplacement += static_cast<float>(p.movementLR) * m_flyTransform.right;
    localDisplacement += static_cast<float>(p.movementUD) * m_flyTransform.up;

    if (glm::length(localDisplacement) > 0) {
        m_flyTransform.position += (glm::normalize(localDisplacement) * camSpeed);
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

    update(false);
}