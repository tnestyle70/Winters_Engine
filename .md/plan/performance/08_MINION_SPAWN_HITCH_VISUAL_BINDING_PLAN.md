Session - 미니언 웨이브 스폰 hitch를 visual binding queue로 제거한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

기존 코드:

```cpp
#include <cstdio>
#include <unordered_map>
```

아래로 교체:

```cpp
#include <cstdio>
#include <deque>
#include <unordered_map>
```

기존 코드:

```cpp
    static const char* ResolveModelPath(eMinionType eType, eMinionTeam eTeam);
    bool_t  Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeam);
    void    Release_NetworkVisual(EntityID entity);
```

아래로 교체:

```cpp
    static const char* ResolveModelPath(eMinionType eType, eMinionTeam eTeam);
    void    QueueNetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeam);
    uint32_t ProcessQueuedNetworkVisuals(uint32_t maxCreates);
    uint32_t GetQueuedNetworkVisualCount() const { return static_cast<uint32_t>(m_deqPendingNetworkVisuals.size()); }
    bool_t  Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeam);
    void    Release_NetworkVisual(EntityID entity);
```

기존 코드:

```cpp
    struct MinionVisualPlaybackState
    {
        eMinionVisualPhase phase = eMinionVisualPhase::Base;
        uint32_t lastActionSeq = 0;
        uint16_t lastAnimId = 0;
        uint8_t baseState = 0xff;
        f32_t phaseTimer = 0.f;
    };
```

아래에 추가:

```cpp
    struct NetworkVisualRequest
    {
        EntityID entity = NULL_ENTITY;
        eMinionType type = eMinionType::Melee;
        eMinionTeam team = eMinionTeam::Blue;
    };
```

기존 코드:

```cpp
    std::vector<EntityID>                                        m_vecSpawnedThisTick;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::unordered_map<EntityID, MinionVisualPlaybackState>       m_mapVisualStates;
```

아래로 교체:

```cpp
    std::vector<EntityID>                                        m_vecSpawnedThisTick;
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::unordered_map<EntityID, MinionVisualPlaybackState>       m_mapVisualStates;
    std::deque<NetworkVisualRequest>                             m_deqPendingNetworkVisuals;
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:

```cpp
    static constexpr f32_t kMinionBaseAnimUpdateInterval = 1.f / 10.f;
    static constexpr f32_t kMinionHighPriorityAnimUpdateInterval = 1.f / 20.f;
    static constexpr uint64_t kMinionAnimUpdateBudget = 14u;
```

아래로 교체:

```cpp
    static constexpr f32_t kMinionBaseAnimUpdateInterval = 1.f / 10.f;
    static constexpr f32_t kMinionHighPriorityAnimUpdateInterval = 1.f / 20.f;
    static constexpr uint64_t kMinionAnimUpdateBudget = 14u;
    static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u;
```

`void CMinion_Manager::Clear()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    m_mapRenderers.clear();
    m_mapVisualStates.clear();
```

아래에 추가:

```cpp
    m_deqPendingNetworkVisuals.clear();
```

`void CMinion_Manager::TickVisuals(f32_t fDeltaTime)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!m_pWorld)
        return;
```

아래에 추가:

```cpp
    ProcessQueuedNetworkVisuals(kNetworkVisualBindBudgetPerFrame);
```

`bool_t CMinion_Manager::Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeamParam)` 바로 위에 추가:

```cpp
void CMinion_Manager::QueueNetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeamParam)
{
    if (!m_pWorld || entity == NULL_ENTITY)
        return;

    if (static_cast<u32_t>(eType) >= static_cast<u32_t>(eMinionType::End))
        eType = eMinionType::Melee;

    if (eTeamParam == eMinionTeam::End)
        eTeamParam = eMinionTeam::Blue;

    if (m_pWorld->HasComponent<RenderComponent>(entity))
    {
        auto& rc = m_pWorld->GetComponent<RenderComponent>(entity);
        if (rc.pRenderer)
            return;
    }

    for (NetworkVisualRequest& request : m_deqPendingNetworkVisuals)
    {
        if (request.entity != entity)
            continue;

        request.type = eType;
        request.team = eTeamParam;
        return;
    }

    NetworkVisualRequest request{};
    request.entity = entity;
    request.type = eType;
    request.team = eTeamParam;
    m_deqPendingNetworkVisuals.push_back(request);
}

