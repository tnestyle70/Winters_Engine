#pragma once

#include "WintersMath.h"

#include <cmath>
#include <cstdint>

namespace MinionSoftSeparationPolicy
{
    inline Vec3 ResolveForwardSafeDirection(
        const Vec3& rawPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker)
    {
        const Vec3 forward = WintersMath::NormalizeXZ(
            preferredForward,
            Vec3{ 0.f, 0.f, 1.f });
        Vec3 corrected{ rawPush.x, 0.f, rawPush.z };
        const float projection =
            corrected.x * forward.x + corrected.z * forward.z;
        if (projection < 0.f)
        {
            corrected.x -= forward.x * projection;
            corrected.z -= forward.z * projection;
        }

        if (WintersMath::LengthSqXZ(corrected) <= 0.000001f)
        {
            const float sideSign =
                (entityTieBreaker & 1u) != 0u ? 1.f : -1.f;
            corrected = Vec3{
                -forward.z * sideSign + forward.x * 0.25f,
                0.f,
                forward.x * sideSign + forward.z * 0.25f };
        }

        return WintersMath::NormalizeXZ(corrected, forward);
    }

    inline Vec3 ResolveStaticTangentDirection(
        const Vec3& staticPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker)
    {
        const Vec3 forward = WintersMath::NormalizeXZ(
            preferredForward, Vec3{ 0.f, 0.f, 1.f });
        const Vec3 normal = WintersMath::NormalizeXZ(staticPush, forward);
        Vec3 tangent{ -normal.z, 0.f, normal.x };
        const float oppositeDot =
            (-tangent.x) * forward.x + (-tangent.z) * forward.z;
        const float tangentDot = tangent.x * forward.x + tangent.z * forward.z;
        if (oppositeDot > tangentDot + 0.0001f ||
            (std::fabs(oppositeDot - tangentDot) <= 0.0001f &&
                (entityTieBreaker & 1u) != 0u))
        {
            tangent.x = -tangent.x;
            tangent.z = -tangent.z;
        }
        return WintersMath::NormalizeXZ(
            Vec3{
                tangent.x + normal.x * 0.25f,
                0.f,
                tangent.z + normal.z * 0.25f },
            tangent);
    }

    inline Vec3 ResolveCompositeDepenetrationDirection(
        const Vec3& softMinionPush,
        const Vec3& otherPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker)
    {
        Vec3 resolved = otherPush;
        const float softLength = std::sqrt(
            softMinionPush.x * softMinionPush.x +
            softMinionPush.z * softMinionPush.z);
        if (softLength > 0.0001f)
        {
            const Vec3 safe = ResolveForwardSafeDirection(
                softMinionPush, preferredForward, entityTieBreaker);
            resolved.x += safe.x * softLength;
            resolved.z += safe.z * softLength;
        }
        return WintersMath::NormalizeXZ(resolved, preferredForward);
    }

    struct DepenetrationCandidateSelection
    {
        Vec3 vPosition{};
        bool_t bUsedStaticTangent = false;
    };

    template <typename TTryClamp>
    inline bool_t TrySelectDepenetrationCandidate(
        const Vec3& start,
        f32_t clearanceRadius,
        f32_t step,
        const Vec3& primaryDirection,
        const Vec3& staticPush,
        const Vec3& preferredForward,
        std::uint32_t entityTieBreaker,
        bool_t bHasStaticBlocker,
        const TTryClamp& tryClamp,
        DepenetrationCandidateSelection& outSelection)
    {
        const auto TryCandidate = [&](const Vec3& direction, Vec3& out)
        {
            const Vec3 desired{
                start.x + direction.x * step,
                start.y,
                start.z + direction.z * step };
            if (!tryClamp(start, desired, clearanceRadius, out))
                return false;
            return WintersMath::DistanceSqXZ(start, out) > 0.0001f;
        };

        outSelection = {};
        if (TryCandidate(primaryDirection, outSelection.vPosition))
            return true;
        if (!bHasStaticBlocker)
            return false;

        const Vec3 tangentDirection = ResolveStaticTangentDirection(
            staticPush, preferredForward, entityTieBreaker);
        if (!TryCandidate(tangentDirection, outSelection.vPosition))
            return false;
        outSelection.bUsedStaticTangent = true;
        return true;
    }
}
