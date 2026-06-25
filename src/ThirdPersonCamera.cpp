#include "ThirdPersonCamera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

ThirdPersonCamera::ThirdPersonCamera(float screenWidth, float screenHeight)
    : AspectRatio(screenWidth / screenHeight)
{
    UpdateCameraPosition();
}

glm::vec3 ThirdPersonCamera::GetFullFrontVector() const
{
    float yawRad = glm::radians(Yaw);
    float pitchRad = glm::radians(Pitch);
    return glm::vec3(
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    );
}

void ThirdPersonCamera::UpdateCameraPosition()
{
    glm::vec3 eyeTarget = TargetPosition + EyeOffset + glm::vec3(0.0f, m_SneakOffset, 0.0f);

    switch (CurrentMode)
    {
    case CameraMode::FirstPerson:
        Position = eyeTarget;
        break;
    case CameraMode::ThirdPersonBack: {
        glm::vec3 front = GetFullFrontVector();
        Position = eyeTarget - front * Distance;
        break;
    }
    case CameraMode::ThirdPersonFront: {
        glm::vec3 front = GetFullFrontVector();
        Position = eyeTarget + front * Distance;
        break;
    }
    }
}

glm::mat4 ThirdPersonCamera::GetViewMatrix() const
{
    glm::vec3 eyeTarget = TargetPosition + EyeOffset + glm::vec3(0.0f, m_SneakOffset, 0.0f);

    switch (CurrentMode)
    {
    case CameraMode::FirstPerson: {
        // compute front on the fly for const correctness
        float yawRad = glm::radians(Yaw);
        float pitchRad = glm::radians(Pitch);
        glm::vec3 front(
            std::cos(pitchRad) * std::cos(yawRad),
            std::sin(pitchRad),
            std::cos(pitchRad) * std::sin(yawRad)
        );
        return glm::lookAt(Position, Position + front, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    case CameraMode::ThirdPersonBack:
    case CameraMode::ThirdPersonFront:
        return glm::lookAt(Position, eyeTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    return glm::mat4(1.0f);
}

glm::mat4 ThirdPersonCamera::GetProjectionMatrix() const
{
    return glm::perspective(glm::radians(Fov), AspectRatio, 0.1f, 1000.0f);
}

void ThirdPersonCamera::ProcessMouseMovement(float xOffset, float yOffset)
{
    Yaw   += xOffset * MouseSensitivity;
    Pitch += yOffset * MouseSensitivity;

    Pitch = std::clamp(Pitch, -89.0f, 89.0f);

    UpdateCameraPosition();
}

glm::vec3 ThirdPersonCamera::GetFrontVector() const
{
    float yawRad = glm::radians(Yaw);
    glm::vec3 front(std::cos(yawRad), 0.0f, std::sin(yawRad));
    return glm::normalize(front);
}

glm::vec3 ThirdPersonCamera::GetRightVector() const
{
    return glm::normalize(glm::cross(GetFrontVector(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

void ThirdPersonCamera::SetScreenSize(float width, float height)
{
    AspectRatio = width / height;
}

void ThirdPersonCamera::ToggleMode()
{
    switch (CurrentMode)
    {
    case CameraMode::FirstPerson:     CurrentMode = CameraMode::ThirdPersonBack;  break;
    case CameraMode::ThirdPersonBack: CurrentMode = CameraMode::ThirdPersonFront; break;
    case CameraMode::ThirdPersonFront: CurrentMode = CameraMode::FirstPerson;     break;
    }
    UpdateCameraPosition();
}