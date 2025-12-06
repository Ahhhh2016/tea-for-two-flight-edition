#include "utils/Camera.h"

Camera::Camera()
	: m_position(0.f, 0.f, 5.f)
	, m_look(0.f, 0.f, -1.f)
	, m_up(0.f, 1.f, 0.f)
	, m_fovYRadians(glm::radians(45.f))
	, m_aspectRatio(1.f)
	, m_nearPlane(0.1f)
	, m_farPlane(100.f)
{
}

void Camera::setFromSceneCameraData(const SceneCameraData &data) {
	m_position = glm::vec3(data.pos);
	m_look     = glm::normalize(glm::vec3(data.look));
	m_up       = glm::normalize(glm::vec3(data.up));
	m_fovYRadians = data.heightAngle;
}

void Camera::setPosition(const glm::vec3 &position){
    m_position = position;
}

void Camera::setLook(const glm::vec3 &lookDirection) {
    m_look = glm::normalize(lookDirection);
}

void Camera::setUp(const glm::vec3 &upDirection) {
    m_up = glm::normalize(upDirection);
}

void Camera::setFovYRadians(float fovYRadians) {
    m_fovYRadians = fovYRadians;
}

void Camera::setAspectRatio(float aspect) {
    m_aspectRatio = aspect;
}

void Camera::setClipPlanes(float nearPlane, float farPlane) {
	m_nearPlane = nearPlane;
	m_farPlane = farPlane;
}

const glm::vec3 &Camera::getPosition() const {
    return m_position;
}

const glm::vec3 &Camera::getLook() const {
    return m_look;
}

const glm::vec3 &Camera::getUp() const {
    return m_up;
}

float Camera::getFovYRadians() const {
    return m_fovYRadians;
}

float Camera::getAspectRatio() const {
    return m_aspectRatio;
}

float Camera::getNearPlane() const {
    return m_nearPlane;
}

float Camera::getFarPlane() const {
    return m_farPlane;
}

glm::mat4 Camera::getViewMatrix() const {
	const glm::vec3 eye = m_position;
    const glm::vec3 f = glm::normalize(m_look); // forward (camera looks towards +f), but view uses -f on the Z axis
	const glm::vec3 upN = glm::normalize(m_up);
	const glm::vec3 s = glm::normalize(glm::cross(f, upN)); // right
	const glm::vec3 u = glm::cross(s, f);                   // true up

	glm::mat4 view(1.0f);
	view[0][0] = s.x; view[1][0] = s.y; view[2][0] = s.z; view[3][0] = -glm::dot(s, eye);
	view[0][1] = u.x; view[1][1] = u.y; view[2][1] = u.z; view[3][1] = -glm::dot(u, eye);
	view[0][2] = -f.x; view[1][2] = -f.y; view[2][2] = -f.z; view[3][2] =  glm::dot(f, eye);
    view[0][3] = 0.0f; view[1][3] = 0.0f; view[2][3] = 0.0f; view[3][3] =  1.0f;
	return view;
}

glm::mat4 Camera::getProjectionMatrix() const {
	const float aspect = (m_aspectRatio > 0.f) ? m_aspectRatio : 1.f;
	const float tanHalfFovy = tanf(m_fovYRadians * 0.5f);
	const float zNear = m_nearPlane;
	const float zFar  = m_farPlane;

    // perspective matrix
	glm::mat4 P(0.0f); 
	P[0][0] = 1.0f / (aspect * tanHalfFovy);
	P[1][1] = 1.0f / (tanHalfFovy);
	P[2][2] = -(zFar + zNear) / (zFar - zNear);
	P[2][3] = -1.0f;
	P[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
	P[3][3] = 0.0f;
	return P;
}


