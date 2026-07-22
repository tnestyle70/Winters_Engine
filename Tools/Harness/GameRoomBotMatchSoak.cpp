#include "Game/GameRoom.h"

#include "Server/Private/Game/GameRoomInternal.h"

#include "Game/ReplayRecorder.h"
#include "Game/SnapshotBuilder.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Core/Checkpoint/WorldKeyframe.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"
#include "Shared/GameSim/Definitions/TeamPingDef.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

#include "ECS/Components/TransformComponent.h"
#include "Manager/Navigation/NavGrid.h"

#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

CGameRoom* g_pRoom = nullptr;

namespace
{
    constexpr u32_t kDefaultTickCount = 1'800u;
    constexpr u64_t kDefaultSeed = 42u;
    constexpr u32_t kDefaultHeartbeatTicks = 1'800u;
    constexpr u64_t kDefaultPrivateByteLimit = 2ull * 1024ull * 1024ull * 1024ull;
    constexpr u32_t kDefaultEntityLimit = 512u;
    constexpr u64_t kBotInactivityLimitTicks = 1'800u;
    constexpr u64_t kPrivateGrowthLimitBytes = 256ull * 1024ull * 1024ull;
    constexpr u64_t kKeyframeByteLimit = 32ull * 1024ull * 1024ull;
    constexpr u32_t kRespawnGraceTicks = 30u;
    constexpr u32_t kSteadyHandleGrowthLimit = 16u;
    constexpr u32_t kDeadlineMissRateDenominator = 200u;
    constexpr u64_t kMinionLaneStallLimitTicks = 180u;
    constexpr f32_t kMinionProgressSlackSq = 0.01f;

    struct Options
    {
        u32_t tickCount = kDefaultTickCount;
        u64_t seed = kDefaultSeed;
        u32_t roomId = 24u;
        u32_t heartbeatTicks = kDefaultHeartbeatTicks;
        u64_t privateByteLimit = kDefaultPrivateByteLimit;
        u32_t entityLimit = kDefaultEntityLimit;
    };

    struct ProcessMemorySample
    {
        u64_t workingSetBytes = 0u;
        u64_t privateBytes = 0u;
        u32_t handleCount = 0u;
        bool_t bValid = false;
    };

    struct WorldMetrics
    {
        u32_t entityCount = 0u;
        u32_t championCount = 0u;
        u32_t botCount = 0u;
        u32_t deadChampionCount = 0u;
        u32_t minionCount = 0u;
        u32_t structureCount = 0u;
        u32_t projectileCount = 0u;
        u32_t aggroNotificationCount = 0u;
        u32_t zeroComponentEntityCount = 0u;
        u32_t netBindingCount = 0u;
        u32_t keyframeCount = 0u;
        u64_t keyframeBytes = 0u;
        u32_t replayRecordCount = 0u;
        u32_t replaySnapshotCount = 0u;
        u32_t replayEventCount = 0u;
        u32_t replayCommandCount = 0u;
        u64_t replayFirstTick = 0u;
        u64_t replayLastTick = 0u;
        u64_t replayPayloadBytes = 0u;
        u64_t replaySpoolBytes = 0u;
        u32_t pendingExecCount = 0u;
        u32_t pendingReplayCount = 0u;
        u64_t commandSequenceSum = 0u;
        bool_t bFinite = true;
        bool_t bEntityMapConsistent = true;
        bool_t bReplayHealthy = true;
        std::string error{};
    };

    struct LifecycleTracker
    {
        std::unordered_map<EntityID, bool_t> pendingByEntity{};
        std::unordered_map<EntityID, u32_t> previousCommandSequenceByEntity{};
        std::unordered_map<EntityID, u64_t> lastCommandActivityTickByEntity{};
        std::unordered_map<EntityID, u64_t> deathTickByEntity{};
        std::unordered_set<EntityID> commandActiveEntities{};
        u64_t maxCommandInactivityTicks = 0u;
        u32_t inactiveBotCount = 0u;
        u32_t overdueRespawnCount = 0u;
        u32_t deathTransitions = 0u;
        u32_t respawnTransitions = 0u;
        u32_t respawnAIResetFailures = 0u;
        u32_t respawnManaFailures = 0u;
    };

    struct MinionMotionSample
    {
        EntityHandle handle{};
        Vec3 previousPosition{};
        Vec3 activeGoal{};
        f32_t bestWaypointDistanceSq = (std::numeric_limits<f32_t>::max)();
        u32_t activeGoalIndex = 0u;
        u64_t lastProgressTick = 0u;
        bool_t bInitialized = false;
        bool_t bTrackingLaneMove = false;
        bool_t bTrackingPathGoal = false;
    };

    struct MinionMotionTracker
    {
        std::unordered_map<EntityID, MinionMotionSample> samples{};
        std::array<bool_t, 6u> observedLaneSlots{};
        u64_t maxLaneStallTicks = 0u;
        u32_t stalledLaneMinionCount = 0u;
        u32_t opposedYawCount = 0u;
        EntityID firstOpposedYawEntity = NULL_ENTITY;
    };

    struct PracticeControlProbeEvidence
    {
        bool_t bExecuted = false;
        NetEntityId sourceNetId = NULL_NET_ENTITY;
        NetEntityId takenNetId = NULL_NET_ENTITY;
        NetEntityId replacementNetId = NULL_NET_ENTITY;
        u64_t toolRevision = 0u;
        u32_t replayCommandCount = 0u;
    };

    struct TeamPingProbeEvidence
    {
        u32_t blueBotCount = 0u;
        u32_t replayCommandCount = 0u;
        u64_t checkpointBytes = 0u;
        bool_t bThreeHumanLanePreset = false;
        bool_t bAssistProducedNextTickMove = false;
        bool_t bDangerConsumedEquivalentMove = false;
        bool_t bExpired = false;
    };

    struct StructureNavigationProbeEvidence
    {
        bool_t bDeadReleased = false;
        bool_t bPathReleased = false;
        bool_t bLaneReleased = false;
        bool_t bChampionPathReached = false;
        bool_t bMinionPathReached = false;
        bool_t bDerivedAllocationsStable = false;
        bool_t bSecondRefreshNoop = false;
        u64_t refreshTickUs = 0ull;
        u64_t firstChampionPathQueryUs = 0ull;
        u64_t firstMinionPathQueryUs = 0ull;
        u64_t noopTickUs = 0ull;
        u64_t derivedRebuildCalls = 0ull;
        u64_t refreshWorkCalls = 0ull;
        u64_t secondRefreshWorkCalls = 0ull;
    };

    bool_t IsFinite(f32_t value)
    {
        return std::isfinite(value);
    }

    bool_t IsFinite(const Vec3& value)
    {
        return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
    }

    void SetFirstError(WorldMetrics& metrics, const std::string& error)
    {
        if (metrics.error.empty())
            metrics.error = error;
    }

    ProcessMemorySample QueryProcessMemory()
    {
        ProcessMemorySample sample{};
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters)))
        {
            return sample;
        }

        DWORD handleCount = 0u;
        if (!GetProcessHandleCount(GetCurrentProcess(), &handleCount))
            return sample;

        sample.workingSetBytes = static_cast<u64_t>(counters.WorkingSetSize);
        sample.privateBytes = static_cast<u64_t>(counters.PrivateUsage);
        sample.handleCount = static_cast<u32_t>(handleCount);
        sample.bValid = true;
        return sample;
    }

    f64_t BytesToMiB(u64_t bytes)
    {
        return static_cast<f64_t>(bytes) / (1024.0 * 1024.0);
    }

    u64_t HashBytes(const std::vector<u8_t>& bytes)
    {
        u64_t hash = 14695981039346656037ull;
        for (const u8_t byte : bytes)
        {
            hash ^= static_cast<u64_t>(byte);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    bool_t WriteBinaryEvidence(
        const std::filesystem::path& path,
        const std::vector<u8_t>& bytes,
        std::string& outError)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            outError = "failed to open binary evidence file";
            return false;
        }
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!output.good())
        {
            outError = "failed to write binary evidence file";
            return false;
        }
        return true;
    }

    bool_t HashReplayRecordStream(
        const std::filesystem::path& path,
        const WorldMetrics& expected,
        u64_t& outHash,
        u64_t& outByteCount,
        std::string& outError)
    {
        outHash = 14695981039346656037ull;
        outByteCount = 0u;
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            outError = "failed to open finalized replay";
            return false;
        }

        Winters::Replay::ReplayFileHeader header{};
        input.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (input.gcount() != static_cast<std::streamsize>(sizeof(header)) ||
            !Winters::Replay::IsReplayMagic(header) ||
            !Winters::Replay::IsSupportedReplayVersion(header.version) ||
            header.headerSize != sizeof(header))
        {
            outError = "invalid finalized replay header";
            return false;
        }

        if (header.recordCount != expected.replayRecordCount ||
            header.snapshotCount != expected.replaySnapshotCount ||
            header.eventCount != expected.replayEventCount ||
            header.firstTick != expected.replayFirstTick ||
            header.lastTick != expected.replayLastTick)
        {
            outError = "finalized replay header does not match current recorder";
            return false;
        }

        std::error_code sizeError;
        const u64_t fileBytes = static_cast<u64_t>(
            std::filesystem::file_size(path, sizeError));
        if (sizeError || fileBytes != expected.replaySpoolBytes)
        {
            outError = "finalized replay size does not match current spool";
            return false;
        }

        std::array<char, 64u * 1024u> buffer{};
        u32_t snapshotCount = 0u;
        u32_t eventCount = 0u;
        u32_t commandCount = 0u;
        u64_t previousTick = 0u;
        for (u32_t recordIndex = 0u;
            recordIndex < header.recordCount;
            ++recordIndex)
        {
            Winters::Replay::ReplayRecordHeader record{};
            input.read(reinterpret_cast<char*>(&record), sizeof(record));
            if (input.gcount() != static_cast<std::streamsize>(sizeof(record)) ||
                record.headerSize != sizeof(record) ||
                record.payloadSize == 0u ||
                !Winters::Replay::IsReplayRecordTypeSupported(
                    header.version,
                    record.type) ||
                record.serverTick < header.firstTick ||
                record.serverTick > header.lastTick ||
                (recordIndex != 0u && record.serverTick < previousTick))
            {
                outError = "invalid replay record stream";
                return false;
            }

            const auto* headerBytes = reinterpret_cast<const u8_t*>(&record);
            for (size_t i = 0u; i < sizeof(record); ++i)
            {
                outHash ^= static_cast<u64_t>(headerBytes[i]);
                outHash *= 1099511628211ull;
            }
            outByteCount += sizeof(record);
            previousTick = record.serverTick;

            const auto type = static_cast<Winters::Replay::eReplayRecordType>(
                record.type);
            if (type == Winters::Replay::eReplayRecordType::Snapshot)
                ++snapshotCount;
            else if (type == Winters::Replay::eReplayRecordType::Event)
                ++eventCount;
            else if (type == Winters::Replay::eReplayRecordType::Command)
                ++commandCount;

            u32_t remaining = record.payloadSize;
            while (remaining != 0u)
            {
                const u32_t chunk = (std::min)(
                    remaining,
                    static_cast<u32_t>(buffer.size()));
                input.read(buffer.data(), static_cast<std::streamsize>(chunk));
                if (input.gcount() != static_cast<std::streamsize>(chunk))
                {
                    outError = "truncated replay record payload";
                    return false;
                }
                for (u32_t i = 0u; i < chunk; ++i)
                {
                    outHash ^= static_cast<u8_t>(buffer[i]);
                    outHash *= 1099511628211ull;
                }
                outByteCount += chunk;
                remaining -= chunk;
            }
        }

        char trailing = 0;
        input.read(&trailing, 1);
        if (!input.eof() || snapshotCount != expected.replaySnapshotCount ||
            eventCount != expected.replayEventCount ||
            commandCount != expected.replayCommandCount ||
            outByteCount + sizeof(header) != expected.replaySpoolBytes)
        {
            outError = "replay record counts or final size do not match";
            return false;
        }
        return true;
    }

    std::string Hex64(u64_t value)
    {
        std::ostringstream stream;
        stream << std::uppercase << std::hex << std::setfill('0')
            << std::setw(16) << value;
        return stream.str();
    }

    bool_t TryParseUnsigned(const char* text, u64_t& outValue)
    {
        if (!text || *text == '\0' || *text == '-')
            return false;

        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(text, &end, 10);
        if (!end || *end != '\0')
            return false;
        outValue = static_cast<u64_t>(parsed);
        return true;
    }

    bool_t TryParseOptions(int argc, char** argv, Options& outOptions)
    {
        if (argc > 6)
            return false;

        u64_t parsed = 0u;
        if (argc >= 2)
        {
            if (!TryParseUnsigned(argv[1], parsed) || parsed == 0u ||
                parsed > (std::numeric_limits<u32_t>::max)())
            {
                return false;
            }
            outOptions.tickCount = static_cast<u32_t>(parsed);
        }
        if (argc >= 3)
        {
            if (!TryParseUnsigned(argv[2], parsed) || parsed == 0u)
                return false;
            outOptions.seed = parsed;
        }
        if (argc >= 4)
        {
            if (!TryParseUnsigned(argv[3], parsed) || parsed == 0u ||
                parsed > (std::numeric_limits<u32_t>::max)())
            {
                return false;
            }
            outOptions.roomId = static_cast<u32_t>(parsed);
        }
        if (argc >= 5)
        {
            if (!TryParseUnsigned(argv[4], parsed) || parsed == 0u ||
                parsed > (std::numeric_limits<u32_t>::max)())
            {
                return false;
            }
            outOptions.heartbeatTicks = static_cast<u32_t>(parsed);
        }
        if (argc >= 6)
        {
            if (!TryParseUnsigned(argv[5], parsed) || parsed == 0u)
                return false;
            outOptions.privateByteLimit = parsed * 1024ull * 1024ull;
        }
        return true;
    }

    f64_t PercentileMicros(const std::vector<f64_t>& samples, f64_t percentile)
    {
        if (samples.empty())
            return 0.0;

        std::vector<f64_t> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const f64_t rank = percentile * static_cast<f64_t>(sorted.size() - 1u);
        const size_t lower = static_cast<size_t>(rank);
        const size_t upper = (std::min)(lower + 1u, sorted.size() - 1u);
        const f64_t fraction = rank - static_cast<f64_t>(lower);
        return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
    }
}

