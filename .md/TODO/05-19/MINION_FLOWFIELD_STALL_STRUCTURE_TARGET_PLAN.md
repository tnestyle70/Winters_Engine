Session - inhibitor front minion stuck is fixed with flowfield progress fallback and structure edge-aware acquisition.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Server/Public/Game/ServerMinionTuning.h

기존 코드:

```cpp
	static constexpr u8_t kBlockedFramesBeforeRepath = 6u;

	static constexpr f32_t kTargetScanIntervalSec = 0.50f;
```

아래에 추가:

```cpp
	static constexpr u8_t kFlowFieldStallFramesBeforePathFallback = 4u;
	static constexpr f32_t kFlowFieldProgressSlackSq = 0.01f;
	static constexpr f32_t kStructureAcquireRangePadding = 0.75f;
```

1-2. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    bool_t TryMoveServerMinionByFlowFields(
        EntityID entity,
        MinionStateComponent& state,
        TransformComponent& transform,
        TickContext& tc,
        bool_t& outMoved);
```

아래로 교체:

```cpp
    bool_t TryMoveServerMinionByFlowFields(
        EntityID entity,
        MinionStateComponent& state,
        TransformComponent& transform,
        const Vec3& vLaneTarget,
        TickContext& tc,
        bool_t& outMoved);
```

1-3. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`TryResolveServerMinionTargetCandidate(...)` 내부에서 아래 기존 코드를 찾는다.

기존 코드:

```cpp
        outPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
        outDistSq = WintersMath::DistanceSqXZ(myPos, outPos);
        const f32_t maxRangeSq = maxRange * maxRange;
        return outDistSq <= maxRangeSq;
```

아래로 교체:

```cpp
        outPos = world.GetComponent<TransformComponent>(candidate).GetPosition();
        outDistSq = WintersMath::DistanceSqXZ(myPos, outPos);

        f32_t resolvedMaxRange = maxRange;
        if (world.HasComponent<StructureComponent>(candidate))
        {
            resolvedMaxRange += ResolveAgentRadius(world, candidate) +
                ServerMinionTuning::kStructureAcquireRangePadding;
        }

        const f32_t maxRangeSq = resolvedMaxRange * resolvedMaxRange;
        return outDistSq <= maxRangeSq;
```

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`FindClosestEnemyCombatTarget(...)`의 SpatialIndex branch에서 아래 기존 코드를 찾는다.

기존 코드:

```cpp
            std::vector<EntityID> candidates;
            candidates.reserve(64);
            pSpatial->QueryRadius(
                myPos,
                maxRange,
                targetMask,
                1u << TeamByte(myTeam),
                candidates);
```

아래로 교체:

```cpp
            const f32_t queryRange =
                maxRange + ServerMinionTuning::kStructureAcquireRangePadding;
            std::vector<EntityID> candidates;
            candidates.reserve(64);
            pSpatial->QueryRadius(
                myPos,
                queryRange,
                targetMask,
                1u << TeamByte(myTeam),
                candidates);
```

1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

lane 이동 호출부에서 아래 기존 코드를 찾는다.

기존 코드:

```cpp
                if (!TryMoveServerMinionByFlowFields(entity, state, transform, tc, bMoved))
```

아래로 교체:

```cpp
                if (!TryMoveServerMinionByFlowFields(entity, state, transform, laneTarget, tc, bMoved))
