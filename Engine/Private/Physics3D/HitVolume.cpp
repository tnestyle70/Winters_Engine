#include "Physics3D/HitVolume.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    struct AxisXZ
    {
        f32_t x = 0.f;
        f32_t z = 0.f;
    };

    struct YawBox
    {
        Vec3 center{};
        Vec3 halfExtents{};
        AxisXZ axisX{};
        AxisXZ axisZ{};
    };

    struct Bounds3D
    {
        Vec3 min{};
        Vec3 max{};
    };

    f32_t AbsF(f32_t value)
    {
        return static_cast<f32_t>(std::fabs(value));
    }

    bool IsFinite(f32_t value)
    {
        return std::isfinite(value);
    }

    f32_t SanitizeScalar(f32_t value)
    {
        return IsFinite(value) ? value : 0.f;
    }

    f32_t ClampExtent(f32_t value)
    {
        if (!IsFinite(value) || value <= 0.f)
            return 0.f;

        return value;
    }

    eHitShape SanitizeShape(eHitShape shape)
    {
        switch (shape)
        {
        case eHitShape::AABB:
        case eHitShape::OBB:
        case eHitShape::Sphere:
            return shape;
        default:
            return eHitShape::AABB;
        }
    }

    Vec3 SanitizeCenter(const Vec3& center)
    {
        return Vec3{
            SanitizeScalar(center.x),
            SanitizeScalar(center.y),
            SanitizeScalar(center.z),
        };
    }

    Vec3 ClampExtents(const Vec3& halfExtents)
    {
        return Vec3{
            ClampExtent(halfExtents.x),
            ClampExtent(halfExtents.y),
            ClampExtent(halfExtents.z),
        };
    }

    HitVolume SanitizeVolume(const HitVolume& volume)
    {
        HitVolume clean{};
        clean.shape = SanitizeShape(volume.shape);
        clean.center = SanitizeCenter(volume.center);
        clean.halfExtents = ClampExtents(volume.halfExtents);
        clean.rotationYaw = WintersMath::NormalizeRadians(volume.rotationYaw);
        return clean;
    }

    f32_t DotXZ(const AxisXZ& lhs, const AxisXZ& rhs)
    {
        return lhs.x * rhs.x + lhs.z * rhs.z;
    }

    f32_t DotDeltaXZ(const Vec3& delta, const AxisXZ& axis)
    {
        return delta.x * axis.x + delta.z * axis.z;
    }

    Bounds3D MakeBounds(const Vec3& rawMin, const Vec3& rawMax)
    {
        const Vec3 a = SanitizeCenter(rawMin);
        const Vec3 b = SanitizeCenter(rawMax);

        Bounds3D bounds{};
        bounds.min = Vec3{
            std::min(a.x, b.x),
            std::min(a.y, b.y),
            std::min(a.z, b.z),
        };
        bounds.max = Vec3{
            std::max(a.x, b.x),
            std::max(a.y, b.y),
            std::max(a.z, b.z),
        };
        return bounds;
    }

    Vec3 AABBMin(const HitVolume& volume)
    {
        const HitVolume clean = SanitizeVolume(volume);
        const Vec3 halfExtents = clean.halfExtents;
        return Vec3{
            clean.center.x - halfExtents.x,
            clean.center.y - halfExtents.y,
            clean.center.z - halfExtents.z,
        };
    }

    Vec3 AABBMax(const HitVolume& volume)
    {
        const HitVolume clean = SanitizeVolume(volume);
        const Vec3 halfExtents = clean.halfExtents;
        return Vec3{
            clean.center.x + halfExtents.x,
            clean.center.y + halfExtents.y,
            clean.center.z + halfExtents.z,
        };
    }

    f32_t SphereRadius(const HitVolume& volume)
    {
        return ClampExtent(volume.halfExtents.x);
    }

    YawBox MakeYawBox(const HitVolume& volume)
    {
        const HitVolume clean = SanitizeVolume(volume);
        const f32_t yaw = (clean.shape == eHitShape::AABB) ? 0.f : clean.rotationYaw;
        const f32_t sinYaw = static_cast<f32_t>(std::sin(yaw));
        const f32_t cosYaw = static_cast<f32_t>(std::cos(yaw));

        YawBox box{};
        box.center = clean.center;
        box.halfExtents = clean.halfExtents;
        box.axisX = AxisXZ{ cosYaw, -sinYaw };
        box.axisZ = AxisXZ{ sinYaw, cosYaw };
        return box;
    }

    f32_t ProjectRadiusXZ(const YawBox& box, const AxisXZ& axis)
    {
        const f32_t x = AbsF(DotXZ(axis, box.axisX)) * box.halfExtents.x;
        const f32_t z = AbsF(DotXZ(axis, box.axisZ)) * box.halfExtents.z;
        return x + z;
    }

    bool OverlapOnAxisXZ(const YawBox& a, const YawBox& b, const AxisXZ& axis)
    {
        const Vec3 delta = b.center - a.center;
        const f32_t distance = AbsF(DotDeltaXZ(delta, axis));
        const f32_t radius = ProjectRadiusXZ(a, axis) + ProjectRadiusXZ(b, axis);
        return distance <= radius;
    }

    bool OverlapYawBox(const YawBox& a, const YawBox& b)
    {
        const f32_t yDistance = AbsF(b.center.y - a.center.y);
        if (yDistance > a.halfExtents.y + b.halfExtents.y)
            return false;

        if (!OverlapOnAxisXZ(a, b, a.axisX))
            return false;
        if (!OverlapOnAxisXZ(a, b, a.axisZ))
            return false;
        if (!OverlapOnAxisXZ(a, b, b.axisX))
            return false;
        if (!OverlapOnAxisXZ(a, b, b.axisZ))
            return false;

        return true;
    }

    bool OverlapSphereBox(const Vec3& sphereCenter, f32_t sphereRadius, const YawBox& box)
    {
        const Vec3 cleanSphereCenter = SanitizeCenter(sphereCenter);
        const f32_t cleanSphereRadius = ClampExtent(sphereRadius);
        const Vec3 delta = cleanSphereCenter - box.center;
        const f32_t localX = DotDeltaXZ(delta, box.axisX);
        const f32_t localY = delta.y;
        const f32_t localZ = DotDeltaXZ(delta, box.axisZ);

        const f32_t dx = std::max(AbsF(localX) - box.halfExtents.x, 0.f);
        const f32_t dy = std::max(AbsF(localY) - box.halfExtents.y, 0.f);
        const f32_t dz = std::max(AbsF(localZ) - box.halfExtents.z, 0.f);
        const f32_t distanceSq = dx * dx + dy * dy + dz * dz;
        return distanceSq <= cleanSphereRadius * cleanSphereRadius;
    }
}

