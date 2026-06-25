#include "FPSCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

FPSCamera::FPSCamera(float screenWidth, float screenHeight, glm::vec3 startPosition)
    : Position(startPosition)
    , WorldUp(glm::vec3(0.0f, 1.0f, 0.0f))
    , Yaw(-90.0f)
    , Pitch(0.0f)
    , MovementSpeed(5.0f)
    , MouseSensitivity(0.1f)
    , Fov(45.0f)
    , AspectRatio(screenWidth / screenHeight)
{
    updateCameraVectors();
}

glm::mat4 FPSCamera::GetViewMatrix() const
{
    return glm::lookAt(Position, Position + Front, Up);
}

void FPSCamera::ProcessMouseMovement(float xoffset, float yoffset)
{
    Yaw   += xoffset * MouseSensitivity;
    Pitch += yoffset * MouseSensitivity;
    Pitch  = std::clamp(Pitch, -89.0f, 89.0f);

    updateCameraVectors();
}

void FPSCamera::ProcessKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = MovementSpeed * deltaTime;

    switch (direction)
    {
    case CameraMovement::FORWARD:
        Position += Front * velocity;
        break;
    case CameraMovement::BACKWARD:
        Position -= Front * velocity;
        break;
    case CameraMovement::LEFT:
        Position -= Right * velocity;
        break;
    case CameraMovement::RIGHT:
        Position += Right * velocity;
        break;
    }
}

void FPSCamera::updateCameraVectors()
{
    float yawRad   = glm::radians(Yaw);
    float pitchRad = glm::radians(Pitch);

    glm::vec3 newFront;
    newFront.x = std::cos(pitchRad) * std::cos(yawRad);
    newFront.y = std::sin(pitchRad);
    newFront.z = std::cos(pitchRad) * std::sin(yawRad);
    Front = glm::normalize(newFront);

    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}

glm::mat4 FPSCamera::GetProjectionMatrix() const
{
    return glm::perspective(glm::radians(Fov), AspectRatio, 0.1f, 1000.0f);
}

void FPSCamera::SetScreenSize(float width, float height)
{
    AspectRatio = width / height;
}
