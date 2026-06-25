#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

Camera::Camera(float screenWidth, float screenHeight)
    : AspectRatio(screenWidth / screenHeight) {}

glm::vec3 Camera::GetPosition() const
{
    float pitchRad = glm::radians(Pitch);
    float yawRad = glm::radians(Yaw);

    float x = std::cos(pitchRad) * std::cos(yawRad);
    float y = std::sin(pitchRad);
    float z = std::cos(pitchRad) * std::sin(yawRad);

    return Target + Distance * glm::vec3(x, y, z);
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(GetPosition(), Target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix() const
{
    return glm::perspective(glm::radians(Fov), AspectRatio, 0.1f, 100.0f);
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset)
{
    Yaw += xoffset * SENSITIVITY;
    Pitch -= yoffset * SENSITIVITY;
    Pitch = std::clamp(Pitch, MIN_PITCH, MAX_PITCH);
}

void Camera::ProcessMouseScroll(float yoffset)
{
    Distance -= yoffset * SCROLL_SENSITIVITY;
    Distance = std::clamp(Distance, MIN_DISTANCE, MAX_DISTANCE);
}

void Camera::SetScreenSize(float width, float height)
{
    AspectRatio = width / height;
}