class CGameRoomIntegrationProbeAccess
{
public:
    static bool_t RunPracticeControlProbe(
        CGameRoom& room,
        u64_t seed,
        PracticeControlProbeEvidence& outEvidence,
        std::string& outError)
    {
        outEvidence = {};
        outError.clear();

#if !defined(_DEBUG)
        (void)room;
        (void)seed;
        return true;
#else
        constexpr u32_t kSessionId = 7001u;
        constexpr u32_t kReloadSequence = 1u;
        constexpr u32_t kTakeSequence = 2u;
        constexpr u32_t kReplaceSequence = 3u;
        constexpr u32_t kPracticeRevision = 17u;
        constexpr eChampion kReplacementChampion = eChampion::EZREAL;

        if (!PreparePracticeControlMatch(room, seed, kSessionId, outError))
            return false;

        std::size_t baseItemCount = 0u;
        const ItemDef* pBaseItems =
            ServerData::GetActiveLoLGameplayDefinitionPack().FindItems(
                baseItemCount);
        const bool_t bHasItem3153 = pBaseItems && std::any_of(
            pBaseItems,
            pBaseItems + baseItemCount,
            [](const ItemDef& item)
            {
                return item.itemId == 3153u;
            });
        if (!bHasItem3153)
        {
            outError = "runtime reload probe base pack is missing item 3153";
            return false;
        }

        const u32_t runtimeRevisionBeforeReload =
            ServerData::GetRuntimeGameplayDefinitionRevision();
        GameCommandWire reload{};
        reload.kind = eCommandKind::PracticeControl;
        reload.sequenceNum = kReloadSequence;
        reload.practiceOperation =
            ePracticeOperation::ReloadGameplayDefinitions;
        if (!ExecuteAcceptedPracticeControlCommand(
            room,
            kSessionId,
            reload,
            outError))
        {
            return false;
        }
        if (ServerData::GetRuntimeGameplayDefinitionRevision() !=
            runtimeRevisionBeforeReload + 1u)
        {
            outError = "runtime definition reload did not publish exactly one revision";
            return false;
        }
        const GameplayDefinitionPack& reloadedPack =
            ServerData::GetActiveLoLGameplayDefinitionPack();
        if (!reloadedPack.economy ||
            std::fabs(reloadedPack.economy->turretGold - 1500.f) > 0.001f ||
            std::fabs(reloadedPack.economy->turretTeamGold - 1000.f) > 0.001f)
        {
            outError = "runtime reload did not overlay turret team gold";
            return false;
        }

        LobbySlotState* pSlots = room.m_pLobbyAuthority->GetSlots();
        const LobbySlotState sourceSlotBeforeTake = pSlots[0];
        const LobbySlotState targetSlotBeforeTake = pSlots[1];
        const EntityID source = room.m_entityMap.FromNet(
            sourceSlotBeforeTake.netId);
        const EntityID target = room.m_entityMap.FromNet(
            targetSlotBeforeTake.netId);
        if (source == NULL_ENTITY || target == NULL_ENTITY ||
            !room.m_world.IsAlive(source) || !room.m_world.IsAlive(target) ||
            room.m_world.HasComponent<ChampionAIComponent>(source) ||
            !room.m_world.HasComponent<ChampionAIComponent>(target))
        {
            outError = "practice control probe initial roles are invalid";
            return false;
        }

        PracticePlayerComponent practice{};
        practice.optionFlags = kPracticeInfiniteManaFlag |
            kPracticeNoCooldownFlag;
        practice.revision = kPracticeRevision;
        room.m_world.AddComponent<PracticePlayerComponent>(source, practice);

        const EntityID ownershipProbe = room.m_world.CreateEntity();
        PracticeSpawnedTag ownership{};
        ownership.ownerEntity = source;
        room.m_world.AddComponent<PracticeSpawnedTag>(ownershipProbe, ownership);
        room.m_PracticeSpawnedEntities.push_back(ownershipProbe);

        GameCommandWire take{};
        take.kind = eCommandKind::PracticeControl;
        take.sequenceNum = kTakeSequence;
        take.targetNet = targetSlotBeforeTake.netId;
        take.practiceOperation =
            ePracticeOperation::TakeControlRosterChampion;
        if (!ExecuteAcceptedPracticeControlCommand(
            room,
            kSessionId,
            take,
            outError))
        {
            return false;
        }

        u8_t controlledSlotId = kInvalidGameRosterSlot;
        EntityID controlledEntity = NULL_ENTITY;
        if (!room.m_pLobbyAuthority->TryGetSessionSlot(
                kSessionId,
                controlledSlotId) ||
            controlledSlotId != targetSlotBeforeTake.slotId ||
            !room.m_sessionBinding.TryGetAlive(
                kSessionId,
                room.m_world,
                controlledEntity) ||
            controlledEntity != target)
        {
            outError = "Take did not move the authoritative session binding";
            return false;
        }

        const LobbySlotState* pSourceAfterTake =
            room.m_pLobbyAuthority->TryGetSlot(sourceSlotBeforeTake.slotId);
        const LobbySlotState* pTargetAfterTake =
            room.m_pLobbyAuthority->TryGetSlot(targetSlotBeforeTake.slotId);
        if (!pSourceAfterTake || !pTargetAfterTake ||
            !pSourceAfterTake->bBot || pSourceAfterTake->bHuman ||
            pSourceAfterTake->sessionId != 0u ||
            !pTargetAfterTake->bHuman || pTargetAfterTake->bBot ||
            pTargetAfterTake->sessionId != kSessionId ||
            !room.m_world.HasComponent<ChampionAIComponent>(source) ||
            room.m_world.HasComponent<ChampionAIComponent>(target))
        {
            outError = "Take did not swap roster and AI ownership roles";
            return false;
        }
        if (room.m_world.HasComponent<PracticePlayerComponent>(source) ||
            !room.m_world.HasComponent<PracticePlayerComponent>(target))
        {
            outError = "Take did not transfer PracticePlayerComponent";
            return false;
        }
        const PracticePlayerComponent& takenPractice =
            room.m_world.GetComponent<PracticePlayerComponent>(target);
        if (takenPractice.optionFlags != practice.optionFlags ||
            takenPractice.revision != kPracticeRevision ||
            room.m_world.GetComponent<PracticeSpawnedTag>(ownershipProbe)
                .ownerEntity != target)
        {
            outError = "Take did not transfer complete practice ownership";
            return false;
        }
        if (!ValidateStrictPracticeRoster(room, kSessionId, outError))
        {
            outError = "Take strict roster invariant: " + outError;
            return false;
        }

        const Vec3 controlledPosition = room.m_world
            .GetComponent<TransformComponent>(target)
            .GetPosition();
        const LobbySlotState controlledSlotBeforeReplace = *pTargetAfterTake;

        GameCommandWire replace{};
        replace.kind = eCommandKind::PracticeControl;
        replace.sequenceNum = kReplaceSequence;
        replace.practiceOperation =
            ePracticeOperation::ReplaceControlledChampion;
        replace.practiceFlags = static_cast<u32_t>(kReplacementChampion);
        if (!ExecuteAcceptedPracticeControlCommand(
            room,
            kSessionId,
            replace,
            outError))
        {
            return false;
        }

        u8_t replacementSlotId = kInvalidGameRosterSlot;
        EntityID replacement = NULL_ENTITY;
        if (!room.m_pLobbyAuthority->TryGetSessionSlot(
                kSessionId,
                replacementSlotId) ||
            replacementSlotId != controlledSlotBeforeReplace.slotId ||
            !room.m_sessionBinding.TryGetAlive(
                kSessionId,
                room.m_world,
                replacement))
        {
            outError = "Fresh replace did not preserve slot/session binding";
            return false;
        }

        const LobbySlotState* pReplacementSlot =
            room.m_pLobbyAuthority->TryGetSlot(replacementSlotId);
        if (!pReplacementSlot ||
            pReplacementSlot->team != controlledSlotBeforeReplace.team ||
            pReplacementSlot->sessionId != kSessionId ||
            !pReplacementSlot->bHuman || pReplacementSlot->bBot ||
            pReplacementSlot->champion != kReplacementChampion ||
            pReplacementSlot->netId == NULL_NET_ENTITY ||
            pReplacementSlot->netId == controlledSlotBeforeReplace.netId ||
            replacement == NULL_ENTITY ||
            replacement == target ||
            !room.m_world.IsAlive(replacement))
        {
            outError = "Fresh replace roster result is invalid";
            return false;
        }
        if (room.m_entityMap.FromNet(controlledSlotBeforeReplace.netId) !=
                NULL_ENTITY ||
            room.m_world.IsAlive(target) ||
            room.m_entityMap.ToNet(replacement) != pReplacementSlot->netId)
        {
            outError = "Fresh replace retained the old entity or NetId mapping";
            return false;
        }

        if (!room.m_world.HasComponent<ChampionComponent>(replacement) ||
            !room.m_world.HasComponent<StatComponent>(replacement) ||
            !room.m_world.HasComponent<GoldComponent>(replacement) ||
            !room.m_world.HasComponent<HealthComponent>(replacement) ||
            !room.m_world.HasComponent<TransformComponent>(replacement) ||
            room.m_world.HasComponent<ChampionAIComponent>(replacement))
        {
            outError = "Fresh replace canonical components are invalid";
            return false;
        }

        const ChampionComponent& replacementChampion =
            room.m_world.GetComponent<ChampionComponent>(replacement);
        const StatComponent& replacementStat =
            room.m_world.GetComponent<StatComponent>(replacement);
        const GoldComponent& replacementGold =
            room.m_world.GetComponent<GoldComponent>(replacement);
        const HealthComponent& replacementHealth =
            room.m_world.GetComponent<HealthComponent>(replacement);
        const Vec3 replacementPosition = room.m_world
            .GetComponent<TransformComponent>(replacement)
            .GetPosition();
        const bool_t bCanonicalResources =
            replacementChampion.id == kReplacementChampion &&
            replacementChampion.team ==
                static_cast<eTeam>(controlledSlotBeforeReplace.team) &&
            replacementChampion.level == 6u &&
            replacementStat.level == 6u &&
            replacementGold.amount == 10000u &&
            replacementHealth.fMaximum > 0.f &&
            std::abs(replacementHealth.fCurrent -
                replacementHealth.fMaximum) <= 0.001f &&
            std::abs(replacementChampion.hp -
                replacementChampion.maxHp) <= 0.001f &&
            std::abs(replacementChampion.hp -
                replacementHealth.fCurrent) <= 0.001f &&
            std::abs(replacementChampion.maxHp -
                replacementHealth.fMaximum) <= 0.001f &&
            replacementChampion.maxMana > 0.f &&
            std::abs(replacementChampion.mana -
                replacementChampion.maxMana) <= 0.001f;
        const bool_t bPositionPreserved =
            std::abs(replacementPosition.x - controlledPosition.x) <= 0.001f &&
            std::abs(replacementPosition.y - controlledPosition.y) <= 0.001f &&
            std::abs(replacementPosition.z - controlledPosition.z) <= 0.001f;
        if (!bCanonicalResources || !bPositionPreserved)
        {
            outError = "Fresh replace did not produce the canonical full baseline";
            return false;
        }
        if (!room.m_world.HasComponent<PracticePlayerComponent>(replacement))
        {
            outError = "Fresh replace did not transfer PracticePlayerComponent";
            return false;
        }
        const PracticePlayerComponent& replacementPractice =
            room.m_world.GetComponent<PracticePlayerComponent>(replacement);
        if (replacementPractice.optionFlags != practice.optionFlags ||
            replacementPractice.revision != kPracticeRevision ||
            room.m_world.GetComponent<PracticeSpawnedTag>(ownershipProbe)
                .ownerEntity != replacement)
        {
            outError = "Fresh replace did not transfer complete practice ownership";
            return false;
        }
        if (!ValidateStrictPracticeRoster(room, kSessionId, outError))
        {
            outError = "Fresh replace strict roster invariant: " + outError;
            return false;
        }
        if (room.m_toolRevision != 3u ||
            !room.m_pReplayRecorder ||
            room.m_pReplayRecorder->GetCommandCount() != 3u)
        {
            outError = "Reload/Take/Fresh accepted replay/tool outcomes are not exact";
            return false;
        }

        outEvidence.bExecuted = true;
        outEvidence.sourceNetId = sourceSlotBeforeTake.netId;
        outEvidence.takenNetId = controlledSlotBeforeReplace.netId;
        outEvidence.replacementNetId = pReplacementSlot->netId;
        outEvidence.toolRevision = room.m_toolRevision;
        outEvidence.replayCommandCount = room.m_pReplayRecorder
            ? room.m_pReplayRecorder->GetCommandCount()
            : 0u;

        room.m_PracticeSpawnedEntities.erase(
            std::remove(
                room.m_PracticeSpawnedEntities.begin(),
                room.m_PracticeSpawnedEntities.end(),
                ownershipProbe),
            room.m_PracticeSpawnedEntities.end());
        room.m_world.DestroyEntity(ownershipProbe);
        return true;
#endif
    }

    static bool_t RunTeamPingCommandProbe(
        CGameRoom& room,
        u64_t seed,
        TeamPingProbeEvidence& outEvidence,
        std::string& outError)
    {
        outEvidence = {};
        outError.clear();

#if !defined(_DEBUG)
        (void)room;
        (void)seed;
        return true;
#else
        constexpr u32_t kSessionId = 7002u;
        if (!room.m_pLobbyAuthority)
        {
            outError = "team ping probe lobby authority is missing";
            return false;
        }

        CLobbyAuthority& laneAuthority = *room.m_pLobbyAuthority;
        laneAuthority.InitializeSlots();
        laneAuthority.m_hostSessionId = kSessionId;
        laneAuthority.m_phase = eRoomPhase::ChampionSelect;
        LobbySlotState* pLaneSlots = laneAuthority.GetSlots();
        u8_t redLaneBefore[5]{};
        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            LobbySlotState& slot = pLaneSlots[i];
            slot.bHuman = i < 3u;
            slot.bBot = i >= 3u;
            slot.sessionId = i < 3u ? kSessionId + i : 0u;
            slot.champion = eChampion::ASHE;
            if (i >= 5u)
                redLaneBefore[i - 5u] = slot.botLane;
        }
        if (!laneAuthority.TryStartGame(kSessionId) ||
            pLaneSlots[3].botLane !=
                static_cast<u8_t>(Winters::Map::eLane::Bot) ||
            pLaneSlots[4].botLane !=
                static_cast<u8_t>(Winters::Map::eLane::Top))
        {
            outError = "3-human + 2-bot lane preset did not produce Bot/Top";
            return false;
        }
        for (u32_t i = 5u; i < kGameRosterSlotCount; ++i)
        {
            if (pLaneSlots[i].botLane != redLaneBefore[i - 5u])
            {
                outError = "3-human lane preset changed the enemy five-bot lanes";
                return false;
            }
        }
        outEvidence.bThreeHumanLanePreset = true;