uint32_t CMinion_Manager::ProcessQueuedNetworkVisuals(uint32_t maxCreates)
{
    if (!m_pWorld || maxCreates == 0u)
        return 0u;

    uint32_t createdCount = 0u;
    uint32_t skippedCount = 0u;
    uint32_t failedCount = 0u;

    while (!m_deqPendingNetworkVisuals.empty() &&
        createdCount < maxCreates)
    {
        const NetworkVisualRequest request = m_deqPendingNetworkVisuals.front();
        m_deqPendingNetworkVisuals.pop_front();

        if (request.entity == NULL_ENTITY ||
            !m_pWorld->IsAlive(request.entity))
        {
            ++skippedCount;
            continue;
        }

        if (m_pWorld->HasComponent<RenderComponent>(request.entity))
        {
            auto& rc = m_pWorld->GetComponent<RenderComponent>(request.entity);
            if (rc.pRenderer)
            {
                ++skippedCount;
                continue;
            }
        }

        if (Ensure_NetworkVisual(request.entity, request.type, request.team))
            ++createdCount;
        else
            ++failedCount;
    }

    WINTERS_PROFILE_COUNT("MinionVisual::Queue", static_cast<uint64_t>(m_deqPendingNetworkVisuals.size()));
    WINTERS_PROFILE_COUNT("MinionVisual::Created", createdCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Skipped", skippedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::Failed", failedCount);

    return createdCount;
}
```

`void CMinion_Manager::Release_NetworkVisual(EntityID entity)` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (entity == NULL_ENTITY)
        return;
```

아래에 추가:

```cpp
    m_deqPendingNetworkVisuals.erase(
        std::remove_if(
            m_deqPendingNetworkVisuals.begin(),
            m_deqPendingNetworkVisuals.end(),
            [entity](const NetworkVisualRequest& request)
            {
                return request.entity == entity;
            }),
        m_deqPendingNetworkVisuals.end());
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
            CMinion_Manager::Get()->Ensure_NetworkVisual(
                e,
                ToSnapshotMinionType(es->subtype()),
                ToSnapshotMinionTeam(es->team()));
```

아래로 교체:

```cpp
            CMinion_Manager::Get()->QueueNetworkVisual(
                e,
                ToSnapshotMinionType(es->subtype()),
                ToSnapshotMinionTeam(es->team()));
```

2. 검증

검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- 첫 미니언 웨이브 스폰 순간 `Frame` spike가 16.67ms 아래로 내려가는지 확인.
- `profiler.json`에서 `MinionVisual::Queue`, `MinionVisual::Created`, `MinionVisual::Failed`를 확인.
- 웨이브가 36마리 생성되어도 한 프레임에 renderer 생성이 3개 이하로 제한되는지 확인.
- 아직 renderer가 붙지 않은 미니언도 서버 snapshot 위치/HP/state component는 계속 갱신되는지 확인.
- 화면에 순차적으로 미니언이 나타나는 1초 내외의 visual pop-in이 허용 가능한지 확인.

Session - 미니언 모델 리소스를 InGame bootstrap에서 prewarm한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/Renderer/ModelRenderer.h

기존 코드:

```cpp
	bool	Initialize(const std::string& strFbxPath,
		const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
```

아래로 교체:

```cpp
	bool	Initialize(const std::string& strFbxPath,
		const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
	static bool PrewarmModel(const std::string& strFbxPath);
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

`bool ModelRenderer::Initialize(const string& strFbxPath, const wchar_t* pHlslPath)` 바로 위에 추가:

```cpp
bool ModelRenderer::PrewarmModel(const string& strFbxPath)
{
    auto& app = CEngineApp::Get();
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    if (!pDevice)
        return false;

    return app.GetResourceCache().LoadModel(pDevice, strFbxPath) != nullptr;
}
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

기존 코드:

```cpp
    void    OnImGui_Tuner();
    void    DEBUG_SpawnWaveNow();
```

아래로 교체:

```cpp
    void    OnImGui_Tuner();
    void    DEBUG_SpawnWaveNow();
    void    PrewarmNetworkVisualResources();
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

`HRESULT CMinion_Manager::Initialize(CWorld* pWorld)` 바로 아래에 추가:

```cpp
void CMinion_Manager::PrewarmNetworkVisualResources()
{
    WINTERS_PROFILE_SCOPE("MinionVisual::Prewarm");

    uint64_t loadedCount = 0u;
    uint64_t failedCount = 0u;

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
            if (!pPath)
            {
                ++failedCount;
                continue;
            }

            if (ModelRenderer::PrewarmModel(pPath))
                ++loadedCount;
            else
                ++failedCount;
        }
    }

    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmLoaded", loadedCount);
    WINTERS_PROFILE_COUNT("MinionVisual::PrewarmFailed", failedCount);
}
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

