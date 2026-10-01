#include "Camera.h"

void ViewerCamera::orbit(float deltaX, float deltaY)
{
    yaw += deltaX * 0.01f;
    pitch = glm::clamp(pitch + deltaY * 0.01f, -1.5f, 1.5f);
}

void ViewerCamera::zoom(float amount)
{
    distance = glm::clamp(distance - amount * 0.35f, 0.5f, 100.0f);
}

glm::vec3 ViewerCamera::position() const
{
    const float horizontal = std::cos(pitch) * distance;
    return target + glm::vec3(std::sin(yaw) * horizontal, std::sin(pitch) * distance,
                              std::cos(yaw) * horizontal);
}

glm::mat4 ViewerCamera::viewMatrix() const
{
    return glm::lookAt(position(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ViewerCamera::projectionMatrix(float aspect) const
{
    if (perspective)
        return glm::perspective(glm::radians(fov), aspect, 0.01f, 100.0f);

    const float halfHeight = distance * 0.65f;
    return glm::ortho(-halfHeight * aspect, halfHeight * aspect, -halfHeight, halfHeight, 0.01f, 100.0f);
}

void ViewerCamera::reset()
{
    target = glm::vec3(0.0f);
    distance = 4.0f;
    yaw = 0.0f;
    pitch = 0.25f;
    fov = 45.0f;
    perspective = true;
}