        if (!PreparePracticeControlMatch(room, seed, kSessionId, outError))
            return false;

        LobbySlotState* pSlots = room.m_pLobbyAuthority->GetSlots();
        const EntityID issuer = room.m_entityMap.FromNet(pSlots[0].netId);
        std::vector<EntityID> blueBots;
        std::vector<EntityID> redBots;
        for (u32_t i = 1u; i < kGameRosterSlotCount; ++i)
        {
            const EntityID entity = room.m_entityMap.FromNet(pSlots[i].netId);
            if (entity == NULL_ENTITY || !room.m_world.IsAlive(entity))
            {
                outError = "team ping probe roster entity is invalid";
                return false;
            }
            (pSlots[i].team == 0u ? blueBots : redBots).push_back(entity);
        }
        if (issuer == NULL_ENTITY || blueBots.size() != 4u || redBots.size() != 5u)
        {
            outError = "team ping probe expected one blue human, four blue bots, five red bots";
            return false;
        }

        const Vec3 issuerPos =
            room.m_world.GetComponent<TransformComponent>(issuer).GetPosition();
        for (u32_t index = 0u; index < blueBots.size(); ++index)
        {
            const EntityID bot = blueBots[index];
            room.m_world.GetComponent<TransformComponent>(bot).SetPosition(
                issuerPos + Vec3{ static_cast<f32_t>(index + 1u), 0.f, 0.f });
            room.m_world.GetComponent<GoldComponent>(bot).amount = 0u;
        }
        for (u32_t index = 0u; index < redBots.size(); ++index)
        {
            room.m_world.GetComponent<TransformComponent>(redBots[index]).SetPosition(
                Vec3{ 300.f + static_cast<f32_t>(index), 0.f, 300.f });
        }

        const auto SubmitPing = [&](const GameCommandWire& wire) -> bool_t
        {
            const u64_t tickBefore = room.m_tickIndex;
            const u32_t replayBefore = room.m_pReplayRecorder->GetCommandCount();
            room.EnqueueCommand(kSessionId, wire, room.m_tickIndex, 0u, 0u);
            room.Tick();
            const auto ack = room.m_lastSimCommandSeqBySession.find(kSessionId);
            if (room.m_tickIndex != tickBefore + 1u ||
                ack == room.m_lastSimCommandSeqBySession.end() ||
                ack->second != wire.sequenceNum ||
                room.m_pReplayRecorder->GetCommandCount() != replayBefore + 1u ||
                !room.m_pendingExecCommands.empty() ||
                !room.m_pendingReplayCommands.empty())
            {
                outError = "team ping was not ACKed/journaled exactly once";
                return false;
            }
            return true;
        };

        GameCommandWire assist{};
        assist.kind = eCommandKind::TeamPing;
        assist.sequenceNum = 1u;
        assist.slot = static_cast<u8_t>(eTeamPingKind::Assist);
        assist.groundPos = issuerPos + Vec3{ 10.f, 0.f, 0.f };
        if (!SubmitPing(assist))
            return false;

        for (EntityID bot : blueBots)
        {
            const ChampionAIComponent& ai =
                room.m_world.GetComponent<ChampionAIComponent>(bot);
            if (!ai.bTeamPingObjectiveActive ||
                ai.teamPingKind != eTeamPingKind::Assist ||
                ai.teamPingExpireTick <= room.m_tickIndex)
            {
                outError = "Assist did not reach every same-team bot";
                return false;
            }
        }
        for (EntityID bot : redBots)
        {
            if (room.m_world.GetComponent<ChampionAIComponent>(bot)
                    .bTeamPingObjectiveActive)
            {
                outError = "Assist leaked to enemy bot";
                return false;
            }
        }

        std::vector<u8_t> checkpoint;
        if (!SimCheckpoint::SaveWorldKeyframe(
                room.m_world,
                room.m_rng,
                room.m_entityMap,
                room.m_tickIndex,
                checkpoint))
        {
            outError = "team ping checkpoint save failed";
            return false;
        }
        CWorld restoredWorld;
        DeterministicRng restoredRng(1ull);
        EntityIdMap restoredEntityMap;
        u64_t restoredTick = 0u;
        if (!SimCheckpoint::RestoreWorldKeyframe(
                restoredWorld,
                restoredRng,
                restoredEntityMap,
                restoredTick,
                checkpoint) ||
            restoredTick != room.m_tickIndex ||
            !restoredWorld.GetComponent<ChampionAIComponent>(blueBots.front())
                .bTeamPingObjectiveActive ||
            restoredWorld.GetComponent<ChampionAIComponent>(blueBots.front())
                .teamPingKind != eTeamPingKind::Assist)
        {
            outError = "team ping checkpoint restore lost Assist state";
            return false;
        }

        const u32_t assistSequenceBefore =
            room.m_world.GetComponent<ChampionAIComponent>(blueBots.front())
                .nextCommandSequence;
        room.Tick();
        const ChampionAIComponent& assistAI =
            room.m_world.GetComponent<ChampionAIComponent>(blueBots.front());
        outEvidence.bAssistProducedNextTickMove =
            assistAI.nextCommandSequence > assistSequenceBefore &&
            assistAI.debugLastCommandKind == static_cast<u8_t>(eCommandKind::Move);
        if (!outEvidence.bAssistProducedNextTickMove)
        {
            outError = "Assist did not produce a next-tick bot Move command";
            return false;
        }

        const EntityID dangerBot = blueBots.front();
        const Vec3 dangerAnchor =
            room.m_world.GetComponent<TransformComponent>(dangerBot).GetPosition();
        room.m_world.GetComponent<TransformComponent>(blueBots.back()).SetPosition(
            dangerAnchor + Vec3{ kTeamPingDangerRadius + 5.f, 0.f, 0.f });
        GameCommandWire danger{};
        danger.kind = eCommandKind::TeamPing;
        danger.sequenceNum = 2u;
        danger.slot = static_cast<u8_t>(eTeamPingKind::Danger);
        danger.groundPos = dangerAnchor;
        if (!SubmitPing(danger))
            return false;

        ChampionAIComponent& dangerState =
            room.m_world.GetComponent<ChampionAIComponent>(dangerBot);
        const Vec3 safeAnchor = dangerState.safeAnchor;
        const u64_t dangerExpireTick = dangerState.teamPingExpireTick;
        if (dangerState.teamPingKind != eTeamPingKind::Danger ||
            dangerExpireTick <= room.m_tickIndex)
        {
            outError = "Danger did not overwrite Assist with a fresh TTL";
            return false;
        }
        const ChampionAIComponent& ineligibleDangerAI =
            room.m_world.GetComponent<ChampionAIComponent>(blueBots.back());
        if (ineligibleDangerAI.bTeamPingObjectiveActive ||
            ineligibleDangerAI.teamPingKind != eTeamPingKind::None)
        {
            outError = "Danger ping-time radius eligibility was not fixed";
            return false;
        }

        MoveTargetComponent& equivalentMove =
            room.m_world.GetComponent<MoveTargetComponent>(dangerBot);
        equivalentMove.target = safeAnchor;
        equivalentMove.bHasTarget = true;
        dangerState.decisionTimer = 0.f;
        room.Tick();
        const MoveTargetComponent& dangerMove =
            room.m_world.GetComponent<MoveTargetComponent>(dangerBot);
        outEvidence.bDangerConsumedEquivalentMove =
            dangerMove.bHasTarget &&
            WintersMath::DistanceSqXZ(dangerMove.target, safeAnchor) <= 0.25f;
        if (!outEvidence.bDangerConsumedEquivalentMove)
        {
            outError = "Danger equivalent Move was overwritten by normal macro";
            return false;
        }

        room.m_world.GetComponent<TransformComponent>(dangerBot).SetPosition(
            dangerAnchor + Vec3{ kTeamPingDangerRadius + 5.f, 0.f, 0.f });
        room.m_world.GetComponent<ChampionAIComponent>(dangerBot).decisionTimer = 0.f;
        room.Tick();
        const ChampionAIComponent& outsideRadiusAI =
            room.m_world.GetComponent<ChampionAIComponent>(dangerBot);
        const MoveTargetComponent& outsideRadiusMove =
            room.m_world.GetComponent<MoveTargetComponent>(dangerBot);
        if (!outsideRadiusAI.bTeamPingObjectiveActive ||
            outsideRadiusAI.teamPingKind != eTeamPingKind::Danger ||
            !outsideRadiusMove.bHasTarget ||
            WintersMath::DistanceSqXZ(
                outsideRadiusMove.target, safeAnchor) > 0.25f)
        {
            outError = "eligible Danger bot stopped retreating after leaving radius";
            return false;
        }

        while (room.m_tickIndex <= dangerExpireTick)
            room.Tick();
        const ChampionAIComponent& expiredAI =
            room.m_world.GetComponent<ChampionAIComponent>(dangerBot);
        outEvidence.bExpired =
            !expiredAI.bTeamPingObjectiveActive &&
            expiredAI.teamPingKind == eTeamPingKind::None;
        if (!outEvidence.bExpired)
        {
            outError = "Danger TTL did not expire back to normal macro";
            return false;
        }

