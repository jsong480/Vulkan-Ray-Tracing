#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct CameraUBO {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
};

struct Camera {
    glm::vec3 position = {0.0f, 0.0f, 2.9f};
    float yaw          = -90.0f;
    float pitch        = 0.0f;
    float fov          = 65.0f;
    float speed        = 1.5f;
    float sensitivity  = 0.15f;
    bool  moved        = false;

    glm::vec3 front() const {
        return glm::normalize(glm::vec3(
            cosf(glm::radians(yaw)) * cosf(glm::radians(pitch)),
            sinf(glm::radians(pitch)),
            sinf(glm::radians(yaw)) * cosf(glm::radians(pitch))));
    }

    void processMouse(float dx, float dy) {
        yaw   += dx * sensitivity;
        pitch += dy * sensitivity;
        pitch  = glm::clamp(pitch, -89.0f, 89.0f);
        moved  = true;
    }

    CameraUBO ubo(float aspect) const {
        CameraUBO u{};
        auto view = glm::lookAt(position, position + front(), glm::vec3(0, 1, 0));
        auto proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
        proj[1][1] *= -1.0f;
        u.viewInverse = glm::inverse(view);
        u.projInverse = glm::inverse(proj);
        return u;
    }
};
