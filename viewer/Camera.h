#ifndef VIEWER_CAMERA_H
#define VIEWER_CAMERA_H

#include "../shared.h"

class ViewerCamera
{
public:
    glm::vec3 target{0.0f};
    float distance = 4.0f;
    float yaw = 0.0f;
    float pitch = 0.25f;
    float fov = 45.0f;
    bool perspective = true;

    void orbit(float deltaX, float deltaY);
    void zoom(float amount);
    glm::vec3 position() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;
    void reset();
};

#endif