        outEvidence.blueBotCount = static_cast<u32_t>(blueBots.size());
        outEvidence.replayCommandCount =
            room.m_pReplayRecorder->GetCommandCount();
        outEvidence.checkpointBytes = static_cast<u64_t>(checkpoint.size());
        return true;
#endif
    }

    static bool_t PrepareBotMatch(CGameRoom& room, u64_t seed, std::string& outError)
    {
        outError.clear();
        if (!room.m_pLobbyAuthority)
        {
            outError = "lobby authority is missing";
            return false;
        }

        static constexpr std::array<eChampion, kGameRosterSlotCount> kRoster =
        {
            eChampion::YASUO,
            eChampion::ZED,
            eChampion::ASHE,
            eChampion::ANNIE,
            eChampion::LEESIN,
            eChampion::RIVEN,
            eChampion::SYLAS,
            eChampion::VIEGO,
            eChampion::YONE,
            eChampion::JAX,
        };
        static constexpr std::array<u8_t, 5u> kTeamLanes =
        {
            static_cast<u8_t>(Winters::Map::eLane::Top),
            static_cast<u8_t>(Winters::Map::eLane::Mid),
            static_cast<u8_t>(Winters::Map::eLane::Bot),
            static_cast<u8_t>(Winters::Map::eLane::Bot),
            static_cast<u8_t>(Winters::Map::eLane::Mid),
        };

        room.m_rng.SetState(seed);
        LobbySlotState* slots = room.m_pLobbyAuthority->GetSlots();
        if (!slots || room.m_pLobbyAuthority->GetSlotCount() != kGameRosterSlotCount)
        {
            outError = "unexpected lobby slot storage";
            return false;
        }

        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            LobbySlotState slot{};
            slot.slotId = static_cast<u8_t>(i);
            slot.team = i < 5u ? 0u : 1u;
            slot.bBot = true;
            slot.champion = kRoster[i];
            slot.botDifficulty = 2u;
            slot.botLane = kTeamLanes[i % 5u];
            slot.bReady = true;
            slot.bLocked = true;
            slots[i] = slot;
        }

        room.SpawnChampionsFromLobby();
        room.SpawnServerGameplayObjects();
        room.m_pLobbyAuthority->m_phase = eRoomPhase::InGame;
        room.m_serverMinionWaves.ScheduleFirstWave(room.m_tickIndex, {});

        WorldMetrics metrics = CollectMetrics(room);
        if (metrics.championCount != kGameRosterSlotCount ||
            metrics.botCount != kGameRosterSlotCount)
        {
            std::ostringstream stream;
            stream << "expected 10 champions and 10 bots, got champions="
                << metrics.championCount << " bots=" << metrics.botCount;
            outError = stream.str();
            return false;
        }
        if (!metrics.bFinite || !metrics.bEntityMapConsistent)
        {
            outError = metrics.error.empty()
                ? "initial world invariant failed"
                : metrics.error;
            return false;
        }
        return true;
    }

    static bool_t RunStructureNavigationRefreshProbe(
        CGameRoom& room,
        StructureNavigationProbeEvidence& outEvidence,
        std::string& outError)
    {
        outEvidence = {};
        outError.clear();
        if (!room.m_pTerrainNavGrid || !room.m_pNavGrid)
        {
            outError = "server terrain/nav grid is missing";
            return false;
        }

        EntityID structureEntity = NULL_ENTITY;
        Engine::CNavGrid::Cell probeCell{};
        Engine::CNavGrid::Cell probeStartCell{};
        const MinionBehaviorDef& minionBehavior =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack().minionBehavior;
        room.m_world.ForEach<StructureComponent, TransformComponent, HealthComponent>(
            [&](EntityID entity,
                StructureComponent&,
                TransformComponent& transform,
                HealthComponent& health)
            {
                if (structureEntity != NULL_ENTITY ||
                    health.bIsDead ||
                    health.fCurrent <= 0.f)
                {
                    return;
                }

                const Engine::CNavGrid::Cell center =
                    room.m_pTerrainNavGrid->WorldToCell(transform.GetPosition());
                for (int32_t dy = -12;
                    dy <= 12 && structureEntity == NULL_ENTITY;
                    ++dy)
                {
                    for (int32_t dx = -12; dx <= 12; ++dx)
                    {
                        const Engine::CNavGrid::Cell candidate{
                            center.x + dx,
                            center.y + dy
                        };
                        const Vec3 candidateWorld =
                            room.m_pTerrainNavGrid->CellToWorld(
                                candidate.x,
                                candidate.y);
                        if (!room.m_pTerrainNavGrid->IsAreaWalkable(
                                candidateWorld,
                                minionBehavior.pathAgentRadius) ||
                            !room.m_pTerrainNavGrid->IsAreaWalkable(
                                candidateWorld,
                                minionBehavior.laneClearanceRadius) ||
                            room.m_pPathNavGrid->IsWalkable(
                                candidate.x, candidate.y) ||
                            room.m_pMinionLaneNavGrid->IsWalkable(
                                candidate.x, candidate.y))
                        {
                            continue;
                        }

                        bool_t bFoundStart = false;
                        for (int32_t radius = 1;
                            radius <= 16 && !bFoundStart;
                            ++radius)
                        {
                            for (int32_t sy = -radius;
                                sy <= radius && !bFoundStart;
                                ++sy)
                            {
                                for (int32_t sx = -radius;
                                    sx <= radius;
                                    ++sx)
                                {
                                    if (std::abs(sx) != radius &&
                                        std::abs(sy) != radius)
                                    {
                                        continue;
                                    }
                                    const Engine::CNavGrid::Cell start{
                                        candidate.x + sx,
                                        candidate.y + sy };
                                    if (!room.m_pPathNavGrid->IsWalkable(
                                            start.x, start.y) ||
                                        !room.m_pMinionLaneNavGrid->IsWalkable(
                                            start.x, start.y))
                                    {
                                        continue;
                                    }
                                    probeStartCell = start;
                                    bFoundStart = true;
                                    break;
                                }
                            }
                        }
                        if (bFoundStart)
                        {
                            structureEntity = entity;
                            probeCell = candidate;
                            break;
                        }
                    }
                }
            });
        if (structureEntity == NULL_ENTITY)
        {
            outError = "no live structure-carved terrain cell was found";
            return false;
        }

        const auto* pPathGridBefore = room.m_pPathNavGrid.get();
        const auto* pLaneGridBefore = room.m_pMinionLaneNavGrid.get();
        const u64_t buildCountBefore = room.m_serverPathNavGridBuildCount;
        const u64_t refreshCountBefore =
            room.m_serverStructureNavigationRefreshCount;

        EntityID sourceChampion = NULL_ENTITY;
        const eTeam structureTeam =
            room.m_world.GetComponent<StructureComponent>(structureEntity).team;
        room.m_world.ForEach<ChampionComponent>(
            [&](EntityID entity, ChampionComponent& champion)
            {
                if (sourceChampion == NULL_ENTITY && champion.team != structureTeam)
                    sourceChampion = entity;
            });
        if (sourceChampion == NULL_ENTITY)
        {
            outError = "no opposing champion for structure damage probe";
            return false;
        }

        const f32_t structureMaximum =
            room.m_world.GetComponent<HealthComponent>(structureEntity).fMaximum;
        DamageRequest lethal{};
        lethal.source = sourceChampion;
        lethal.target = structureEntity;
        lethal.sourceTeam = room.m_world.GetComponent<ChampionComponent>(
            sourceChampion).team;
        lethal.type = eDamageType::True;
        lethal.flatAmount = structureMaximum + 1000.f;
        lethal.eSourceKind = eDamageSourceKind::BasicAttack;
        EnqueueDamageRequest(room.m_world, lethal);

        room.Tick();
        const HealthComponent& killedHealth =
            room.m_world.GetComponent<HealthComponent>(structureEntity);
        if (!killedHealth.bIsDead || killedHealth.fCurrent > 0.f)
        {
            outError = "queued lethal structure damage was not applied";
            return false;
        }

        const auto refreshStart = std::chrono::steady_clock::now();
        room.Tick();
        const auto refreshEnd = std::chrono::steady_clock::now();
        outEvidence.refreshTickUs = static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                refreshEnd - refreshStart).count());
        outEvidence.bDeadReleased =
            room.m_pNavGrid->IsWalkable(probeCell.x, probeCell.y);
        outEvidence.bPathReleased =
            room.m_pPathNavGrid->IsWalkable(probeCell.x, probeCell.y);
        outEvidence.bLaneReleased =
            room.m_pMinionLaneNavGrid->IsWalkable(
                probeCell.x, probeCell.y);
        outEvidence.bDerivedAllocationsStable =
            room.m_pPathNavGrid.get() == pPathGridBefore &&
            room.m_pMinionLaneNavGrid.get() == pLaneGridBefore;
        outEvidence.derivedRebuildCalls =
            room.m_serverPathNavGridBuildCount - buildCountBefore;
        outEvidence.refreshWorkCalls =
            room.m_serverStructureNavigationRefreshCount - refreshCountBefore;

        Vec3 pathStart = room.m_pPathNavGrid->CellToWorld(
            probeStartCell.x, probeStartCell.y);
        Vec3 pathGoal = room.m_pPathNavGrid->CellToWorld(
            probeCell.x, probeCell.y);
        Vec3 championWaypoints[64]{};
        u16_t championWaypointCount = 0u;
        Vec3 championResolved{};
        const auto championPathStart = std::chrono::steady_clock::now();
        const bool_t bChampionPathBuilt = room.TryBuildMovePath(
            pathStart,
            pathGoal,
            championWaypoints,
            static_cast<u16_t>(std::size(championWaypoints)),
            championWaypointCount,
            championResolved);
        const auto championPathEnd = std::chrono::steady_clock::now();
        outEvidence.firstChampionPathQueryUs = static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                championPathEnd - championPathStart).count());
        outEvidence.bChampionPathReached =
            bChampionPathBuilt &&
            championWaypointCount > 0u &&
            room.m_pPathNavGrid->WorldToCell(championResolved).x ==
                probeCell.x &&
            room.m_pPathNavGrid->WorldToCell(championResolved).y ==
                probeCell.y;

        Vec3 minionWaypoints[MinionStateComponent::PathMaxWaypoints]{};
        u16_t minionWaypointCount = 0u;
        Vec3 minionResolved{};
        const auto minionPathStart = std::chrono::steady_clock::now();
        const bool_t bMinionPathBuilt = room.TryBuildServerMinionMovePath(
            pathStart,
            pathGoal,
            minionWaypoints,
            MinionStateComponent::PathMaxWaypoints,
            minionWaypointCount,
            minionResolved);
        const auto minionPathEnd = std::chrono::steady_clock::now();
        outEvidence.firstMinionPathQueryUs = static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                minionPathEnd - minionPathStart).count());
        outEvidence.bMinionPathReached =
            bMinionPathBuilt &&
            minionWaypointCount > 0u &&
            room.m_pMinionLaneNavGrid->WorldToCell(minionResolved).x ==
                probeCell.x &&
            room.m_pMinionLaneNavGrid->WorldToCell(minionResolved).y ==
                probeCell.y;

        const u64_t settledHash = room.m_serverStructureNavigationStateHash;
        const u64_t refreshCountAfter =
            room.m_serverStructureNavigationRefreshCount;
        const auto noopStart = std::chrono::steady_clock::now();
        room.Tick();
        const auto noopEnd = std::chrono::steady_clock::now();
        outEvidence.noopTickUs = static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                noopEnd - noopStart).count());
        outEvidence.bSecondRefreshNoop =
            room.m_serverStructureNavigationStateHash == settledHash &&
            room.m_pPathNavGrid.get() == pPathGridBefore &&
            room.m_pMinionLaneNavGrid.get() == pLaneGridBefore;
        outEvidence.secondRefreshWorkCalls =
            room.m_serverStructureNavigationRefreshCount - refreshCountAfter;

        if (!outEvidence.bDeadReleased ||
            !outEvidence.bPathReleased ||
            !outEvidence.bLaneReleased ||
            !outEvidence.bChampionPathReached ||
            !outEvidence.bMinionPathReached ||
            !outEvidence.bDerivedAllocationsStable ||
            !outEvidence.bSecondRefreshNoop ||
            outEvidence.derivedRebuildCalls != 0ull ||
            outEvidence.refreshWorkCalls != 1ull ||
            outEvidence.secondRefreshWorkCalls != 0ull ||
            outEvidence.refreshTickUs > 33333ull ||
            outEvidence.firstChampionPathQueryUs > 33333ull ||
            outEvidence.firstMinionPathQueryUs > 33333ull)
        {
            outError = "structure death nav refresh exceeded its runtime contract";
            return false;
        }
        return true;
    }

    static bool_t RunMinionCombatExitWaypointProbe(
        CGameRoom& room,
        std::string& outError)
    {
        constexpr eTeam team = eTeam::Blue;
        constexpr u8_t lane = static_cast<u8_t>(Winters::Map::eLane::Mid);
        const u8_t waypointLane = ResolveServerWaypointLane(team, lane);
        const u32_t waypointCount = room.GetServerMinionWaypointCount(
            team, waypointLane);
        if (waypointCount < 3u)
        {
            outError = "minion combat-exit probe needs three waypoints";
            return false;
        }

        constexpr u32_t currentIndex = 1u;
        const Vec3 previous = room.GetServerMinionWaypoint(
            team, waypointLane, currentIndex - 1u);
        const Vec3 current = room.GetServerMinionWaypoint(
            team, waypointLane, currentIndex);
        const Vec3 next = room.GetServerMinionWaypoint(
            team, waypointLane, currentIndex + 1u);
        const Vec3 forward = WintersMath::DirectionXZ(previous, current, Vec3{});
        const Vec3 spawnPos{
            current.x + forward.x * 1.1f,
            current.y,
            current.z + forward.z * 1.1f };
        const EntityID minion = room.SpawnServerMinion(team, 0u, lane, spawnPos);
        if (minion == NULL_ENTITY)
        {
            outError = "minion combat-exit probe spawn failed";
            return false;
        }

        const EntityID deadTarget = room.m_world.CreateEntity();
        const Vec3 staleTargetPos{
            spawnPos.x - forward.x * 8.f,
            spawnPos.y,
            spawnPos.z - forward.z * 8.f };
        TransformComponent deadTransform{};
        deadTransform.SetPosition(staleTargetPos);
        room.m_world.AddComponent<TransformComponent>(deadTarget, deadTransform);
        HealthComponent deadHealth{};
        deadHealth.fCurrent = 0.f;
        deadHealth.fMaximum = 100.f;
        deadHealth.bIsDead = true;
        room.m_world.AddComponent<HealthComponent>(deadTarget, deadHealth);

        auto& state = room.m_world.GetComponent<MinionStateComponent>(minion);
        state.current = MinionStateComponent::Chase;
        state.currentWaypoint = currentIndex;
        state.attackTargetId = deadTarget;
        state.targetScanCooldown = 1.f;
        state.PathTarget = staleTargetPos;
        state.PathResolvedTarget = staleTargetPos;
        state.PathWaypoints[0] = staleTargetPos;
        state.PathCount = 1u;
        state.PathIndex = 0u;
        state.PathRebuildCooldown = 0.2f;

        const Vec3 before = room.m_world.GetComponent<TransformComponent>(minion)
            .GetPosition();
        room.Tick();
        const Vec3 after = room.m_world.GetComponent<TransformComponent>(minion)
            .GetPosition();
        const auto& afterState =
            room.m_world.GetComponent<MinionStateComponent>(minion);
        const Vec3 displacement{
            after.x - before.x, 0.f, after.z - before.z };
        const Vec3 laneForward = WintersMath::DirectionXZ(current, next, Vec3{});
        const bool_t stalePathCleared =
            afterState.PathCount == 0u ||
            WintersMath::DistanceSqXZ(
                afterState.PathTarget, staleTargetPos) > 0.01f;
        const bool_t monotonic =
            afterState.currentWaypoint == currentIndex + 1u;
        const bool_t backward =
            displacement.x * laneForward.x +
            displacement.z * laneForward.z < -0.05f;
        if (!stalePathCleared || !monotonic || backward)
        {
            outError = "minion combat exit reused stale path or waypoint";
            return false;
        }
        return true;
    }

    static bool_t RunAIDebugSnapshotEvidenceProbe(
        CGameRoom& room,
        std::string& outError,
        std::size_t& outBaselineBytes,
        std::size_t& outEvidenceBytes)
    {
        outError.clear();
        outBaselineBytes = 0u;
        outEvidenceBytes = 0u;

#if !defined(_DEBUG)
        (void)room;
        return true;
#else
        EntityID bot = NULL_ENTITY;
        room.m_world.ForEach<ChampionAIComponent>(
            [&](EntityID entity, ChampionAIComponent&)
            {
                if (bot == NULL_ENTITY &&
                    room.m_world.HasComponent<TransformComponent>(entity) &&
                    room.m_world.HasComponent<NetEntityIdComponent>(entity))
                {
                    bot = entity;
                }
            });
        if (bot == NULL_ENTITY)
        {
            outError = "AI snapshot probe found no replicated bot";
            return false;
        }

        ChampionAIComponent& ai =
            room.m_world.GetComponent<ChampionAIComponent>(bot);
        const ChampionAIComponent savedAI = ai;
        const bool_t bHadResearch =
            room.m_world.HasComponent<ChampionAIResearchDebugComponent>(bot);
        const ChampionAIResearchDebugComponent savedResearch = bHadResearch
            ? room.m_world.GetComponent<ChampionAIResearchDebugComponent>(bot)
            : ChampionAIResearchDebugComponent{};
        if (!bHadResearch)
        {
            room.m_world.AddComponent<ChampionAIResearchDebugComponent>(
                bot,
                ChampionAIResearchDebugComponent{});
        }
        ChampionAIResearchDebugComponent& research =
            room.m_world.GetComponent<ChampionAIResearchDebugComponent>(bot);
        const auto Restore = [&]()
        {
            ai = savedAI;
            if (bHadResearch)
            {
                room.m_world.GetComponent<ChampionAIResearchDebugComponent>(
                    bot) = savedResearch;
            }
            else if (room.m_world.HasComponent<ChampionAIResearchDebugComponent>(bot))
            {
                room.m_world.RemoveComponent<ChampionAIResearchDebugComponent>(bot);
            }
        };
        const auto Fail = [&](const char* message)
        {
            outError = message;
            Restore();
            return false;
        };

        constexpr u64_t kEvidenceTick = 77u;
        const NetEntityId botNetId = room.m_entityMap.ToNet(bot);
        std::array<SkillCommandFeedback, 5u> feedback{};
        auto snapshotBuilder = CSnapshotBuilder::Create();
        const auto BuildAt = [&](u64_t tick)
        {
            return snapshotBuilder->Build(
                room.m_world,
                room.m_entityMap,
                tick,
                tick * 33u,
                room.m_rng.GetState(),
                0u,
                feedback,
                botNetId,
                1u,
                1u,
                1u,
                false,
                1.f);
        };
        const auto VerifyAndGet = [](
            const flatbuffers::DetachedBuffer& buffer,
            const Shared::Schema::Snapshot*& outSnapshot)
        {
            flatbuffers::Verifier verifier(buffer.data(), buffer.size());
            if (!Shared::Schema::VerifySnapshotBuffer(verifier))
                return false;
            outSnapshot = Shared::Schema::GetSnapshot(buffer.data());
            return outSnapshot != nullptr;
        };
        const auto FindBotSnapshot = [&](const Shared::Schema::Snapshot& snapshot)
            -> const Shared::Schema::EntitySnapshot*
        {
            const auto* entities = snapshot.entities();
            if (!entities)
                return nullptr;
            for (flatbuffers::uoffset_t index = 0u;
                index < entities->size();
                ++index)
            {
                const Shared::Schema::EntitySnapshot* entity =
                    entities->Get(index);
                if (entity && entity->netId() == botNetId)
                    return entity;
            }
            return nullptr;
        };

        research.decisionDraft = ChampionAIResearch::MakeDecisionTraceV1();
        ai.debugDecisionTraceCount = 0u;
        ai.debugDecisionTraceHead = 0u;
        flatbuffers::DetachedBuffer baseline = BuildAt(kEvidenceTick);
        const Shared::Schema::Snapshot* baselineSnapshot = nullptr;
        if (!VerifyAndGet(baseline, baselineSnapshot))
            return Fail("AI snapshot baseline FlatBuffer verification failed");
        const Shared::Schema::EntitySnapshot* baselineBot =
            FindBotSnapshot(*baselineSnapshot);
        if (!baselineBot ||
            baselineBot->aiDebugCandidateTick() != 0u ||
            (baselineBot->aiDebugCandidateKinds() &&
                baselineBot->aiDebugCandidateKinds()->size() != 0u))
        {
            return Fail("AI snapshot baseline unexpectedly carried evidence");
        }
        outBaselineBytes = baseline.size();

        AiDecisionTraceV1 draft = ChampionAIResearch::MakeDecisionTraceV1();
        draft.tick = kEvidenceTick;
        draft.candidateCount = kAiDecisionCandidateCapacityV1;
        for (u8_t index = 0u;
            index < kAiDecisionCandidateCapacityV1;
            ++index)
        {
            AiCandidateEvidenceV1& candidate = draft.candidates[index];
            candidate.candidateKind = static_cast<u8_t>(index + 1u);
            candidate.flags = kAiCandidateLegalFlagV1;
            candidate.targetNetEntityId = botNetId;
            candidate.score = 0.2f * static_cast<f32_t>(index + 1u);
            candidate.contributionCount = 1u;
            candidate.contributions[0] =
                ChampionAIResearch::MakeFeatureContributionV1();
            candidate.contributions[0].featureId =
                static_cast<u16_t>(AiFeatureIdV1::UtilityScore);
            candidate.contributions[0].rawValue = candidate.score;
            candidate.contributions[0].weight = 1.f;
            candidate.contributions[0].contribution = candidate.score;
        }
        research.decisionDraft = draft;
        ChampionAIDecisionTraceEntry selected{};
        selected.tick = kEvidenceTick;
        selected.intent = eChampionAIIntent::FarmMinion;
        ai.debugDecisionTrace[0] = selected;
        ai.debugDecisionTraceHead = 1u;
        ai.debugDecisionTraceCount = 1u;

        flatbuffers::DetachedBuffer evidence = BuildAt(kEvidenceTick);
        const Shared::Schema::Snapshot* evidenceSnapshot = nullptr;
        if (!VerifyAndGet(evidence, evidenceSnapshot))
            return Fail("AI snapshot evidence FlatBuffer verification failed");
        const Shared::Schema::EntitySnapshot* evidenceBot =
            FindBotSnapshot(*evidenceSnapshot);
        if (!evidenceBot ||
            evidenceBot->aiDebugCandidateTick() != kEvidenceTick ||
            evidenceBot->aiDebugSelectionTick() != kEvidenceTick ||
            !evidenceBot->aiDebugCandidateKinds() ||
            !evidenceBot->aiDebugCandidateFlags() ||
            !evidenceBot->aiDebugCandidateScores() ||
            !evidenceBot->aiDebugCandidateTermCounts() ||
            !evidenceBot->aiDebugCandidateTermContributions() ||
            evidenceBot->aiDebugCandidateKinds()->size() !=
                kAiDecisionCandidateCapacityV1 ||
            evidenceBot->aiDebugCandidateTermContributions()->size() !=
                kAiDecisionCandidateCapacityV1 *
                    kAiFeatureContributionCapacityV1 ||
            (evidenceBot->aiDebugCandidateFlags()->Get(2u) &
                kAiCandidateSelectedFlagV1) == 0u)
        {
            return Fail("AI snapshot evidence vectors/selection mismatch");
        }
        for (u8_t candidateIndex = 0u;
            candidateIndex < kAiDecisionCandidateCapacityV1;
            ++candidateIndex)
        {
            f32_t sum = 0.f;
            const u8_t count = evidenceBot->aiDebugCandidateTermCounts()->Get(
                candidateIndex);
            for (u8_t term = 0u; term < count; ++term)
            {
                const u32_t flatIndex =
                    candidateIndex * kAiFeatureContributionCapacityV1 + term;
                sum += evidenceBot->aiDebugCandidateTermContributions()->Get(
                    flatIndex);
            }
            if (std::fabs(
                    sum - evidenceBot->aiDebugCandidateScores()->Get(
                        candidateIndex)) > 0.001f)
            {
                return Fail("AI snapshot contribution sum mismatch");
            }
        }
        outEvidenceBytes = evidence.size();
        if (outEvidenceBytes < outBaselineBytes ||
            outEvidenceBytes - outBaselineBytes > 6u * 1024u)
        {
            return Fail("AI snapshot evidence exceeded 6 KiB budget");
        }

        flatbuffers::DetachedBuffer stale = BuildAt(kEvidenceTick + 1u);
        const Shared::Schema::Snapshot* staleSnapshot = nullptr;
        if (!VerifyAndGet(stale, staleSnapshot))
            return Fail("AI snapshot stale FlatBuffer verification failed");
        const Shared::Schema::EntitySnapshot* staleBot =
            FindBotSnapshot(*staleSnapshot);
        if (!staleBot ||
            staleBot->aiDebugCandidateTick() != 0u ||
            (staleBot->aiDebugCandidateKinds() &&
                staleBot->aiDebugCandidateKinds()->size() != 0u))
        {
            return Fail("AI snapshot stale evidence was not omitted");
        }

        Restore();
        return true;
#endif
    }

    static void Tick(CGameRoom& room)
    {
        room.Tick();
    }

    static u64_t TickIndex(const CGameRoom& room)
    {
        return room.m_tickIndex;
    }

    static WorldMetrics CollectMetrics(CGameRoom& room)
    {
        WorldMetrics metrics{};
        metrics.entityCount = room.m_world.GetEntityCount();
        metrics.pendingExecCount = static_cast<u32_t>(room.m_pendingExecCommands.size());
        metrics.pendingReplayCount = static_cast<u32_t>(room.m_pendingReplayCommands.size());
        metrics.keyframeCount = static_cast<u32_t>(room.m_keyframes.size());
        for (const CGameRoom::RoomKeyframe& keyframe : room.m_keyframes)
            metrics.keyframeBytes += static_cast<u64_t>(keyframe.simBytes.size());

        if (room.m_pReplayRecorder)
        {
            metrics.replayRecordCount = room.m_pReplayRecorder->GetRecordCount();
            metrics.replaySnapshotCount = room.m_pReplayRecorder->GetSnapshotCount();
            metrics.replayEventCount = room.m_pReplayRecorder->GetEventCount();
            metrics.replayCommandCount = room.m_pReplayRecorder->GetCommandCount();
            metrics.replayFirstTick = room.m_pReplayRecorder->GetFirstTick();
            metrics.replayLastTick = room.m_pReplayRecorder->GetLastTick();
            metrics.replayPayloadBytes = room.m_pReplayRecorder->GetPayloadBytes();
            metrics.replaySpoolBytes = room.m_pReplayRecorder->GetSpoolBytes();
            metrics.bReplayHealthy = !room.m_pReplayRecorder->HasWriteFailure();
            if (!metrics.bReplayHealthy)
            {
                SetFirstError(metrics,
                    "replay spool failure: " +
                    room.m_pReplayRecorder->GetWriteError());
            }
        }

        room.m_world.ForEach<TransformComponent>(
            [&](EntityID entity, TransformComponent& transform)
            {
                if (!IsFinite(transform.GetPosition()) ||
                    !IsFinite(transform.GetRotation()) ||
                    !IsFinite(transform.GetScale()))
                {
                    metrics.bFinite = false;
                    SetFirstError(metrics,
                        "non-finite transform on entity " + std::to_string(entity));
                }
            });

        room.m_world.ForEach<HealthComponent>(
            [&](EntityID entity, HealthComponent& health)
            {
                if (!IsFinite(health.fCurrent) || !IsFinite(health.fMaximum) ||
                    health.fMaximum < 0.f || health.fCurrent < -0.01f ||
                    health.fCurrent > health.fMaximum + 0.01f)
                {
                    metrics.bFinite = false;
                    SetFirstError(metrics,
                        "invalid health on entity " + std::to_string(entity));
                }
            });

        room.m_world.ForEach<ChampionComponent>(
            [&](EntityID entity, ChampionComponent& champion)
            {
                ++metrics.championCount;
                if (!IsFinite(champion.hp) || !IsFinite(champion.maxHp) ||
                    !IsFinite(champion.mana) || !IsFinite(champion.maxMana) ||
                    !IsFinite(champion.moveSpeed) || champion.maxHp < 0.f ||
                    champion.hp < -0.01f || champion.hp > champion.maxHp + 0.01f ||
                    champion.maxMana < 0.f || champion.mana < -0.01f ||
                    champion.mana > champion.maxMana + 0.01f)
                {
                    metrics.bFinite = false;
                    SetFirstError(metrics,
                        "invalid champion resource on entity " + std::to_string(entity));
                }
                for (const f32_t cooldown : champion.cooldowns)
                {
                    if (!IsFinite(cooldown))
                    {
                        metrics.bFinite = false;
                        SetFirstError(metrics,
                            "non-finite champion cooldown on entity " +
                            std::to_string(entity));
                    }
                }
                if (room.m_world.HasComponent<HealthComponent>(entity) &&
                    room.m_world.GetComponent<HealthComponent>(entity).bIsDead)
                {
                    ++metrics.deadChampionCount;
                }
            });

        room.m_world.ForEach<ChampionAIComponent>(
            [&](EntityID entity, ChampionAIComponent& ai)
            {
                ++metrics.botCount;
                metrics.commandSequenceSum += static_cast<u64_t>(ai.nextCommandSequence);
                const bool_t bTimersFinite =
                    IsFinite(ai.decisionTimer) && IsFinite(ai.decisionInterval) &&
                    IsFinite(ai.intentHoldTimer) && IsFinite(ai.intentHoldDuration) &&
                    IsFinite(ai.fPostComboBATimer) && IsFinite(ai.fDiveExtraBATimer) &&
                    IsFinite(ai.fSkillCastCooldownTimer);
                if (!bTimersFinite || !IsFinite(ai.laneGoal) ||
                    !IsFinite(ai.safeAnchor) || !IsFinite(ai.retreatGoal) ||
                    !IsFinite(ai.lastSeenEnemyChampionPos))
                {
                    metrics.bFinite = false;
                    SetFirstError(metrics,
                        "non-finite AI state on entity " + std::to_string(entity));
                }
            });

        room.m_world.ForEach<MinionComponent>(
            [&](EntityID, MinionComponent& minion)
            {
                ++metrics.minionCount;
                if (!IsFinite(minion.hp) || !IsFinite(minion.maxHp))
                    metrics.bFinite = false;
            });
        room.m_world.ForEach<StructureComponent>(
            [&](EntityID, StructureComponent& structure)
            {
                ++metrics.structureCount;
                if (!IsFinite(structure.hp) || !IsFinite(structure.maxHp))
                    metrics.bFinite = false;
            });
        room.m_world.ForEach<StructureProjectileComponent>(
            [&](EntityID, StructureProjectileComponent&) { ++metrics.projectileCount; });
        room.m_world.ForEach<SkillProjectileComponent>(
            [&](EntityID, SkillProjectileComponent&) { ++metrics.projectileCount; });
        room.m_world.ForEach<TowerAggroNotifyComponent>(
            [&](EntityID, TowerAggroNotifyComponent&)
            {
                ++metrics.aggroNotificationCount;
            });

        const auto& entitySlots = room.m_world.GetEntityManager().RawSlots();
        for (EntityID entity = 1u; entity < entitySlots.size(); ++entity)
        {
            if (!room.m_world.IsAlive(entity))
                continue;

            bool_t bHasComponent = false;
            room.m_world.ForEachStoreBase(
                [&](const std::type_index&, const IComponentStoreBase& store)
                {
                    if (!bHasComponent && store.Has(entity))
                        bHasComponent = true;
                });
            if (!bHasComponent)
                ++metrics.zeroComponentEntityCount;
        }

        std::unordered_set<NetEntityId> netIds;
        std::unordered_set<EntityID> entities;
        room.m_entityMap.ForEachBinding(
            [&](NetEntityId netId, EntityID entity)
            {
                ++metrics.netBindingCount;
                if (netId == NULL_NET_ENTITY || entity == NULL_ENTITY ||
                    !room.m_world.IsAlive(entity) ||
                    room.m_entityMap.ToNet(entity) != netId ||
                    !netIds.emplace(netId).second ||
                    !entities.emplace(entity).second)
                {
                    metrics.bEntityMapConsistent = false;
                    SetFirstError(metrics, "entity map binding invariant failed");
                }
            });
        room.m_world.ForEach<NetEntityIdComponent>(
            [&](EntityID entity, NetEntityIdComponent& net)
            {
                if (net.netId == NULL_NET_ENTITY ||
                    room.m_entityMap.FromNet(net.netId) != entity)
                {
                    metrics.bEntityMapConsistent = false;
                    SetFirstError(metrics,
                        "NetEntityIdComponent binding invariant failed");
                }
            });

        if (!metrics.bFinite && metrics.error.empty())
            metrics.error = "non-finite world state";
        return metrics;
    }

    static wstring_t ReplayPath(const CGameRoom& room)
    {
        return room.m_pReplayRecorder
            ? room.m_pReplayRecorder->MakeDefaultPath()
            : wstring_t{};
    }

    static void ObserveLifecycle(
        CGameRoom& room,
        u64_t tick,
        LifecycleTracker& tracker)
    {
        tracker.inactiveBotCount = 0u;
        tracker.overdueRespawnCount = 0u;
        room.m_world.ForEach<ChampionAIComponent>(
            [&](EntityID entity, ChampionAIComponent& ai)
            {
                const auto [sequenceIt, sequenceInserted] =
                    tracker.previousCommandSequenceByEntity.emplace(
                        entity,
                        ai.nextCommandSequence);
                if (!sequenceInserted &&
                    sequenceIt->second != ai.nextCommandSequence)
                {
                    sequenceIt->second = ai.nextCommandSequence;
                    tracker.lastCommandActivityTickByEntity[entity] = tick;
                    tracker.commandActiveEntities.emplace(entity);
                }

                const u64_t lastActivityTick =
                    tracker.lastCommandActivityTickByEntity[entity];
                const u64_t inactivity = tick - lastActivityTick;
                tracker.maxCommandInactivityTicks = (std::max)(
                    tracker.maxCommandInactivityTicks,
                    inactivity);

                if (!room.m_world.HasComponent<RespawnComponent>(entity))
                {
                    if (tick > kBotInactivityLimitTicks &&
                        inactivity > kBotInactivityLimitTicks)
                    {
                        ++tracker.inactiveBotCount;
                    }
                    return;
                }

                const RespawnComponent& respawn =
                    room.m_world.GetComponent<RespawnComponent>(entity);
                const bool_t pending = respawn.bPending;
                const auto [it, inserted] =
                    tracker.pendingByEntity.emplace(entity, pending);
                if (!inserted && it->second != pending)
                {
                    if (pending)
                    {
                        ++tracker.deathTransitions;
                        tracker.deathTickByEntity[entity] = tick;
                    }
                    else
                    {
                        ++tracker.respawnTransitions;
                        tracker.deathTickByEntity.erase(entity);
                        const bool_t bAIReset =
                            ai.state == eChampionAIState::MoveToOuterTurret &&
                            ai.lastAction == eChampionAIAction::MoveToSafeAnchor &&
                            ai.intent == eChampionAIIntent::FarmMinion &&
                            ai.lockedChampion == NULL_ENTITY &&
                            ai.targetMinion == NULL_ENTITY &&
                            ai.targetStructure == NULL_ENTITY &&
                            ai.alliedWave == NULL_ENTITY &&
                            ai.comboTarget == NULL_ENTITY &&
                            ai.lowHpEnemyChampion == NULL_ENTITY &&
                            ai.diveTarget == NULL_ENTITY &&
                            ai.lastSeenEnemyChampion == NULL_ENTITY &&
                            ai.lastSeenEnemyChampionTick == 0u &&
                            ai.divePhase == eChampionAIDivePhase::None &&
                            ai.comboStep == 0u &&
                            ai.diveExtraBACount == 0u &&
                            ai.intentHoldTimer == 0.f &&
                            ai.fPostComboBATimer == 0.f &&
                            ai.fDiveExtraBATimer == 0.f &&
                            ai.fSkillCastCooldownTimer == 0.f &&
                            !ai.bPostComboBAAllowed &&
                            !ai.bDebugForceAction;
                        if (!bAIReset)
                            ++tracker.respawnAIResetFailures;

                        if (room.m_world.HasComponent<ChampionComponent>(entity))
                        {
                            const ChampionComponent& champion =
                                room.m_world.GetComponent<ChampionComponent>(entity);
                            if (champion.mana + 0.01f < champion.maxMana)
                                ++tracker.respawnManaFailures;
                        }
                    }
                    it->second = pending;
                }

                if (pending)
                {
                    const auto deathIt = tracker.deathTickByEntity.find(entity);
                    const u64_t respawnLimitTicks = static_cast<u64_t>(
                        std::ceil(respawn.respawnDelay * 30.f)) +
                        kRespawnGraceTicks;
                    if (deathIt != tracker.deathTickByEntity.end() &&
                        tick - deathIt->second > respawnLimitTicks)
                    {
                        ++tracker.overdueRespawnCount;
                    }
                    return;
                }

                if (tick > kBotInactivityLimitTicks &&
                    inactivity > kBotInactivityLimitTicks)
                {
                    ++tracker.inactiveBotCount;
                }
            });
    }

    static void ObserveMinionMotion(
        CGameRoom& room,
        u64_t tick,
        MinionMotionTracker& tracker)
    {
        tracker.stalledLaneMinionCount = 0u;
        room.m_world.ForEach<MinionStateComponent>(
            [&](EntityID entity, MinionStateComponent& state)
            {
                if (!room.m_world.HasComponent<TransformComponent>(entity))
                    return;

                const TransformComponent& transform =
                    room.m_world.GetComponent<TransformComponent>(entity);
                const Vec3 position = transform.GetPosition();
                MinionMotionSample& sample = tracker.samples[entity];
                const EntityHandle handle = room.m_world.GetEntityHandle(entity);
                if (!sample.bInitialized || sample.handle != handle)
                {
                    sample = MinionMotionSample{};
                    sample.handle = handle;
                    sample.previousPosition = position;
                    sample.lastProgressTick = tick;
                    sample.bInitialized = true;
                }

                const bool_t bHasPathGoal =
                    state.PathCount > 0u && state.PathIndex < state.PathCount;
                const u8_t waypointLane = ResolveServerWaypointLane(
                    state.team,
                    state.lane);
                const u32_t waypointCount = room.GetServerMinionWaypointCount(
                    state.team,
                    waypointLane);
                const bool_t bTrackLaneMove =
                    state.current == MinionStateComponent::LaneMove &&
                    (bHasPathGoal || state.currentWaypoint < waypointCount);
                if (!bTrackLaneMove)
                {
                    sample.previousPosition = position;
                    sample.lastProgressTick = tick;
                    sample.bTrackingLaneMove = false;
                    return;
                }

                const u32_t teamIndex = static_cast<u32_t>(state.team);
                if (teamIndex < 2u && state.lane < 3u)
                    tracker.observedLaneSlots[teamIndex * 3u + state.lane] = true;

                const Vec3 target = bHasPathGoal
                    ? state.PathWaypoints[state.PathIndex]
                    : room.GetServerMinionWaypoint(
                        state.team,
                        waypointLane,
                        state.currentWaypoint);
                const u32_t activeGoalIndex = bHasPathGoal
                    ? static_cast<u32_t>(state.PathIndex)
                    : state.currentWaypoint;
                const f32_t targetDistanceSq =
                    WintersMath::DistanceSqXZ(position, target);

                if (sample.bTrackingLaneMove)
                {
                    const Vec3 displacement{
                        position.x - sample.previousPosition.x,
                        0.f,
                        position.z - sample.previousPosition.z };
                    const f32_t moveLengthSq =
                        displacement.x * displacement.x + displacement.z * displacement.z;
                    if (moveLengthSq > 0.0001f)
                    {
                        const f32_t yaw = transform.GetRotation().y;
                        const f32_t facingDotMove =
                            (-std::sin(yaw) * displacement.x -
                                std::cos(yaw) * displacement.z) /
                            std::sqrt(moveLengthSq);
                        if (facingDotMove < -0.05f)
                        {
                            ++tracker.opposedYawCount;
                            if (tracker.firstOpposedYawEntity == NULL_ENTITY)
                                tracker.firstOpposedYawEntity = entity;
                        }
                    }
                }

                const bool_t bGoalChanged =
                    !sample.bTrackingLaneMove ||
                    sample.bTrackingPathGoal != bHasPathGoal ||
                    sample.activeGoalIndex != activeGoalIndex ||
                    WintersMath::DistanceSqXZ(sample.activeGoal, target) > 0.01f;
                if (bGoalChanged)
                {
                    sample.activeGoal = target;
                    sample.activeGoalIndex = activeGoalIndex;
                    sample.bestWaypointDistanceSq = targetDistanceSq;
                    sample.lastProgressTick = tick;
                }
                else if (targetDistanceSq + kMinionProgressSlackSq <
                    sample.bestWaypointDistanceSq)
                {
                    sample.bestWaypointDistanceSq = targetDistanceSq;
                    sample.lastProgressTick = tick;
                }

                const u64_t stallTicks = tick - sample.lastProgressTick;
                tracker.maxLaneStallTicks =
                    (std::max)(tracker.maxLaneStallTicks, stallTicks);
                if (stallTicks > kMinionLaneStallLimitTicks)
                    ++tracker.stalledLaneMinionCount;

                sample.previousPosition = position;
                sample.bTrackingLaneMove = true;
                sample.bTrackingPathGoal = bHasPathGoal;
            });
    }

    static void PrintInactiveBots(
        CGameRoom& room,
        u64_t tick,
        const LifecycleTracker& tracker)
    {
        room.m_world.ForEach<ChampionAIComponent>(
            [&](EntityID entity, ChampionAIComponent& ai)
            {
                const auto activityIt =
                    tracker.lastCommandActivityTickByEntity.find(entity);
                const u64_t lastActivityTick = activityIt !=
                    tracker.lastCommandActivityTickByEntity.end()
                    ? activityIt->second
                    : 0u;
                const u64_t inactivity = tick - lastActivityTick;
                const bool_t bRespawnPending =
                    room.m_world.HasComponent<RespawnComponent>(entity) &&
                    room.m_world.GetComponent<RespawnComponent>(entity).bPending;
                if (inactivity <= kBotInactivityLimitTicks || bRespawnPending)
                    return;

                f32_t health = -1.f;
                f32_t maxHealth = -1.f;
                bool_t bHealthDead = false;
                if (room.m_world.HasComponent<HealthComponent>(entity))
                {
                    const HealthComponent& component =
                        room.m_world.GetComponent<HealthComponent>(entity);
                    health = component.fCurrent;
                    maxHealth = component.fMaximum;
                    bHealthDead = component.bIsDead;
                }

                Vec3 position{};
                if (room.m_world.HasComponent<TransformComponent>(entity))
                {
                    position = room.m_world
                        .GetComponent<TransformComponent>(entity)
                        .m_LocalPosition;
                }

                bool_t bHasMoveTarget = false;
                Vec3 moveTarget{};
                Vec3 activeMoveTarget{};
                u16_t pathIndex = 0u;
                u16_t pathCount = 0u;
                u16_t blockedMoveTicks = 0u;
                f32_t bestMoveDistance = -1.f;
                if (room.m_world.HasComponent<MoveTargetComponent>(entity))
                {
                    const MoveTargetComponent& move =
                        room.m_world.GetComponent<MoveTargetComponent>(entity);
                    bHasMoveTarget = move.bHasTarget;
                    moveTarget = move.target;
                    activeMoveTarget = move.target;
                    pathIndex = move.pathIndex;
                    pathCount = move.pathCount;
                    blockedMoveTicks = move.blockedMoveTicks;
                    bestMoveDistance = move.bestMoveDistance;
                    if (move.pathIndex < move.pathCount)
                        activeMoveTarget = move.pathWaypoints[move.pathIndex];
                }

                std::cout << "INACTIVE_BOT"
                    << " tick=" << tick
                    << " entity=" << entity
                    << " champion=" << static_cast<u32_t>(ai.champion)
                    << " team=" << static_cast<u32_t>(ai.team)
                    << " lane=" << static_cast<u32_t>(ai.activeLane)
                    << " state=" << static_cast<u32_t>(ai.state)
                    << " intent=" << static_cast<u32_t>(ai.intent)
                    << " action=" << static_cast<u32_t>(ai.lastAction)
                    << " seq=" << ai.nextCommandSequence
                    << " last_activity_tick=" << lastActivityTick
                    << " inactivity_ticks=" << inactivity
                    << " health=" << health
                    << " max_health=" << maxHealth
                    << " health_dead=" << (bHealthDead ? 1 : 0)
                    << " respawn_pending=" << (bRespawnPending ? 1 : 0)
                    << " pos_x=" << position.x
                    << " pos_z=" << position.z
                    << " retreat_x=" << ai.retreatGoal.x
                    << " retreat_z=" << ai.retreatGoal.z
                    << " retreat_distance=" << std::sqrt(
                        WintersMath::DistanceSqXZ(position, ai.retreatGoal))
                    << " has_move_target=" << (bHasMoveTarget ? 1 : 0)
                    << " move_target_x=" << moveTarget.x
                    << " move_target_z=" << moveTarget.z
                    << " move_distance=" << std::sqrt(
                        WintersMath::DistanceSqXZ(position, moveTarget))
                    << " active_move_target_x=" << activeMoveTarget.x
                    << " active_move_target_z=" << activeMoveTarget.z
                    << " active_move_distance=" << std::sqrt(
                        WintersMath::DistanceSqXZ(position, activeMoveTarget))
                    << " path_index=" << pathIndex
                    << " path_count=" << pathCount
                    << " blocked_move_ticks=" << blockedMoveTicks
                    << " best_move_distance=" << bestMoveDistance
                    << " locked_champion=" << ai.lockedChampion
                    << " target_minion=" << ai.targetMinion
                    << " target_structure=" << ai.targetStructure
                    << " allied_wave=" << ai.alliedWave
                    << " combo_target=" << ai.comboTarget
                    << " dive_target=" << ai.diveTarget
                    << " last_seen_enemy=" << ai.lastSeenEnemyChampion
                    << " last_seen_tick=" << ai.lastSeenEnemyChampionTick
                    << " decision_timer=" << ai.decisionTimer
                    << " last_command_kind="
                    << static_cast<u32_t>(ai.debugLastCommandKind)
                    << " last_command_slot="
                    << static_cast<u32_t>(ai.debugLastCommandSlot)
                    << " block_reason="
                    << static_cast<u32_t>(ai.debugLastBlockReason)
                    << '\n';
            });
        std::cout.flush();
    }

    static bool_t BuildFinalState(
        CGameRoom& room,
        std::vector<u8_t>& outBytes,
        std::string& outError)
    {
        outError.clear();
        if (!SimCheckpoint::SaveWorldKeyframe(
            room.m_world,
            room.m_rng,
            room.m_entityMap,
            room.m_tickIndex,
            outBytes))
        {
            outError = "failed to save final world keyframe";
            return false;
        }
        return true;
    }

