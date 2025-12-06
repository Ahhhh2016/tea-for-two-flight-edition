#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "utils/scenedata.h"

class Camera {
public:
	Camera();

	void setFromSceneCameraData(const SceneCameraData &data);

	void setPosition(const glm::vec3 &position);
	void setLook(const glm::vec3 &lookDirection);
	void setUp(const glm::vec3 &upDirection);
	void setFovYRadians(float fovYRadians);
	void setAspectRatio(float aspect);
	void setClipPlanes(float nearPlane, float farPlane);

	const glm::vec3 &getPosition() const;
	const glm::vec3 &getLook() const;
	const glm::vec3 &getUp() const;
	float getFovYRadians() const;
	float getAspectRatio() const;
	float getNearPlane() const;
	float getFarPlane() const;

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;

private:
	glm::vec3 m_position;
    glm::vec3 m_look;
	glm::vec3 m_up;
	float m_fovYRadians;
	float m_aspectRatio;
	float m_nearPlane;
	float m_farPlane;
};