namespace WintersPhysics3D
{
    const char* HitShapeName(eHitShape shape)
    {
        switch (shape)
        {
        case eHitShape::AABB:
            return "AABB";
        case eHitShape::OBB:
            return "OBB";
        case eHitShape::Sphere:
            return "Sphere";
        default:
            return "Unknown";
        }
    }

    HitVolume MakeAABB(const Vec3& center, const Vec3& halfExtents)
    {
        HitVolume volume{};
        volume.shape = eHitShape::AABB;
        volume.center = center;
        volume.halfExtents = halfExtents;
        volume.rotationYaw = 0.f;
        return SanitizeVolume(volume);
    }

    HitVolume MakeSphere(const Vec3& center, f32_t radius)
    {
        const f32_t cleanRadius = ClampExtent(radius);

        HitVolume volume{};
        volume.shape = eHitShape::Sphere;
        volume.center = center;
        volume.halfExtents = Vec3{ cleanRadius, cleanRadius, cleanRadius };
        volume.rotationYaw = 0.f;
        return SanitizeVolume(volume);
    }

    HitVolume MakeOBB(const Vec3& center, const Vec3& halfExtents, f32_t yawRadians)
    {
        HitVolume volume{};
        volume.shape = eHitShape::OBB;
        volume.center = center;
        volume.halfExtents = halfExtents;
        volume.rotationYaw = yawRadians;
        return SanitizeVolume(volume);
    }

    HitVolume Sanitize(const HitVolume& volume)
    {
        return SanitizeVolume(volume);
    }

    HitActiveWindow MakeActiveWindow(
        u16_t activeFrameStart,
        u16_t activeFrameEnd,
        u16_t telegraphFrameStart)
    {
        HitActiveWindow window{};
        window.activeFrameStart = activeFrameStart;
        window.activeFrameEnd = std::max(activeFrameStart, activeFrameEnd);
        window.telegraphFrameStart = std::min(telegraphFrameStart, window.activeFrameStart);
        return window;
    }

    bool_t IsFrameActive(const HitActiveWindow& window, u16_t frame)
    {
        const HitActiveWindow clean = MakeActiveWindow(
            window.activeFrameStart,
            window.activeFrameEnd,
            window.telegraphFrameStart);
        return frame >= clean.activeFrameStart && frame <= clean.activeFrameEnd;
    }

