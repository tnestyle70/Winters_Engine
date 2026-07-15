#include "Shared/GameSim/Systems/ChampionAI/ChampionAIInfluenceMap.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

namespace
{
    AiInfluenceSourceV1 MakeSource(
        std::uint32_t netId,
        AiInfluenceLayerV1 layer,
        std::uint8_t flags,
        float x,
        float z,
        float magnitude,
        float radius,
        float eta,
        float confidence)
    {
        AiInfluenceSourceV1 source{};
        source.sourceNetEntityId = netId;
        source.layer = static_cast<std::uint8_t>(layer);
        source.flags = flags;
        source.positionX = x;
        source.positionZ = z;
        source.magnitude = magnitude;
        source.radius = radius;
        source.etaSeconds = eta;
        source.confidence = confidence;
        return source;
    }

    std::uint16_t CellIndex(std::uint8_t x, std::uint8_t z)
    {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(z) * kAiInfluenceMapWidthV1 + x);
    }
}

static_assert(std::is_standard_layout_v<AiInfluenceSourceV1>);
static_assert(std::is_trivial_v<AiInfluenceSourceV1>);
static_assert(std::is_trivially_copyable_v<AiInfluenceSourceV1>);
static_assert(std::is_standard_layout_v<AiInfluenceCellV1>);
static_assert(std::is_trivial_v<AiInfluenceCellV1>);
static_assert(std::is_trivially_copyable_v<AiInfluenceCellV1>);
static_assert(std::is_standard_layout_v<AiInfluenceMapV1>);
static_assert(std::is_trivial_v<AiInfluenceMapV1>);
static_assert(std::is_trivially_copyable_v<AiInfluenceMapV1>);
static_assert(sizeof(AiInfluenceMapV1) < 65536u);

int main()
{
    const AiInfluenceSourceV1 sources[] = {
        MakeSource(
            2002u,
            AiInfluenceLayerV1::ThreatBelief,
            kAiInfluenceBeliefFlagV1,
            2.f, 0.f, 0.8f, 4.f, 1.5f, 0.5f),
        MakeSource(
            1001u,
            AiInfluenceLayerV1::ThreatNow,
            kAiInfluenceCurrentVisibleFlagV1,
            0.f, 0.f, 1.f, 4.f, 0.f, 1.f),
        MakeSource(
            3003u,
            AiInfluenceLayerV1::SupportEta,
            kAiInfluenceCurrentVisibleFlagV1 | kAiInfluenceAlliedFlagV1,
            -2.f, 0.f, 1.f, 4.f, 2.f, 1.f),
        MakeSource(
            0u,
            AiInfluenceLayerV1::EscapeCost,
            kAiInfluenceWalkabilityCostFlagV1,
            0.f, 3.f, 1.f, 2.f, 0.f, 1.f),
    };
    const AiInfluenceSourceV1 permuted[] = {
        sources[3], sources[1], sources[0], sources[2]
    };

    const AiInfluenceMapV1 first = ChampionAIInfluence::BuildMapV1(
        0.f, 0.f, 1.f, sources, 4u);
    const AiInfluenceMapV1 second = ChampionAIInfluence::BuildMapV1(
        0.f, 0.f, 1.f, permuted, 4u);
    const bool sourceOrderIndependent =
        std::memcmp(&first, &second, sizeof(first)) == 0;

    AiInfluenceSourceV1 overCapacity[40]{};
    AiInfluenceSourceV1 overCapacityReversed[40]{};
    for (std::uint8_t i = 0u; i < 40u; ++i)
    {
        overCapacity[i] = MakeSource(
            100u + i,
            AiInfluenceLayerV1::ThreatNow,
            kAiInfluenceCurrentVisibleFlagV1,
            0.f, 0.f, 1.f, 4.f, 0.f, 1.f);
        overCapacityReversed[39u - i] = overCapacity[i];
    }
    const AiInfluenceMapV1 cappedForward =
        ChampionAIInfluence::BuildMapV1(
            0.f, 0.f, 1.f, overCapacity, 40u);
    const AiInfluenceMapV1 cappedReversed =
        ChampionAIInfluence::BuildMapV1(
            0.f, 0.f, 1.f, overCapacityReversed, 40u);
    const bool capacitySelectionOrderIndependent =
        cappedForward.acceptedSourceCount == kAiInfluenceSourceCapacityV1 &&
        std::memcmp(
            &cappedForward,
            &cappedReversed,
            sizeof(cappedForward)) == 0;

    const std::uint16_t center = CellIndex(4u, 4u);
    const std::uint16_t edge = CellIndex(0u, 4u);
    const std::uint8_t threatNow = static_cast<std::uint8_t>(
        AiInfluenceLayerV1::ThreatNow);
    const bool radialFalloffAndSourceTrace =
        first.cells[center].values[threatNow] >
            first.cells[edge].values[threatNow] &&
        first.cells[center].dominantSourceNetEntityIds[threatNow] == 1001u;

    const AiInfluenceSourceV1 leakedCurrentFact = MakeSource(
        9009u,
        AiInfluenceLayerV1::ThreatNow,
        kAiInfluenceBeliefFlagV1,
        0.f, 0.f, 100.f, 4.f, 0.f, 1.f);
    const AiInfluenceMapV1 rejectedLeak = ChampionAIInfluence::BuildMapV1(
        0.f, 0.f, 1.f, &leakedCurrentFact, 1u);
    const bool hiddenCurrentFactRejected =
        rejectedLeak.acceptedSourceCount == 0u &&
        rejectedLeak.cells[center].values[threatNow] == 0.f;

    AiInfluenceSourceV1 invalid = sources[0];
    invalid.confidence = std::numeric_limits<float>::quiet_NaN();
    const AiInfluenceMapV1 rejectedNaN = ChampionAIInfluence::BuildMapV1(
        0.f, 0.f, 1.f, &invalid, 1u);
    const bool nanRejected = rejectedNaN.acceptedSourceCount == 0u;

    const bool schemaAndBounds =
        first.schemaVersion == kAiInfluenceMapSchemaVersionV1 &&
        first.byteSize == sizeof(AiInfluenceMapV1) &&
        first.width == kAiInfluenceMapWidthV1 &&
        first.height == kAiInfluenceMapHeightV1 &&
        first.layerCount == kAiInfluenceLayerCountV1 &&
        first.acceptedSourceCount == 4u;

    const bool pass =
        schemaAndBounds &&
        sourceOrderIndependent &&
        capacitySelectionOrderIndependent &&
        radialFalloffAndSourceTrace &&
        hiddenCurrentFactRejected &&
        nanRejected;
    std::printf(
        "[AIInfluenceMap] %s: schema/POD, source-order determinism, "
        "capacity selection, layer provenance, radial falloff, NaN guard\n",
        pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
