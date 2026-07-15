#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

// Deterministic CPU-side research/debug evidence derived from authoritative
// observations. It is not a live decision input yet. Callers must provide
// observation-filtered sources; this type never reads the world directly and
// therefore cannot turn render visibility into gameplay truth.

inline constexpr std::uint16_t kAiInfluenceMapSchemaVersionV1 = 1u;
inline constexpr std::uint8_t kAiInfluenceMapWidthV1 = 9u;
inline constexpr std::uint8_t kAiInfluenceMapHeightV1 = 9u;
inline constexpr std::uint8_t kAiInfluenceLayerCountV1 = 4u;
inline constexpr std::uint8_t kAiInfluenceSourceCapacityV1 = 32u;
inline constexpr std::uint16_t kAiInfluenceCellCapacityV1 =
    static_cast<std::uint16_t>(
        kAiInfluenceMapWidthV1 * kAiInfluenceMapHeightV1);

enum class AiInfluenceLayerV1 : std::uint8_t
{
    ThreatNow = 0u,
    ThreatBelief = 1u,
    SupportEta = 2u,
    EscapeCost = 3u,
};

inline constexpr std::uint8_t kAiInfluenceCurrentVisibleFlagV1 = 1u << 0;
inline constexpr std::uint8_t kAiInfluenceBeliefFlagV1 = 1u << 1;
inline constexpr std::uint8_t kAiInfluenceAlliedFlagV1 = 1u << 2;
inline constexpr std::uint8_t kAiInfluenceWalkabilityCostFlagV1 = 1u << 3;

struct AiInfluenceSourceV1
{
    std::uint32_t sourceNetEntityId;
    std::uint8_t layer;
    std::uint8_t flags;
    std::uint16_t reserved0;
    float positionX;
    float positionZ;
    float radius;
    float magnitude;
    float etaSeconds;
    float confidence;
};

struct AiInfluenceCellV1
{
    float values[kAiInfluenceLayerCountV1];
    std::uint32_t dominantSourceNetEntityIds[kAiInfluenceLayerCountV1];
    float dominantEtaSeconds[kAiInfluenceLayerCountV1];
    float dominantConfidence[kAiInfluenceLayerCountV1];
    float dominantContribution[kAiInfluenceLayerCountV1];
};

struct AiInfluenceMapV1
{
    std::uint16_t schemaVersion;
    std::uint16_t byteSize;
    std::uint8_t width;
    std::uint8_t height;
    std::uint8_t layerCount;
    std::uint8_t acceptedSourceCount;
    float originX;
    float originZ;
    float cellSize;
    std::uint32_t reserved0;
    AiInfluenceCellV1 cells[kAiInfluenceCellCapacityV1];
};

namespace ChampionAIInfluence
{
    inline bool IsFinite(float value) noexcept
    {
        return std::isfinite(value);
    }

    inline bool IsLayerFlagContractValid(
        AiInfluenceLayerV1 layer,
        std::uint8_t flags) noexcept
    {
        switch (layer)
        {
        case AiInfluenceLayerV1::ThreatNow:
            return (flags & kAiInfluenceCurrentVisibleFlagV1) != 0u &&
                (flags & kAiInfluenceBeliefFlagV1) == 0u;
        case AiInfluenceLayerV1::ThreatBelief:
            return (flags & kAiInfluenceBeliefFlagV1) != 0u &&
                (flags & kAiInfluenceCurrentVisibleFlagV1) == 0u;
        case AiInfluenceLayerV1::SupportEta:
            return (flags & kAiInfluenceAlliedFlagV1) != 0u &&
                (flags & kAiInfluenceCurrentVisibleFlagV1) != 0u;
        case AiInfluenceLayerV1::EscapeCost:
            return (flags & kAiInfluenceWalkabilityCostFlagV1) != 0u;
        default:
            return false;
        }
    }

    inline bool IsSourceValid(const AiInfluenceSourceV1& source) noexcept
    {
        if (source.layer >= kAiInfluenceLayerCountV1 ||
            !IsFinite(source.positionX) ||
            !IsFinite(source.positionZ) ||
            !IsFinite(source.radius) ||
            !IsFinite(source.magnitude) ||
            !IsFinite(source.etaSeconds) ||
            !IsFinite(source.confidence) ||
            source.radius <= 0.f ||
            source.magnitude < 0.f ||
            source.etaSeconds < 0.f ||
            source.confidence < 0.f ||
            source.confidence > 1.f)
        {
            return false;
        }

        return IsLayerFlagContractValid(
            static_cast<AiInfluenceLayerV1>(source.layer),
            source.flags);
    }

    inline AiInfluenceMapV1 MakeMapV1(
        float centerX,
        float centerZ,
        float cellSize) noexcept
    {
        AiInfluenceMapV1 map{};
        map.schemaVersion = kAiInfluenceMapSchemaVersionV1;
        map.byteSize = static_cast<std::uint16_t>(sizeof(AiInfluenceMapV1));
        map.width = kAiInfluenceMapWidthV1;
        map.height = kAiInfluenceMapHeightV1;
        map.layerCount = kAiInfluenceLayerCountV1;
        map.cellSize = IsFinite(cellSize) && cellSize > 0.f ? cellSize : 1.f;
        map.originX = centerX -
            0.5f * static_cast<float>(map.width - 1u) * map.cellSize;
        map.originZ = centerZ -
            0.5f * static_cast<float>(map.height - 1u) * map.cellSize;
        return map;
    }

