#pragma once

#include <cstdint>

#include "glm/glm.hpp"

namespace UnityExternal {

struct ScreenRect {
    float x;
    float y;
    float width;
    float height;
};

struct WorldToScreenResult {
    bool  visible;   // true if point is in front of camera and projection succeeded
    float x;         // screen X in pixels
    float y;         // screen Y in pixels
    float depth;     // depth along camera forward (>0 means in front)
};

// Simplified: Convert world position to screen position using only the view-projection matrix.
// viewProj  : projection * view matrix (from Camera+0x100)
// screen    : screen/viewport rectangle (usually 0,0,width,height)
// worldPos  : target world position
inline WorldToScreenResult WorldToScreenPoint(const glm::mat4& viewProj,
                                              const ScreenRect& screen,
                                              const glm::vec3& worldPos)
{
    WorldToScreenResult out{};
    out.visible = false;
    out.x = 0.0f;
    out.y = 0.0f;
    out.depth = 0.0f;

    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f) {
        return out;
    }

    glm::vec3 ndc = glm::vec3(clip) / clip.w; // -1..1

    // NDC -> screen pixels (Y flipped for screen coordinates)
    float sx = (ndc.x + 1.0f) * 0.5f * screen.width  + screen.x;
    float sy = (1.0f - ndc.y) * 0.5f * screen.height + screen.y;

    out.x = sx;
    out.y = sy;
    out.depth = clip.w;
    out.visible = (ndc.x >= -1.0f && ndc.x <= 1.0f && ndc.y >= -1.0f && ndc.y <= 1.0f);
    return out;
}

// Full version: Convert world position to screen position with explicit camera position/forward.
inline WorldToScreenResult WorldToScreenPointFull(const glm::mat4& viewProj,
                                                  const glm::vec3& camPos,
                                                  const glm::vec3& camForward,
                                                  const ScreenRect& screen,
                                                  const glm::vec3& worldPos)
{
    WorldToScreenResult out{};
    out.visible = false;
    out.x = 0.0f;
    out.y = 0.0f;
    out.depth = 0.0f;

    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    if (clip.w == 0.0f) {
        return out;
    }

    glm::vec3 ndc = glm::vec3(clip) / clip.w; // -1..1

    // NDC -> screen pixels
    float sx = (ndc.x + 1.0f) * 0.5f * screen.width  + screen.x;
    float sy = (ndc.y + 1.0f) * 0.5f * screen.height + screen.y;

    // depth along camera forward
    glm::vec3 delta = worldPos - camPos;
    float depth = glm::dot(delta, camForward);

    out.x = sx;
    out.y = sy;
    out.depth = depth;
    out.visible = (depth > 0.0f);
    return out;
}

} // namespace UnityExternal
