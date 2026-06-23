#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <string>

enum class eHitShape : u8_t
{
    AABB = 0,
    OBB = 1,
    Sphere = 2,
};

struct HitVolume
{
    eHitShape shape = eHitShape::AABB;
    Vec3 center{ 0.f, 0.f, 0.f };
    Vec3 halfExtents{ 0.5f, 0.5f, 0.5f }; // Sphere uses x as radius.
    f32_t rotationYaw = 0.f;               // Radians, yaw-only for OBB.
};

struct HitActiveWindow
{
    u16_t telegraphFrameStart = 0;
    u16_t activeFrameStart = 0;
    u16_t activeFrameEnd = 0;
};

namespace WintersPhysics3D
{
    WINTERS_ENGINE const char* HitShapeName(eHitShape shape);

    WINTERS_ENGINE HitVolume MakeAABB(const Vec3& center, const Vec3& halfExtents);
    WINTERS_ENGINE HitVolume MakeSphere(const Vec3& center, f32_t radius);
    WINTERS_ENGINE HitVolume MakeOBB(const Vec3& center, const Vec3& halfExtents, f32_t yawRadians);
    WINTERS_ENGINE HitVolume Sanitize(const HitVolume& volume);

    WINTERS_ENGINE HitActiveWindow MakeActiveWindow(
        u16_t activeFrameStart,
        u16_t activeFrameEnd,
        u16_t telegraphFrameStart = 0);
    WINTERS_ENGINE bool_t IsFrameActive(const HitActiveWindow& window, u16_t frame);
    WINTERS_ENGINE u16_t ActiveWindowLengthFrames(const HitActiveWindow& window);
    WINTERS_ENGINE u16_t DodgeWindowLengthFrames(const HitActiveWindow& window);

    WINTERS_ENGINE std::string ToDebugString(const HitVolume& volume);
    WINTERS_ENGINE std::string ToDebugString(const HitActiveWindow& window);

    WINTERS_ENGINE bool Overlap(const HitVolume& a, const HitVolume& b);
    WINTERS_ENGINE bool OverlapAABB(const Vec3& aMin, const Vec3& aMax,
        const Vec3& bMin, const Vec3& bMax);
    WINTERS_ENGINE bool OverlapSphere(const Vec3& ca, f32_t ra,
        const Vec3& cb, f32_t rb);
    WINTERS_ENGINE bool OverlapOBB(const HitVolume& a, const HitVolume& b);
}
