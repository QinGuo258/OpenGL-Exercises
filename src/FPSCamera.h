#pragma once

#include <glm/glm.hpp>

enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class FPSCamera
{
public:
    FPSCamera(float screenWidth, float screenHeight,
              glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;

    void ProcessMouseMovement(float xoffset, float yoffset);
    void ProcessKeyboard(CameraMovement direction, float deltaTime);
    void SetScreenSize(float width, float height);

    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;

private:
    void updateCameraVectors();

    float Fov;
    float AspectRatio;
};