기존 코드:

```cpp
    CStructure_Manager::Get()->Initialize(&scene.m_World);
    CJungle_Manager::Get()->Initialize(&scene.m_World);
    CMinion_Manager::Get()->Initialize(&scene.m_World);
```

아래로 교체:

```cpp
    CStructure_Manager::Get()->Initialize(&scene.m_World);
    CJungle_Manager::Get()->Initialize(&scene.m_World);
    CMinion_Manager::Get()->Initialize(&scene.m_World);
    CMinion_Manager::Get()->PrewarmNetworkVisualResources();
```

2. 검증

검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- 첫 웨이브 전 `Model::LoadModel`, `Model::WMeshLoad`가 InGame bootstrap 구간에서만 발생하는지 확인.
- 첫 웨이브 스폰 순간 `Model::LoadModel` row가 다시 나타나지 않는지 확인.
- `MinionVisual::PrewarmFailed`가 0인지 확인.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.

Session - 해제된 미니언 ModelRenderer를 타입/팀별 pool로 재사용한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

기존 코드:

```cpp
    void     DoSpawnWave();
    EntityID Spawn_Minion(eMinionType eType, eMinionTeam eTeam, eMinionWay eWay);
    void     UpdateMinionVisual(EntityID entity,
        MinionStateComponent& ms,
        RenderComponent& rc,
        f32_t fDeltaTime);
```

아래로 교체:

```cpp
    void     DoSpawnWave();
    EntityID Spawn_Minion(eMinionType eType, eMinionTeam eTeam, eMinionWay eWay);
    std::unique_ptr<ModelRenderer> AcquireNetworkRenderer(
        eMinionType eType,
        eMinionTeam eTeam,
        const char* pPath);
    void     PoolNetworkRenderer(
        eMinionType eType,
        eMinionTeam eTeam,
        std::unique_ptr<ModelRenderer> pRenderer);
    void     UpdateMinionVisual(EntityID entity,
        MinionStateComponent& ms,
        RenderComponent& rc,
        f32_t fDeltaTime);
```

기존 코드:

```cpp
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::unordered_map<EntityID, MinionVisualPlaybackState>       m_mapVisualStates;
    std::deque<NetworkVisualRequest>                             m_deqPendingNetworkVisuals;
```

아래로 교체:

```cpp
    std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_mapRenderers;
    std::unordered_map<EntityID, MinionVisualPlaybackState>       m_mapVisualStates;
    std::deque<NetworkVisualRequest>                             m_deqPendingNetworkVisuals;
    std::vector<std::unique_ptr<ModelRenderer>>                   m_vecNetworkRendererPool[2][5];
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

`bool_t CMinion_Manager::Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeamParam)` 바로 위에 추가:

```cpp
std::unique_ptr<ModelRenderer> CMinion_Manager::AcquireNetworkRenderer(
    eMinionType eType,
    eMinionTeam eTeamParam,
    const char* pPath)
{
    const u32_t teamIndex = (eTeamParam == eMinionTeam::Blue) ? 0u : 1u;
    const u32_t typeIndex = static_cast<u32_t>(eType);
    if (typeIndex >= 5u)
        return nullptr;

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

    if (eType == eMinionType::Tibbers)
        pRenderer->LoadTextureForAllMeshes(kTibbersTexturePath);

    return pRenderer;
}

void CMinion_Manager::PoolNetworkRenderer(
    eMinionType eType,
    eMinionTeam eTeamParam,
    std::unique_ptr<ModelRenderer> pRenderer)
{
    if (!pRenderer)
        return;

    const u32_t teamIndex = (eTeamParam == eMinionTeam::Blue) ? 0u : 1u;
    const u32_t typeIndex = static_cast<u32_t>(eType);
    if (typeIndex >= 5u)
        return;

    m_vecNetworkRendererPool[teamIndex][typeIndex].push_back(std::move(pRenderer));
}
```

`bool_t CMinion_Manager::Ensure_NetworkVisual(EntityID entity, eMinionType eType, eMinionTeam eTeamParam)` 안에서 아래 코드를:

```cpp
    auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
    if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
    {
        char msg[512]{};
        sprintf_s(msg,
            "[MinionVisual] bind FAIL: ModelRenderer::Initialize failed entity=%u type=%u team=%u path=%s\n",
            static_cast<u32_t>(entity),
            static_cast<u32_t>(eType),
            static_cast<u32_t>(eTeamParam),
            pPath);
        OutputMinionDebug(msg);
        return false;
    }
    if (eType == eMinionType::Tibbers)
        pRenderer->LoadTextureForAllMeshes(kTibbersTexturePath);