```

1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`TryMoveServerMinionByFlowFields(...)` 함수 전체를 아래 기존 코드에서 교체한다.

기존 코드:

```cpp
bool_t CGameRoom::TryMoveServerMinionByFlowFields(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    TickContext& tc,
    bool_t& outMoved)
{
    state.PathRebuildCooldown = (std::max)(0.f, state.PathRebuildCooldown - tc.fDt);

    Vec3 vDir{};
    if (!m_serverMinionWaves.TryResolveFlowDirection(
        state.team,
        state.lane,
        transform.GetPosition(),
        vDir))
    {
        return false;
    }

    const f32_t fLenSq = vDir.x * vDir.x + vDir.z * vDir.z;
    if (fLenSq <= 0.0001f)
        return false;

    const Vec3 vPos = transform.GetPosition();
    const f32_t fStep = state.moveSpeed * tc.fDt;
    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, vNext))
    {
        ++state.BlockedMoveFrames;
        return false;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = MinionStateComponent::LaneMove;
    state.PathCount = 0u;
    state.PathIndex = 0u;
    state.BlockedMoveFrames = 0u;
    outMoved = true;
    return true;
}
```

아래로 교체:

```cpp
bool_t CGameRoom::TryMoveServerMinionByFlowFields(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    const Vec3& vLaneTarget,
    TickContext& tc,
    bool_t& outMoved)
{
    state.PathRebuildCooldown = (std::max)(0.f, state.PathRebuildCooldown - tc.fDt);

    if (state.PathCount > 0u && state.PathIndex < state.PathCount)
        return false;

    const Vec3 vPos = transform.GetPosition();
    const f32_t fPrevLaneDistSq = WintersMath::DistanceSqXZ(vPos, vLaneTarget);

    Vec3 vDir{};
    if (!m_serverMinionWaves.TryResolveFlowDirection(
        state.team,
        state.lane,
        vPos,
        vDir))
    {
        return false;
    }

    const f32_t fLenSq = vDir.x * vDir.x + vDir.z * vDir.z;
    if (fLenSq <= 0.0001f)
        return false;

    const f32_t fStep = state.moveSpeed * tc.fDt;
    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, vNext))
    {
        ++state.BlockedMoveFrames;
        return false;
    }

    const f32_t fNextLaneDistSq = WintersMath::DistanceSqXZ(vNext, vLaneTarget);
    const bool_t bProgressed =
        fNextLaneDistSq + ServerMinionTuning::kFlowFieldProgressSlackSq < fPrevLaneDistSq;

    if (!bProgressed)
    {
        ++state.BlockedMoveFrames;
        if (state.BlockedMoveFrames >= ServerMinionTuning::kFlowFieldStallFramesBeforePathFallback)
        {
            static u32_t s_flowFieldFallbackLogCount = 0;
            if (s_flowFieldFallbackLogCount < 64u)
            {
                char msg[256]{};
                sprintf_s(msg,
                    "[MinionAI] flow fallback reason=stall entity=%u team=%u lane=%u blocked=%u pos=(%.2f,%.2f) target=(%.2f,%.2f)\n",
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(state.team),
                    static_cast<u32_t>(state.lane),
                    static_cast<u32_t>(state.BlockedMoveFrames),
                    vPos.x,
                    vPos.z,
                    vLaneTarget.x,
                    vLaneTarget.z);
                OutputServerAITrace(msg);
                ++s_flowFieldFallbackLogCount;
            }

            return false;
        }
    }
    else
    {
        state.BlockedMoveFrames = 0u;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = MinionStateComponent::LaneMove;
    state.PathCount = 0u;
    state.PathIndex = 0u;
    outMoved = true;
    return true;
}
```

1-7. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

미니언 attack debug log 안에서 아래 기존 코드를 찾는다.

기존 코드:

```cpp
                        char msg[256]{};
                        sprintf_s(msg,
                            "[MinionAI] attack tick=%llu entity=%u team=%u lane=%u target=%u pos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
                            static_cast<unsigned long long>(tc.tickIndex),
                            static_cast<u32_t>(entity),
                            static_cast<u32_t>(minion.team),
                            static_cast<u32_t>(state.lane),
                            static_cast<u32_t>(target),
                            pos.x,
                            pos.y,
                            pos.z,
                            targetPos.x,
                            targetPos.y,
                            targetPos.z);
```

아래로 교체:

```cpp
                        const char* pTargetKind = m_world.HasComponent<StructureComponent>(target)
                            ? "structure"
                            : (m_world.HasComponent<MinionComponent>(target) ? "minion" : "champion");
                        char msg[288]{};
                        sprintf_s(msg,
                            "[MinionAI] attack tick=%llu entity=%u team=%u lane=%u target=%u targetKind=%s pos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
                            static_cast<unsigned long long>(tc.tickIndex),
                            static_cast<u32_t>(entity),
                            static_cast<u32_t>(minion.team),
                            static_cast<u32_t>(state.lane),
                            static_cast<u32_t>(target),
                            pTargetKind,
                            pos.x,
                            pos.y,
                            pos.z,
                            targetPos.x,
                            targetPos.y,
                            targetPos.z);
```

1-8. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

변경 없음. `ServerMinionTuning.h`, `ServerMinionFlowField.*`, `ServerMinionWaveRuntime.*`, `GameRoom.*`는 이미 프로젝트에 등록되어 있다.

2. 검증

검증 명령:
- `git diff --check -- Server/Public/Game/ServerMinionTuning.h Server/Public/Game/GameRoom.h Server/Private/Game/GameRoom.cpp .md/TODO/05-19/MINION_FLOWFIELD_STALL_STRUCTURE_TARGET_PLAN.md`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

런타임 확인 필요:
- 억제기 앞에서 `[MinionAI] flow fallback reason=stall` 로그가 4프레임 정체 후 발생하는지 확인.
- fallback 후 active path가 남아 있는 동안 flowfield가 path를 지우지 않고 A* lane fallback이 이동을 이어받는지 확인.
- 포탑/억제기 edge 진입 시 structure가 target acquisition 후보에 들어오고 `[MinionAI] attack ... targetKind=structure` 로그가 나오는지 확인.
- 포탑 AI는 기존 `TurretAISystem::SelectTarget` 경로로 미니언을 계속 공격하는지 확인.
