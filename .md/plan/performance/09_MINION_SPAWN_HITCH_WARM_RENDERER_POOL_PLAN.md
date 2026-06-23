Session - 미니언 스폰 프레임 hitch를 첫 웨이브 renderer pool prewarm과 원인 계측으로 제거한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

기존 코드:

```cpp
    std::deque<NetworkVisualRequest> m_deqPendingNetworkVisuals;
    std::vector<std::unique_ptr<ModelRenderer>> m_vecNetworkRendererPool[2][5];
```

아래로 교체:

```cpp
    std::deque<NetworkVisualRequest> m_deqPendingNetworkVisuals;
    std::vector<std::unique_ptr<ModelRenderer>> m_vecNetworkRendererPool[2][5];
    u32_t m_uNetworkPoolHitsThisFrame = 0u;
    u32_t m_uNetworkColdCreatesThisFrame = 0u;
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:

```cpp
    static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u;
```

아래로 교체:

```cpp
    static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u;
    static constexpr u32_t kNetworkWarmRoleCount = 2u;
    static constexpr u32_t kNetworkWarmPoolSizePerTeamAndRole = 9u;
```

`std::unique_ptr<ModelRenderer> CMinion_Manager::AcquireNetworkRenderer(...)` 안에서 아래 코드를:

```cpp
    auto& pool = m_vecNetworkRendererPool[teamIndex][typeIndex];
    if (!pool.empty())
    {
        std::unique_ptr<ModelRenderer> pRenderer = std::move(pool.back());
        pool.pop_back();
        return pRenderer;
    }

    std::unique_ptr<ModelRenderer> pRenderer(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
        return nullptr;
```

아래로 교체:

```cpp
    auto& pool = m_vecNetworkRendererPool[teamIndex][typeIndex];
    if (!pool.empty())
    {
        ++m_uNetworkPoolHitsThisFrame;
        std::unique_ptr<ModelRenderer> pRenderer = std::move(pool.back());
        pool.pop_back();
        return pRenderer;
    }

    ++m_uNetworkColdCreatesThisFrame;
    WINTERS_PROFILE_SCOPE("MinionVisual::ColdCreate");
    std::unique_ptr<ModelRenderer> pRenderer(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
        return nullptr;
```

`void CMinion_Manager::PrewarmNetworkVisualResources()` 전체를 아래로 교체:

```cpp
void CMinion_Manager::PrewarmNetworkVisualResources()
{
    WINTERS_PROFILE_SCOPE("MinionVisual::Prewarm");

    uint64_t loadedCount = 0u;
    uint64_t failedCount = 0u;
    uint64_t rendererCount = 0u;

    for (u32_t teamIndex = 0u;
        teamIndex < static_cast<u32_t>(eMinionTeam::End);
        ++teamIndex)
    {
        const eMinionTeam team = static_cast<eMinionTeam>(teamIndex);

        for (u32_t typeIndex = 0u;
            typeIndex < static_cast<u32_t>(eMinionType::End);
            ++typeIndex)
        {
            const eMinionType type = static_cast<eMinionType>(typeIndex);
            const char* pPath = ResolveModelPath(type, team);
            if (!pPath || !ModelRenderer::PrewarmModel(pPath))
            {
                ++failedCount;
                continue;
            }

            ++loadedCount;
            if (typeIndex >= kNetworkWarmRoleCount)
                continue;

            auto& pool = m_vecNetworkRendererPool[teamIndex][typeIndex];
            while (pool.size() < kNetworkWarmPoolSizePerTeamAndRole)
            {
                WINTERS_PROFILE_SCOPE("MinionVisual::PrewarmRenderer");
                std::unique_ptr<ModelRenderer> pRenderer(new ModelRenderer());
                if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
                {
                    ++failedCount;
                    break;
                }

                pool.push_back(std::move(pRenderer));
                ++rendererCount;
            }
        }
    }

    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmLoaded", loadedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmFailed", failedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmRenderers", rendererCount);
}
```

`uint32_t CMinion_Manager::ProcessQueueNetworkVisual(uint32_t maxCreates)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!m_pWorld || maxCreates == 0)
        return 0u;
```

아래에 추가:

```cpp
    WINTERS_PROFILE_SCOPE("MinionVisual::BindQueue");
    const uint64_t queueBefore = static_cast<uint64_t>(m_deqPendingNetworkVisuals.size());
    m_uNetworkPoolHitsThisFrame = 0u;
    m_uNetworkColdCreatesThisFrame = 0u;
```

기존 코드:

```cpp
    WINTERS_PROFILE_COUNT("MinionVisual::Queue", static_cast<uint64_t>(m_deqPendingNetworkVisuals.size()));
    WINTERS_PROFILE_COUNT("MinionVisual::Created", createdCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Skipped", skippedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Failed", failedCount);

    return createdCount;
```

아래로 교체:

```cpp
    WINTERS_PROFILE_COUNT("MinionVisual::QueueBefore", queueBefore);
    WINTERS_PROFILE_COUNT("MinionVisual::Queue", static_cast<uint64_t>(m_deqPendingNetworkVisuals.size()));
    WINTERS_PROFILE_COUNT("MinionVisual::Created", createdCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Skipped", skippedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Failed", failedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::PoolHit", m_uNetworkPoolHitsThisFrame);
    WINTERS_PROFILE_COUNT("MinionVisual::ColdCreate", m_uNetworkColdCreatesThisFrame);

#if defined(_DEBUG)
    if (createdCount > 0u || failedCount > 0u)
    {
        static u32_t s_networkVisualBatchLogCount = 0u;
        if (s_networkVisualBatchLogCount < 256u)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[MinionVisualBatch] queue=%llu->%llu created=%u poolHit=%u coldCreate=%u failed=%u\n",
                static_cast<unsigned long long>(queueBefore),
                static_cast<unsigned long long>(m_deqPendingNetworkVisuals.size()),
                createdCount,
                m_uNetworkPoolHitsThisFrame,
                m_uNetworkColdCreatesThisFrame,
                failedCount);
            OutputDebugStringA(msg);
            ++s_networkVisualBatchLogCount;
        }
    }
#endif

    return createdCount;
```

2. 검증

미검증:
- 실제 인게임 첫 웨이브의 `Frame` spike와 `MinionVisual::BindQueue` 상관관계 미검증

검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- F3 profiler를 켠 뒤 첫 웨이브 직후 F4로 저장한 `profiler.json`에서 `MinionVisual::BindQueue`와 `Frame`을 비교.
- 첫 웨이브 동안 `MinionVisual::PoolHit`는 생성 수와 같고 `MinionVisual::ColdCreate`는 0인지 확인.
- Visual Studio Output의 `[MinionVisualBatch]` 로그에서 `coldCreate=0`인지 확인.
- 첫 웨이브에서 미니언 36마리가 누락 없이 표시되고 run/attack/death animation이 정상인지 확인.
- 이후 웨이브에서 `coldCreate`가 다시 발생하면 동시 생존 미니언 수에 맞춰 별도 capacity 측정을 진행.

목표 budget:
- 첫 웨이브 스폰 프레임 `Frame` 16.67ms 이하.
- `MinionVisual::BindQueue` 1.0ms 이하.
- 첫 웨이브 `MinionVisual::ColdCreate` 0.
