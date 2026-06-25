#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
    Camera(float screenWidth, float screenHeight);

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;

    void ProcessMouseMovement(float xoffset, float yoffset);
    void ProcessMouseScroll(float yoffset);
    void SetScreenSize(float width, float height);

    glm::vec3 Target = glm::vec3(0.0f, 0.0f, 0.0f);

    glm::vec3 GetPosition() const;

private:
    float Distance = 5.0f;
    float Yaw = -90.0f;
    float Pitch = -30.0f;
    float Fov = 45.0f;
    float AspectRatio;

    static constexpr float MIN_DISTANCE = 1.0f;
    static constexpr float MAX_DISTANCE = 50.0f;
    static constexpr float SENSITIVITY = 0.15f;
    static constexpr float SCROLL_SENSITIVITY = 1.5f;
    static constexpr float MIN_PITCH = -89.0f;
    static constexpr float MAX_PITCH = 89.0f;
};