    inline bool SourceLess(
        const AiInfluenceSourceV1& lhs,
        const AiInfluenceSourceV1& rhs) noexcept
    {
        if (lhs.layer != rhs.layer)
            return lhs.layer < rhs.layer;
        if (lhs.sourceNetEntityId != rhs.sourceNetEntityId)
            return lhs.sourceNetEntityId < rhs.sourceNetEntityId;
        if (lhs.positionX != rhs.positionX)
            return lhs.positionX < rhs.positionX;
        if (lhs.positionZ != rhs.positionZ)
            return lhs.positionZ < rhs.positionZ;
        if (lhs.radius != rhs.radius)
            return lhs.radius < rhs.radius;
        if (lhs.magnitude != rhs.magnitude)
            return lhs.magnitude < rhs.magnitude;
        if (lhs.etaSeconds != rhs.etaSeconds)
            return lhs.etaSeconds < rhs.etaSeconds;
        if (lhs.confidence != rhs.confidence)
            return lhs.confidence < rhs.confidence;
        return lhs.flags < rhs.flags;
    }

    inline float ResolveContribution(
        const AiInfluenceSourceV1& source,
        float distanceSquared) noexcept
    {
        const float radiusSquared = source.radius * source.radius;
        if (distanceSquared > radiusSquared)
            return 0.f;

        const float falloff = 1.f - distanceSquared / radiusSquared;
        float contribution = source.magnitude * source.confidence * falloff;
        if (source.layer == static_cast<std::uint8_t>(
            AiInfluenceLayerV1::SupportEta))
        {
            contribution /= 1.f + source.etaSeconds;
        }
        return contribution;
    }

    inline void AccumulateSource(
        AiInfluenceMapV1& map,
        const AiInfluenceSourceV1& source) noexcept
    {
        const std::uint8_t layer = source.layer;
        for (std::uint8_t z = 0u; z < map.height; ++z)
        {
            const float cellZ = map.originZ +
                static_cast<float>(z) * map.cellSize;
            for (std::uint8_t x = 0u; x < map.width; ++x)
            {
                const float cellX = map.originX +
                    static_cast<float>(x) * map.cellSize;
                const float dx = cellX - source.positionX;
                const float dz = cellZ - source.positionZ;
                const float contribution = ResolveContribution(
                    source,
                    dx * dx + dz * dz);
                if (contribution <= 0.f)
                    continue;

                const std::uint16_t cellIndex = static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(z) * map.width + x);
                AiInfluenceCellV1& cell = map.cells[cellIndex];
                cell.values[layer] += contribution;

                const std::uint32_t previousSource =
                    cell.dominantSourceNetEntityIds[layer];
                const float previousContribution =
                    cell.dominantContribution[layer];
                const bool replaceDominant =
                    contribution > previousContribution ||
                    (contribution == previousContribution &&
                        source.sourceNetEntityId != 0u &&
                        (previousSource == 0u ||
                            source.sourceNetEntityId < previousSource));
                if (replaceDominant)
                {
                    cell.dominantSourceNetEntityIds[layer] =
                        source.sourceNetEntityId;
                    cell.dominantEtaSeconds[layer] = source.etaSeconds;
                    cell.dominantConfidence[layer] = source.confidence;
                    cell.dominantContribution[layer] = contribution;
                }
            }
        }
    }

    inline AiInfluenceMapV1 BuildMapV1(
        float centerX,
        float centerZ,
        float cellSize,
        const AiInfluenceSourceV1* sources,
        std::uint8_t sourceCount) noexcept
    {
        AiInfluenceMapV1 map = MakeMapV1(centerX, centerZ, cellSize);
        if (!sources || sourceCount == 0u)
            return map;

        AiInfluenceSourceV1 accepted[kAiInfluenceSourceCapacityV1]{};
        std::uint8_t acceptedCount = 0u;
        for (std::uint8_t i = 0u; i < sourceCount; ++i)
        {
            if (!IsSourceValid(sources[i]))
                continue;

            const AiInfluenceSourceV1 value = sources[i];
            std::uint8_t insert = 0u;
            while (insert < acceptedCount &&
                !SourceLess(value, accepted[insert]))
            {
                ++insert;
            }

            if (acceptedCount == kAiInfluenceSourceCapacityV1 &&
                insert == acceptedCount)
            {
                continue;
            }

            const std::uint8_t last = acceptedCount <
                kAiInfluenceSourceCapacityV1
                ? acceptedCount
                : static_cast<std::uint8_t>(
                    kAiInfluenceSourceCapacityV1 - 1u);
            for (std::uint8_t move = last; move > insert; --move)
                accepted[move] = accepted[move - 1u];
            accepted[insert] = value;
            if (acceptedCount < kAiInfluenceSourceCapacityV1)
                ++acceptedCount;
        }

        map.acceptedSourceCount = acceptedCount;
        for (std::uint8_t i = 0u; i < acceptedCount; ++i)
            AccumulateSource(map, accepted[i]);
        return map;
    }
}