private:
#if defined(_DEBUG)
    static bool_t PreparePracticeControlMatch(
        CGameRoom& room,
        u64_t seed,
        u32_t sessionId,
        std::string& outError)
    {
        static constexpr std::array<eChampion, kGameRosterSlotCount> kRoster =
        {
            eChampion::IRELIA,
            eChampion::YASUO,
            eChampion::ZED,
            eChampion::ASHE,
            eChampion::ANNIE,
            eChampion::RIVEN,
            eChampion::SYLAS,
            eChampion::VIEGO,
            eChampion::YONE,
            eChampion::JAX,
        };
        static constexpr std::array<u8_t, 5u> kTeamLanes =
        {
            static_cast<u8_t>(Winters::Map::eLane::Top),
            static_cast<u8_t>(Winters::Map::eLane::Mid),
            static_cast<u8_t>(Winters::Map::eLane::Bot),
            static_cast<u8_t>(Winters::Map::eLane::Bot),
            static_cast<u8_t>(Winters::Map::eLane::Mid),
        };

        outError.clear();
        if (!room.m_pLobbyAuthority)
        {
            outError = "practice control probe lobby authority is missing";
            return false;
        }

        room.m_rng.SetState(seed);
        CLobbyAuthority& authority = *room.m_pLobbyAuthority;
        authority.InitializeSlots();
        authority.m_hostSessionId = sessionId;
        authority.m_phase = eRoomPhase::InGame;
        authority.m_sessionToSlot.clear();
        authority.m_sessionToSlot[sessionId] = 0u;

        LobbySlotState* pSlots = authority.GetSlots();
        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            LobbySlotState slot{};
            slot.slotId = static_cast<u8_t>(i);
            slot.team = i < 5u ? 0u : 1u;
            slot.bHuman = i == 0u;
            slot.bBot = i != 0u;
            slot.sessionId = i == 0u ? sessionId : 0u;
            slot.champion = kRoster[i];
            slot.botDifficulty = 2u;
            slot.botLane = kTeamLanes[i % 5u];
            slot.bReady = true;
            slot.bLocked = true;
            pSlots[i] = slot;
        }

        room.m_sessionIds = { sessionId };
        room.SpawnChampionsFromLobby();
        room.SpawnServerGameplayObjects();
        room.m_serverMinionWaves.ScheduleFirstWave(room.m_tickIndex, {});
        room.m_bPracticeModeEnabled = true;
        return ValidateStrictPracticeRoster(room, sessionId, outError);
    }

    static bool_t ExecuteAcceptedPracticeControlCommand(
        CGameRoom& room,
        u32_t sessionId,
        const GameCommandWire& wire,
        std::string& outError)
    {
        if (!room.m_pReplayRecorder)
        {
            outError = "practice control probe replay recorder is missing";
            return false;
        }

        const u64_t tickBefore = room.m_tickIndex;
        const u64_t toolRevisionBefore = room.m_toolRevision;
        const u32_t replayCommandsBefore =
            room.m_pReplayRecorder->GetCommandCount();
        room.EnqueueCommand(
            sessionId,
            wire,
            room.m_tickIndex,
            0u,
            0u);
        room.Tick();

        const auto ack = room.m_lastSimCommandSeqBySession.find(sessionId);
        if (room.m_tickIndex != tickBefore + 1u ||
            ack == room.m_lastSimCommandSeqBySession.end() ||
            ack->second != wire.sequenceNum ||
            room.m_toolRevision != toolRevisionBefore + 1u ||
            room.m_pReplayRecorder->GetCommandCount() !=
                replayCommandsBefore + 1u ||
            !room.m_pendingExecCommands.empty() ||
            !room.m_pendingReplayCommands.empty() ||
            room.m_PendingPracticeControlChange.eKind !=
                CGameRoom::PracticeControlChangeKind::None)
        {
            std::ostringstream stream;
            stream << "practice control command was not ACKed/journaled exactly once"
                << " op=" << static_cast<u32_t>(wire.practiceOperation)
                << " seq=" << wire.sequenceNum;
            outError = stream.str();
            return false;
        }
        return true;
    }

    static bool_t ValidateStrictPracticeRoster(
        CGameRoom& room,
        u32_t sessionId,
        std::string& outError)
    {
        outError.clear();
        if (!room.m_pLobbyAuthority ||
            room.m_pLobbyAuthority->GetPhase() != eRoomPhase::InGame)
        {
            outError = "room is not in-game";
            return false;
        }

        const LobbySlotState* pSlots =
            room.m_pLobbyAuthority->GetSlots();
        if (!pSlots ||
            room.m_pLobbyAuthority->GetSlotCount() != kGameRosterSlotCount)
        {
            outError = "lobby roster storage is invalid";
            return false;
        }

        u32_t humanCount = 0u;
        u32_t botCount = 0u;
        u32_t blueCount = 0u;
        u32_t redCount = 0u;
        EntityID humanEntity = NULL_ENTITY;
        std::unordered_set<NetEntityId> netIds;
        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            const LobbySlotState& slot = pSlots[i];
            const EntityID entity = room.m_entityMap.FromNet(slot.netId);
            if (slot.slotId != static_cast<u8_t>(i) ||
                slot.team != (i < 5u ? 0u : 1u) ||
                slot.bHuman == slot.bBot || slot.bDummy ||
                slot.champion == eChampion::NONE ||
                slot.champion == eChampion::END ||
                slot.netId == NULL_NET_ENTITY ||
                !netIds.insert(slot.netId).second ||
                entity == NULL_ENTITY || !room.m_world.IsAlive(entity) ||
                room.m_entityMap.ToNet(entity) != slot.netId ||
                !room.m_world.HasComponent<NetEntityIdComponent>(entity) ||
                room.m_world.GetComponent<NetEntityIdComponent>(entity).netId !=
                    slot.netId ||
                !room.m_world.HasComponent<ChampionComponent>(entity))
            {
                outError = "slot/entity/NetId invariant failed at slot " +
                    std::to_string(i);
                return false;
            }

            const ChampionComponent& champion =
                room.m_world.GetComponent<ChampionComponent>(entity);
            if (champion.id != slot.champion ||
                champion.team != static_cast<eTeam>(slot.team) ||
                (slot.bHuman &&
                    (slot.sessionId != sessionId ||
                        room.m_world.HasComponent<ChampionAIComponent>(entity))) ||
                (slot.bBot &&
                    (slot.sessionId != 0u ||
                        !room.m_world.HasComponent<ChampionAIComponent>(entity))))
            {
                outError = "slot role/component invariant failed at slot " +
                    std::to_string(i);
                return false;
            }

            if (slot.bHuman)
                humanEntity = entity;
            humanCount += slot.bHuman ? 1u : 0u;
            botCount += slot.bBot ? 1u : 0u;
            blueCount += slot.team == 0u ? 1u : 0u;
            redCount += slot.team == 1u ? 1u : 0u;
        }

        u32_t championCount = 0u;
        room.m_world.ForEach<ChampionComponent>(
            [&](EntityID, ChampionComponent&)
            {
                ++championCount;
            });
        EntityID boundEntity = NULL_ENTITY;
        if (championCount != kGameRosterSlotCount ||
            humanCount != 1u || botCount != 9u ||
            blueCount != 5u || redCount != 5u ||
            humanEntity == NULL_ENTITY ||
            !room.m_sessionBinding.TryGetAlive(
                sessionId,
                room.m_world,
                boundEntity) ||
            boundEntity != humanEntity)
        {
            outError = "strict 10/human1/bot9/blue5/red5 invariant failed";
            return false;
        }
        return true;
    }
