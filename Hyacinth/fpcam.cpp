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

glm::mat4 createViewMatrix(Transform& t) {
    glm::vec3 forward = glm::normalize(glm::vec3(
        cos(glm::radians(t.yaw)) * cos(glm::radians(t.pitch + t.pitchAdditional)),
        sin(glm::radians(t.pitch + t.pitchAdditional)),
        sin(glm::radians(t.yaw)) * cos(glm::radians(t.pitch + t.pitchAdditional))
    ));

    return glm::lookAt(t.position, t.position + forward, t.up);
}

void Camera::setViewMatrix(Transform& t) { // pitchadditional is for camera recoil
    m_view = createViewMatrix(t);
    m_dirtyView = false;
}

void Camera::setProjectionMatrix() {
    glm::mat4 proj = glm::perspective(glm::radians(m_FOV), m_aspectRatio, m_zNear, m_zFar);
    proj[1][1] *= -1;
    m_proj = proj;
    m_dirtyProj = false;
}

void Camera::update(bool flycam) { // true is flycam, false is player cam
    setProjectionMatrix();       

    setViewMatrix(flycam ? m_flyTransform : m_transform);

    glm::mat4 planesMat = flycam ? createViewMatrix(m_transform) : m_view;
    GetFrustumPlanes(m_frustumPlanes.planes, m_proj * planesMat); // frustum planes should always come from player camera
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
        m_flyTransform.position += (glm::normalize(localDisplacement) * (camSpeed * 100.f) * deltaTime);
    }

    if (InputManager::reloadKeyDown()) {
        m_flyTransform.position = glm::vec3(0.f);
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