    u16_t ActiveWindowLengthFrames(const HitActiveWindow& window)
    {
        const HitActiveWindow clean = MakeActiveWindow(
            window.activeFrameStart,
            window.activeFrameEnd,
            window.telegraphFrameStart);
        const u32_t length = static_cast<u32_t>(clean.activeFrameEnd)
            - static_cast<u32_t>(clean.activeFrameStart) + 1u;
        return static_cast<u16_t>(std::min<u32_t>(length, 0xFFFFu));
    }

    u16_t DodgeWindowLengthFrames(const HitActiveWindow& window)
    {
        const HitActiveWindow clean = MakeActiveWindow(
            window.activeFrameStart,
            window.activeFrameEnd,
            window.telegraphFrameStart);
        const u32_t length = static_cast<u32_t>(clean.activeFrameStart)
            - static_cast<u32_t>(clean.telegraphFrameStart);
        return static_cast<u16_t>(std::min<u32_t>(length, 0xFFFFu));
    }

    std::string ToDebugString(const HitVolume& volume)
    {
        const HitVolume clean = SanitizeVolume(volume);
        char buffer[256]{};
        std::snprintf(buffer, sizeof(buffer),
            "HitVolume{shape=%s center=(%.3f, %.3f, %.3f) halfExtents=(%.3f, %.3f, %.3f) yaw=%.3f radius=%.3f}",
            HitShapeName(clean.shape),
            clean.center.x,
            clean.center.y,
            clean.center.z,
            clean.halfExtents.x,
            clean.halfExtents.y,
            clean.halfExtents.z,
            clean.rotationYaw,
            SphereRadius(clean));
        return std::string(buffer);
    }

    std::string ToDebugString(const HitActiveWindow& window)
    {
        const HitActiveWindow clean = MakeActiveWindow(
            window.activeFrameStart,
            window.activeFrameEnd,
            window.telegraphFrameStart);
        char buffer[128]{};
        std::snprintf(buffer, sizeof(buffer),
            "HitActiveWindow{telegraph=%u activeStart=%u activeEnd=%u activeLength=%u dodgeLength=%u}",
            static_cast<unsigned>(clean.telegraphFrameStart),
            static_cast<unsigned>(clean.activeFrameStart),
            static_cast<unsigned>(clean.activeFrameEnd),
            static_cast<unsigned>(ActiveWindowLengthFrames(clean)),
            static_cast<unsigned>(DodgeWindowLengthFrames(clean)));
        return std::string(buffer);
    }

    bool OverlapAABB(const Vec3& aMin, const Vec3& aMax,
        const Vec3& bMin, const Vec3& bMax)
    {
        const Bounds3D a = MakeBounds(aMin, aMax);
        const Bounds3D b = MakeBounds(bMin, bMax);
        return a.min.x <= b.max.x && a.max.x >= b.min.x
            && a.min.y <= b.max.y && a.max.y >= b.min.y
            && a.min.z <= b.max.z && a.max.z >= b.min.z;
    }

    bool OverlapSphere(const Vec3& ca, f32_t ra, const Vec3& cb, f32_t rb)
    {
        const Vec3 cleanA = SanitizeCenter(ca);
        const Vec3 cleanB = SanitizeCenter(cb);
        const f32_t radius = ClampExtent(ra) + ClampExtent(rb);
        const f32_t dx = cleanB.x - cleanA.x;
        const f32_t dy = cleanB.y - cleanA.y;
        const f32_t dz = cleanB.z - cleanA.z;
        const f32_t distanceSq = dx * dx + dy * dy + dz * dz;
        return distanceSq <= radius * radius;
    }

    bool OverlapOBB(const HitVolume& a, const HitVolume& b)
    {
        return OverlapYawBox(MakeYawBox(a), MakeYawBox(b));
    }

    bool Overlap(const HitVolume& a, const HitVolume& b)
    {
        const HitVolume cleanA = SanitizeVolume(a);
        const HitVolume cleanB = SanitizeVolume(b);

        if (cleanA.shape == eHitShape::Sphere && cleanB.shape == eHitShape::Sphere)
            return OverlapSphere(cleanA.center, SphereRadius(cleanA), cleanB.center, SphereRadius(cleanB));

        if (cleanA.shape == eHitShape::Sphere)
            return OverlapSphereBox(cleanA.center, SphereRadius(cleanA), MakeYawBox(cleanB));

        if (cleanB.shape == eHitShape::Sphere)
            return OverlapSphereBox(cleanB.center, SphereRadius(cleanB), MakeYawBox(cleanA));

        if (cleanA.shape == eHitShape::AABB && cleanB.shape == eHitShape::AABB)
            return OverlapAABB(AABBMin(cleanA), AABBMax(cleanA), AABBMin(cleanB), AABBMax(cleanB));

        return OverlapOBB(cleanA, cleanB);
    }
}