```

아래로 교체:

```cpp
    auto pRenderer = AcquireNetworkRenderer(eType, eTeamParam, pPath);
    if (!pRenderer)
    {
        char msg[512]{};
        sprintf_s(msg,
            "[MinionVisual] bind FAIL: AcquireNetworkRenderer failed entity=%u type=%u team=%u path=%s\n",
            static_cast<u32_t>(entity),
            static_cast<u32_t>(eType),
            static_cast<u32_t>(eTeamParam),
            pPath);
        OutputMinionDebug(msg);
        return false;
    }
```

`void CMinion_Manager::Release_NetworkVisual(EntityID entity)` 안에서 아래 코드를:

```cpp
    m_mapRenderers.erase(entity);
```

아래로 교체:

```cpp
    eMinionType type = eMinionType::Melee;
    eMinionTeam team = eMinionTeam::Blue;
    if (m_pWorld && m_pWorld->IsAlive(entity) &&
        m_pWorld->HasComponent<MinionStateComponent>(entity))
    {
        const auto& state = m_pWorld->GetComponent<MinionStateComponent>(entity);
        type = static_cast<eMinionType>(state.type);
        team = (state.team == eTeam::Red) ? eMinionTeam::Red : eMinionTeam::Blue;
    }

    if (m_pWorld && m_pWorld->IsAlive(entity) &&
        m_pWorld->HasComponent<RenderComponent>(entity))
    {
        auto& rc = m_pWorld->GetComponent<RenderComponent>(entity);
        rc.pRenderer = nullptr;
        rc.bVisible = false;
        rc.bAnimated = false;
    }

    auto rendererIt = m_mapRenderers.find(entity);
    if (rendererIt != m_mapRenderers.end())
    {
        PoolNetworkRenderer(type, team, std::move(rendererIt->second));
        m_mapRenderers.erase(rendererIt);
    }
```

`void CMinion_Manager::Clear()` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    m_deqPendingNetworkVisuals.clear();
```

아래에 추가:

```cpp
    for (auto& teamPool : m_vecNetworkRendererPool)
    {
        for (auto& typePool : teamPool)
            typePool.clear();
    }
```

2. 검증

검증 명령:
- `git diff --check`
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

수동 확인:
- 두 번째 이후 웨이브에서 `ModelRenderer::Initialize` 비용이 첫 웨이브보다 낮은지 확인.
- 미니언 사망 후 재사용된 renderer가 death animation 상태로 시작하지 않고 run/idle 상태로 정상 재개되는지 확인.
- Scene 전환 또는 `Clear()` 후 pool이 비워져 이전 world의 renderer가 남지 않는지 확인.

Session - 스킨드 미니언 GPU instancing 적용 가능 범위를 확정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

CONFIRM_NEEDED:
- 현재 `ModelRenderer::RenderWithVisibility`는 entity마다 `CBPerObject`, `CBBoneMatrices`, material bind 후 `CModel::RenderWithMask`를 호출한다.
- 스킨드 미니언은 개체마다 bone palette가 달라서 단순 `DrawIndexedInstanced`로 묶을 수 있는지 확인이 필요하다.
- 구현 전 `Skinned3D.hlsl`의 bone matrix 입력 방식, `CBBoneMatrices` 크기, per-instance transform 전달 방식을 확인한다.

1-2. C:/Users/tnest/Desktop/Winters/Engine/Private/Resource/Mesh.cpp

CONFIRM_NEEDED:
- `CMesh::Render`가 `DX11Buffer::DrawIndexed`만 호출하므로 instanced draw entry point가 없다.
- `CMesh::RenderInstanced` 또는 별도 minion batch renderer를 추가할지 확인한다.

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

CONFIRM_NEEDED:
- Session 1~3 이후에도 `Minion::Render`가 1ms 이상이면 `CMinion_Manager::Render`를 타입/팀/animation phase별 batch list로 바꾸는 별도 세션을 작성한다.
- animated skinned instancing이 과하면 1차 대안은 renderer pool + AOI + animation budget 유지로 둔다.

2. 검증

검증 명령:
- `git diff --check`
- Engine/Client Debug x64 빌드

확인 필요:
- Session 1~3 적용 후 `Minion::Render`, `Minion::AnimUpdate`, `Render::EndFrame`이 남는 병목인지 `profiler.json`으로 다시 판정한다.
- GPU instancing은 현재 hitch 제거의 필수 조건이 아니다. 스폰 hitch는 visual binding queue와 prewarm으로 먼저 제거한다.
