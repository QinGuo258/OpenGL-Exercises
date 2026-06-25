#pragma once

#include <glm/glm.hpp>

enum class CameraMode { FirstPerson, ThirdPersonBack, ThirdPersonFront };

class ThirdPersonCamera
{
public:
    ThirdPersonCamera(float screenWidth, float screenHeight);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::vec3 GetFrontVector() const;
    glm::vec3 GetRightVector() const;

    void ProcessMouseMovement(float xOffset, float yOffset);
    void SetScreenSize(float width, float height);
    void ToggleMode();
    void SetSneakOffset(float offset) { m_SneakOffset = offset; }

    glm::vec3 TargetPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    float Distance = 5.0f;
    float Pitch = 0.0f;
    float Yaw = -90.0f;
    float MouseSensitivity = 0.1f;
    glm::vec3 EyeOffset = glm::vec3(0.0f, 1.75f, 0.0f);
    CameraMode CurrentMode = CameraMode::FirstPerson;

    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetFullFrontVector() const;
    void UpdateCameraPosition();
    void SetFOV(float fov) { Fov = fov; }

private:

    glm::vec3 Position;
    float Fov = 45.0f;
    float AspectRatio;
    float m_SneakOffset = 0.0f;
};