#endif
};

int main(int argc, char** argv)
{
    Options options{};
    if (!TryParseOptions(argc, argv, options))
    {
        std::cerr << "usage: GameRoomBotMatchSoak.exe "
            << "[ticks] [seed] [roomId] [heartbeatTicks] [privateLimitMiB]\n";
        return 2;
    }

    std::cout << "START ticks=" << options.tickCount
        << " seed=" << options.seed
        << " room=" << options.roomId
        << " heartbeat_ticks=" << options.heartbeatTicks
        << " private_limit_mib=" << BytesToMiB(options.privateByteLimit)
        << '\n';
    std::cout.flush();

    const ProcessMemorySample processStart = QueryProcessMemory();
    std::string error;
#if defined(_DEBUG)
    auto controlProbeRoom = CGameRoom::Create(
        options.roomId ^ 0x80000000u);
    if (!controlProbeRoom)
    {
        std::cerr << "RESULT status=FAIL reason=control_probe_room_create_failed\n";
        return 1;
    }

    PracticeControlProbeEvidence controlEvidence{};
    if (!CGameRoomIntegrationProbeAccess::RunPracticeControlProbe(
            *controlProbeRoom,
            options.seed,
            controlEvidence,
            error))
    {
        controlProbeRoom->Stop();
        std::cerr << "RESULT status=FAIL reason=control_probe_failed detail=\""
            << error << "\"\n";
        return 1;
    }
    std::cout << "CONTROL_PROBE status=PASS"
        << " source_net=" << controlEvidence.sourceNetId
        << " taken_net=" << controlEvidence.takenNetId
        << " replacement_net=" << controlEvidence.replacementNetId
        << " strict_champions=10 human=1 bots=9 blue=5 red=5"
        << " level=6 gold=10000 full_hp_mp=1"
        << " tool_revision=" << controlEvidence.toolRevision
        << " replay_commands=" << controlEvidence.replayCommandCount
        << '\n';
    std::cout.flush();
    controlProbeRoom->Stop();
    controlProbeRoom.reset();

    auto teamPingProbeRoom = CGameRoom::Create(
        options.roomId ^ 0x40000000u);
    TeamPingProbeEvidence teamPingEvidence{};
    if (!teamPingProbeRoom ||
        !CGameRoomIntegrationProbeAccess::RunTeamPingCommandProbe(
            *teamPingProbeRoom,
            options.seed,
            teamPingEvidence,
            error))
    {
        if (teamPingProbeRoom)
            teamPingProbeRoom->Stop();
        std::cerr << "RESULT status=FAIL reason=team_ping_probe_failed detail=\""
            << error << "\"\n";
        return 1;
    }
    std::cout << "TEAM_PING_PROBE status=PASS"
        << " blue_bots=" << teamPingEvidence.blueBotCount
        << " replay_commands=" << teamPingEvidence.replayCommandCount
        << " checkpoint_bytes=" << teamPingEvidence.checkpointBytes
        << " lane_preset=1 assist_next_tick_move=1"
        << " danger_equivalent_move=1 expired=1\n";
    teamPingProbeRoom->Stop();
    teamPingProbeRoom.reset();
#endif

    StructureNavigationProbeEvidence structureNavEvidence{};
    auto structureNavProbeRoom = CGameRoom::Create(
        options.roomId ^ 0x20000000u);
    if (!structureNavProbeRoom ||
        !CGameRoomIntegrationProbeAccess::PrepareBotMatch(
            *structureNavProbeRoom,
            options.seed,
            error) ||
        !CGameRoomIntegrationProbeAccess::RunStructureNavigationRefreshProbe(
            *structureNavProbeRoom,
            structureNavEvidence,
            error) ||
        !CGameRoomIntegrationProbeAccess::RunMinionCombatExitWaypointProbe(
            *structureNavProbeRoom,
            error))
    {
        if (structureNavProbeRoom)
            structureNavProbeRoom->Stop();
        std::cerr
            << "RESULT status=FAIL reason=structure_nav_probe_failed detail=\""
            << error << "\"\n";
        return 1;
    }
    std::cout << "STRUCTURE_NAV_PROBE status=PASS"
        << " live_blocked=1 dead_released=1 path_released=1 lane_released=1"
        << " champion_path_reached=1 minion_path_reached=1"
        << " refresh_tick_us=" << structureNavEvidence.refreshTickUs
        << " first_champion_path_query_us="
        << structureNavEvidence.firstChampionPathQueryUs
        << " first_minion_path_query_us="
        << structureNavEvidence.firstMinionPathQueryUs
        << " noop_tick_us=" << structureNavEvidence.noopTickUs
        << " derived_rebuild_calls=" << structureNavEvidence.derivedRebuildCalls
        << " refresh_work_calls=" << structureNavEvidence.refreshWorkCalls
        << " second_refresh_work_calls="
        << structureNavEvidence.secondRefreshWorkCalls
        << " minion_combat_exit=1\n";
    std::ofstream navJson("structure_nav_probe.json", std::ios::trunc);
    navJson << "{\n"
        << "  \"scope\": \"ServerStructureNav::AuthoritativeDeathRefreshTick\",\n"
        << "  \"refreshTickUs\": " << structureNavEvidence.refreshTickUs << ",\n"
        << "  \"firstChampionPathQueryUs\": "
        << structureNavEvidence.firstChampionPathQueryUs << ",\n"
        << "  \"firstMinionPathQueryUs\": "
        << structureNavEvidence.firstMinionPathQueryUs << ",\n"
        << "  \"noopTickUs\": " << structureNavEvidence.noopTickUs << ",\n"
        << "  \"derivedRebuildCalls\": "
        << structureNavEvidence.derivedRebuildCalls << ",\n"
        << "  \"refreshWorkCalls\": "
        << structureNavEvidence.refreshWorkCalls << ",\n"
        << "  \"secondRefreshWorkCalls\": "
        << structureNavEvidence.secondRefreshWorkCalls << "\n"
        << "}\n";
    if (!navJson.good())
    {
        std::cerr
            << "RESULT status=FAIL reason=structure_nav_json_write_failed\n";
        return 1;
    }
    structureNavProbeRoom->Stop();
    structureNavProbeRoom.reset();

    auto room = CGameRoom::Create(options.roomId);
    if (!room)
    {
        std::cerr << "RESULT status=FAIL reason=room_create_failed\n";
        return 1;
    }

    if (!CGameRoomIntegrationProbeAccess::PrepareBotMatch(*room, options.seed, error))
    {
        std::cerr << "RESULT status=FAIL reason=prepare_failed detail=\""
            << error << "\"\n";
        return 1;
    }

#if defined(_DEBUG)
    std::size_t aiSnapshotBaselineBytes = 0u;
    std::size_t aiSnapshotEvidenceBytes = 0u;
    if (!CGameRoomIntegrationProbeAccess::RunAIDebugSnapshotEvidenceProbe(
            *room,
            error,
            aiSnapshotBaselineBytes,
            aiSnapshotEvidenceBytes))
    {
        room->Stop();
        std::cerr << "RESULT status=FAIL reason=ai_snapshot_probe_failed detail=\""
            << error << "\"\n";
        return 1;
    }
    std::cout << "AI_SNAPSHOT_PROBE status=PASS"
        << " baseline_bytes=" << aiSnapshotBaselineBytes
        << " evidence_bytes=" << aiSnapshotEvidenceBytes
        << " delta_bytes="
        << (aiSnapshotEvidenceBytes - aiSnapshotBaselineBytes)
        << " stale_omitted=1 selected_tick=1 contribution_sum=1\n";
    std::cout.flush();
#endif

    const ProcessMemorySample afterPrepare = QueryProcessMemory();
    const WorldMetrics initialMetrics =
        CGameRoomIntegrationProbeAccess::CollectMetrics(*room);
    std::cout << std::fixed << std::setprecision(3)
        << "PREPARED entities=" << initialMetrics.entityCount
        << " champions=" << initialMetrics.championCount
        << " bots=" << initialMetrics.botCount
        << " structures=" << initialMetrics.structureCount
        << " rss_mib=" << BytesToMiB(afterPrepare.workingSetBytes)
        << " private_mib=" << BytesToMiB(afterPrepare.privateBytes)
        << '\n';
    std::cout.flush();

    std::vector<f64_t> tickMicros;
    tickMicros.reserve(options.tickCount);
    LifecycleTracker lifecycle{};
    MinionMotionTracker minionMotion{};
    CGameRoomIntegrationProbeAccess::ObserveLifecycle(*room, 0u, lifecycle);
    CGameRoomIntegrationProbeAccess::ObserveMinionMotion(*room, 0u, minionMotion);

    WorldMetrics lastMetrics = initialMetrics;
    ProcessMemorySample lastMemory = afterPrepare;
    u64_t peakWorkingSetBytes = lastMemory.workingSetBytes;
    u64_t peakPrivateBytes = lastMemory.privateBytes;
    u32_t peakEntityCount = lastMetrics.entityCount;
    u32_t peakMinionCount = lastMetrics.minionCount;
    u32_t peakProjectileCount = lastMetrics.projectileCount;
    u32_t deadlineMissCount = 0u;
    std::string failure;

    using Clock = std::chrono::steady_clock;
    const auto runStart = Clock::now();
    for (u32_t iteration = 0u; iteration < options.tickCount; ++iteration)
    {
        const auto tickStart = Clock::now();
        CGameRoomIntegrationProbeAccess::Tick(*room);
        const auto tickEnd = Clock::now();
        const f64_t elapsedMicros =
            std::chrono::duration<f64_t, std::micro>(tickEnd - tickStart).count();
        tickMicros.push_back(elapsedMicros);
        if (elapsedMicros > 33'333.333)
            ++deadlineMissCount;

        const u64_t tick = CGameRoomIntegrationProbeAccess::TickIndex(*room);
        if (tick != static_cast<u64_t>(iteration) + 1u)
        {
            failure = "tick index did not advance exactly once";
            break;
        }
        CGameRoomIntegrationProbeAccess::ObserveLifecycle(*room, tick, lifecycle);
        CGameRoomIntegrationProbeAccess::ObserveMinionMotion(*room, tick, minionMotion);
        if (minionMotion.opposedYawCount != 0u)
        {
            failure = "lane minion faced opposite its applied movement";
            break;
        }
        if (minionMotion.stalledLaneMinionCount != 0u)
        {
            failure = "lane minion made no waypoint progress for over 6 seconds";
            break;
        }

        const bool_t bSample = (tick % 30u) == 0u || tick == options.tickCount;
        const bool_t bHeartbeat =
            (tick % options.heartbeatTicks) == 0u || tick == options.tickCount;
        if (bSample)
        {
            lastMetrics = CGameRoomIntegrationProbeAccess::CollectMetrics(*room);
            peakEntityCount = (std::max)(peakEntityCount, lastMetrics.entityCount);
            peakMinionCount = (std::max)(peakMinionCount, lastMetrics.minionCount);
            peakProjectileCount =
                (std::max)(peakProjectileCount, lastMetrics.projectileCount);

            if (!lastMetrics.bFinite || !lastMetrics.bEntityMapConsistent ||
                !lastMetrics.bReplayHealthy)
            {
                failure = lastMetrics.error.empty()
                    ? "world invariant failed"
                    : lastMetrics.error;
                break;
            }
            if (lastMetrics.championCount != kGameRosterSlotCount ||
                lastMetrics.botCount != kGameRosterSlotCount)
            {
                failure = "champion or bot count changed";
                break;
            }
            if (lastMetrics.entityCount > options.entityLimit)
            {
                failure = "entity limit exceeded";
                break;
            }
            if (lastMetrics.zeroComponentEntityCount != 0u)
            {
                failure = "alive ECS entity has no components";
                break;
            }
            if (lastMetrics.aggroNotificationCount > 512u)
            {
                failure = "tower aggro notification backlog exceeded limit";
                break;
            }
            if (lastMetrics.pendingExecCount != 0u ||
                lastMetrics.pendingReplayCount != 0u)
            {
                failure = "room command queue did not drain by tick end";
                break;
            }
            if (lastMetrics.keyframeCount > 90u)
            {
                failure = "debug keyframe ring exceeded capacity";
                break;
            }
            if (lastMetrics.keyframeBytes > kKeyframeByteLimit)
            {
                failure = "debug keyframe ring exceeded byte limit";
                break;
            }
            if (lifecycle.respawnAIResetFailures != 0u)
            {
                failure = "respawn retained stale ChampionAI commitment state";
                break;
            }
            if (lifecycle.respawnManaFailures != 0u)
            {
                failure = "respawn did not restore champion mana";
                break;
            }
            if (lifecycle.overdueRespawnCount != 0u)
            {
                failure = "champion exceeded respawn deadline";
                break;
            }
            if (tick > kBotInactivityLimitTicks &&
                (lifecycle.inactiveBotCount != 0u ||
                    lifecycle.commandActiveEntities.size() !=
                        kGameRosterSlotCount))
            {
                CGameRoomIntegrationProbeAccess::PrintInactiveBots(
                    *room,
                    tick,
                    lifecycle);
                failure = "one or more bots were command-inactive for 60 simulated seconds";
                break;
            }
        }

        if (bHeartbeat)
        {
            lastMemory = QueryProcessMemory();
            if (!lastMemory.bValid)
            {
                failure = "process memory query failed";
                break;
            }
            peakWorkingSetBytes =
                (std::max)(peakWorkingSetBytes, lastMemory.workingSetBytes);
            peakPrivateBytes = (std::max)(peakPrivateBytes, lastMemory.privateBytes);
            if (lastMemory.privateBytes > options.privateByteLimit)
            {
                failure = "private memory limit exceeded";
                break;
            }
            if (lastMemory.privateBytes >
                afterPrepare.privateBytes + kPrivateGrowthLimitBytes)
            {
                failure = "private memory growth exceeded 256 MiB";
                break;
            }

            const f64_t wallSeconds =
                std::chrono::duration<f64_t>(Clock::now() - runStart).count();
            std::cout << "PROGRESS tick=" << tick
                << " sim_sec=" << static_cast<f64_t>(tick) / 30.0
                << " wall_sec=" << wallSeconds
                << " entities=" << lastMetrics.entityCount
                << " minions=" << lastMetrics.minionCount
                << " projectiles=" << lastMetrics.projectileCount
                << " aggro_notifications="
                << lastMetrics.aggroNotificationCount
                << " zero_component_entities="
                << lastMetrics.zeroComponentEntityCount
                << " dead_champions=" << lastMetrics.deadChampionCount
                << " replay_records=" << lastMetrics.replayRecordCount
                << " replay_mib=" << BytesToMiB(lastMetrics.replaySpoolBytes)
                << " keyframe_mib=" << BytesToMiB(lastMetrics.keyframeBytes)
                << " rss_mib=" << BytesToMiB(lastMemory.workingSetBytes)
                << " private_mib=" << BytesToMiB(lastMemory.privateBytes)
                << " deaths=" << lifecycle.deathTransitions
                << " respawns=" << lifecycle.respawnTransitions
                << " respawn_ai_reset_failures="
                << lifecycle.respawnAIResetFailures
                << " respawn_mana_failures=" << lifecycle.respawnManaFailures
                << " command_active_bots="
                << lifecycle.commandActiveEntities.size()
                << " inactive_bots=" << lifecycle.inactiveBotCount
                << " command_seq_sum=" << lastMetrics.commandSequenceSum
                << '\n';
            std::cout.flush();
        }
    }

    const auto runEnd = Clock::now();
    const u64_t finalTick = CGameRoomIntegrationProbeAccess::TickIndex(*room);
    if (failure.empty() && finalTick != options.tickCount)
        failure = "final tick count mismatch";
    const u32_t observedMinionLaneSlotCount = static_cast<u32_t>(std::count(
        minionMotion.observedLaneSlots.begin(),
        minionMotion.observedLaneSlots.end(),
        true));
    if (failure.empty() &&
        finalTick >= 600u &&
        observedMinionLaneSlotCount != 6u)
    {
        failure = "GameRoom soak did not observe all six team/lane minion slots";
    }
    if (failure.empty() &&
        lifecycle.deathTransitions !=
            lifecycle.respawnTransitions + lastMetrics.deadChampionCount)
    {
        failure = "death and respawn lifecycle counts are inconsistent";
    }
    if (failure.empty() &&
        (lastMetrics.replayRecordCount == 0u ||
            lastMetrics.replaySnapshotCount != finalTick ||
            lastMetrics.replayFirstTick != 1u ||
            lastMetrics.replayLastTick != finalTick))
    {
        failure = "current replay did not cover every simulated tick";
    }

    std::vector<u8_t> finalState;
    if (failure.empty() &&
        !CGameRoomIntegrationProbeAccess::BuildFinalState(*room, finalState, error))
    {
        failure = error;
    }
    const u64_t finalHash = finalState.empty() ? 0u : HashBytes(finalState);
    if (failure.empty() &&
        !WriteBinaryEvidence("final_state.bin", finalState, error))
    {
        failure = error;
    }
    const size_t replayRecordsBeforeStop = lastMetrics.replayRecordCount;
    const WorldMetrics expectedReplay = lastMetrics;
    const wstring_t replayPath =
        CGameRoomIntegrationProbeAccess::ReplayPath(*room);

    const auto stopStart = Clock::now();
    room->Stop();
    const auto stopEnd = Clock::now();
    room.reset();
    const ProcessMemorySample afterStop = QueryProcessMemory();

    u64_t replayHash = 0u;
    u64_t replayHashedBytes = 0u;
    if (failure.empty() && !HashReplayRecordStream(
        std::filesystem::path(replayPath),
        expectedReplay,
        replayHash,
        replayHashedBytes,
        error))
    {
        failure = error;
    }

    const f64_t wallSeconds =
        std::chrono::duration<f64_t>(runEnd - runStart).count();
    const f64_t stopSeconds =
        std::chrono::duration<f64_t>(stopEnd - stopStart).count();
    const f64_t tickP50 = PercentileMicros(tickMicros, 0.50);
    const f64_t tickP95 = PercentileMicros(tickMicros, 0.95);
    const f64_t tickP99 = PercentileMicros(tickMicros, 0.99);
    const f64_t tickMax = tickMicros.empty()
        ? 0.0
        : *(std::max_element)(tickMicros.begin(), tickMicros.end());

    if (!afterStop.bValid && failure.empty())
        failure = "post-stop process memory query failed";
    if (stopSeconds > 10.0 && failure.empty())
        failure = "room stop exceeded 10 seconds";
    if (tickP99 > 33'333.333 && failure.empty())
        failure = "tick p99 exceeded the 30 Hz budget";
    if (tickMax >= 500'000.0 && failure.empty())
        failure = "single tick stall reached 500 milliseconds";
    const u32_t deadlineMissLimit = (options.tickCount +
        kDeadlineMissRateDenominator - 1u) /
        kDeadlineMissRateDenominator;
    if (deadlineMissCount > deadlineMissLimit && failure.empty())
        failure = "30 Hz deadline miss rate exceeded 0.5 percent";
    const int64_t steadyHandleDelta =
        static_cast<int64_t>(afterStop.handleCount) -
        static_cast<int64_t>(afterPrepare.handleCount);
    if (steadyHandleDelta > kSteadyHandleGrowthLimit && failure.empty())
        failure = "steady handle growth exceeded limit";

    const char* status = failure.empty() ? "PASS" : "FAIL";
    std::cout << "RESULT status=" << status
        << " ticks=" << finalTick
        << " seed=" << options.seed
        << " replay_hash=" << Hex64(replayHash)
        << " world_hash=" << Hex64(finalHash)
        << " replay_hashed_bytes=" << replayHashedBytes
        << " final_keyframe_bytes=" << finalState.size()
        << " wall_sec=" << wallSeconds
        << " stop_sec=" << stopSeconds
        << " tick_p50_us=" << tickP50
        << " tick_p95_us=" << tickP95
        << " tick_p99_us=" << tickP99
        << " tick_max_us=" << tickMax
        << " deadline_misses=" << deadlineMissCount
        << " peak_entities=" << peakEntityCount
        << " peak_minions=" << peakMinionCount
        << " peak_projectiles=" << peakProjectileCount
        << " deaths=" << lifecycle.deathTransitions
        << " respawns=" << lifecycle.respawnTransitions
        << " respawn_ai_reset_failures=" << lifecycle.respawnAIResetFailures
        << " respawn_mana_failures=" << lifecycle.respawnManaFailures
        << " command_active_bots=" << lifecycle.commandActiveEntities.size()
        << " inactive_bots=" << lifecycle.inactiveBotCount
        << " max_command_inactive_ticks=" << lifecycle.maxCommandInactivityTicks
        << " minion_lane_slots=" << observedMinionLaneSlotCount
        << " max_minion_lane_stall_ticks=" << minionMotion.maxLaneStallTicks
        << " minion_opposed_yaw=" << minionMotion.opposedYawCount
        << " first_opposed_yaw_entity="
        << static_cast<u32_t>(minionMotion.firstOpposedYawEntity)
        << " replay_records=" << replayRecordsBeforeStop
        << " peak_rss_mib=" << BytesToMiB(peakWorkingSetBytes)
        << " peak_private_mib=" << BytesToMiB(peakPrivateBytes)
        << " private_growth_mib="
        << BytesToMiB(peakPrivateBytes > afterPrepare.privateBytes
            ? peakPrivateBytes - afterPrepare.privateBytes
            : 0u)
        << " post_stop_private_mib=" << BytesToMiB(afterStop.privateBytes)
        << " handle_delta="
        << static_cast<int64_t>(afterStop.handleCount) -
            static_cast<int64_t>(processStart.handleCount)
        << " steady_handle_delta="
        << steadyHandleDelta;
    if (!failure.empty())
        std::cout << " reason=\"" << failure << '"';
    std::cout << '\n';

    return failure.empty() ? 0 : 1;
}
