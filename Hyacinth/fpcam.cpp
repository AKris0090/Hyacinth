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

void Camera::update() {
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

    update();
}