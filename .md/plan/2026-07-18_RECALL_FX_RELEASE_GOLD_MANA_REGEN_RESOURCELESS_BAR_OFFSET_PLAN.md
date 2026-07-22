Session - 귀환 채널을 파란 원으로 표시하고 Release 시작 골드·마나 재생·무자원 월드 체력바 높이를 서버/클라이언트 소유권에 맞게 보정한다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-18_CHAMPION_RESOURCE_DIFFERENTIATION_PLAN.md · 2026-07-18_CHAMPION_RESOURCE_DIFFERENTIATION_RESULT.md · CLAUDE_Legacy.md

## 1. 결정 기록

① 문제·제약: 귀환 전용 WFX는 0개이고 Release 시작 골드는 10000, Mana 10명의 초당 재생은 0이다. 무자원 월드바는 높이 0.68배를 같은 중심에 배치해 리소스 보유 바보다 위쪽이 `0.16 × bar height` 낮다.
② 순진한 해법의 실패: Fiora E 전체 cue를 재생하면 노란 몸통·무기 FX까지 섞이고, 클라이언트에서 마나를 더하면 서버 snapshot과 갈라진다. 빈 마나바를 그리면 무자원 계약이 다시 흐려진다.
③ 메커니즘: 공용 `Recall.Channel` 지면 WFX를 권위 snapshot의 Recall action 수명에만 부착하고, 서버 Stat tick은 Mana/Energy의 authored 재생을 적용한다. 무자원 바는 높이를 줄이되 원래 2단 바의 위쪽 좌표를 유지한다.
④ 대조: 귀환 전용 원본 자산 대신 저장소에 실제 존재하는 `fiora_base_e_buff_mult.png`를 임시 공용 링으로 사용한다. Release 골드는 데이터 팩 10000을 유지하고 서버 Release 컴파일에서만 4000으로 override해 Debug 실험 기준선을 보존한다.
⑤ 대가: Release에서는 `spawnLoadout.startGold` 핫 오버레이가 4000 override보다 우선하지 않는다. Fiora 링은 임시 시각물이며, recall 원본 자산이 들어오면 WFX texture만 교체해야 한다. 픽셀 위치/색은 F5 화면 확인이 남는다.

## 2. 반영해야 하는 코드

### 2-1. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/recall.wfx

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Recall.Channel",
  "emitters": [
    {
      "name": "recall_ground_ring",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult.png",
      "lifetime": 6.0,
      "fade_in": 0.12,
      "fade_out": 0.35,
      "width": 3.0,
      "height": 3.0,
      "color": [0.45, 0.75, 1.35, 0.78],
      "attach_offset": [0.0, 0.05, 0.0],
      "billboard": false
    }
  ]
}
```

### 2-2. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    std::unordered_map<EntityID, NetworkActionAnimationState> m_NetworkActionAnimStates{};
```

아래에 추가:

```cpp
    std::unordered_map<EntityID, EntityHandle> m_NetworkRecallFxHandles{};
```

기존 코드:

```cpp
    void InitializeNetworkSession();
    bool_t PumpNetwork();
    void ReplayLastNetworkHelloIfShared();
```

아래로 교체:

```cpp
    void InitializeNetworkSession();
    bool_t PumpNetwork();
    void ReplayLastNetworkHelloIfShared();
    void StartNetworkRecallFx(EntityID entity);
    void StopNetworkRecallFx(EntityID entity);
    void ClearNetworkRecallFx();
```

### 2-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

기존 코드:

```cpp
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
```

아래로 교체:

```cpp
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
```

익명 namespace 종료 직후, `InitializeNetworkSession()` 바로 위에 추가:

```cpp
void CScene_InGame::StartNetworkRecallFx(EntityID entity)
{
    StopNetworkRecallFx(entity);
    if (entity == NULL_ENTITY ||
        !m_World.IsAlive(entity) ||
        !m_World.HasComponent<TransformComponent>(entity))
    {
        return;
    }

    FxCueContext cue{};
    cue.attachTo = entity;
    cue.vWorldPos = m_World.GetComponent<TransformComponent>(entity).GetPosition();
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = kRecallDurationSec;

    const EntityID fxEntity =
        CFxCuePlayer::Play(m_World, "Recall.Channel", cue);
    if (fxEntity != NULL_ENTITY && m_World.IsAlive(fxEntity))
        m_NetworkRecallFxHandles[entity] = m_World.GetEntityHandle(fxEntity);
}

void CScene_InGame::StopNetworkRecallFx(EntityID entity)
{
    const auto it = m_NetworkRecallFxHandles.find(entity);
    if (it == m_NetworkRecallFxHandles.end())
        return;

    if (m_World.IsAlive(it->second))
        m_World.DestroyEntity(it->second);
    m_NetworkRecallFxHandles.erase(it);
}

void CScene_InGame::ClearNetworkRecallFx()
{
    for (const auto& [entity, handle] : m_NetworkRecallFxHandles)
    {
        (void)entity;
        if (m_World.IsAlive(handle))
            m_World.DestroyEntity(handle);
    }
    m_NetworkRecallFxHandles.clear();
}
```

`UpdateNetworkChampionLocomotion`의 기존 코드:

```cpp
            const u64_t serverTick = m_pSnapshotApplier
                ? m_pSnapshotApplier->GetLastAppliedServerTick()
                : 0u;
            const bool_t bMoveBlockedByNetworkAction =
```

아래로 교체:

```cpp
            const u64_t serverTick = m_pSnapshotApplier
                ? m_pSnapshotApplier->GetLastAppliedServerTick()
                : 0u;
            const bool_t bRecallActionActive =
                pAction &&
                pAction->sequence != 0u &&
                static_cast<eActionStateId>(pAction->actionId) ==
                    eActionStateId::Recall &&
                serverTick < pAction->lockEndTick;
            if (!bRecallActionActive)
                StopNetworkRecallFx(e);
            const bool_t bMoveBlockedByNetworkAction =
```

`SetOnChampionVisualChangedCallback`에서 기존 코드:

```cpp
                m_NetworkActionAnimStates.erase(entity);
```

아래로 교체:

```cpp
                StopNetworkRecallFx(entity);
                m_NetworkActionAnimStates.erase(entity);
```

`SetOnRemoveEntityCallback`에서 같은 erase 직전에도 `StopNetworkRecallFx(entity);`를 추가한다.

사망 presentation의 기존 코드:

```cpp
                if (actionState.baseSeq != deathSeq)
                {
                    actionState = {};
```

아래로 교체:

```cpp
                if (actionState.baseSeq != deathSeq)
                {
                    StopNetworkRecallFx(e);
                    actionState = {};
```

새 action sequence의 기존 코드:

```cpp
                        const u32_t prevSoundActionSeq = actionState.actionSeq;
                        actionState = {};
                        actionState.actionSeq = pAction->sequence;
                        actionState.actionId = pAction->actionId;
```

아래로 교체:

```cpp
                        const u32_t prevSoundActionSeq = actionState.actionSeq;
                        StopNetworkRecallFx(e);
                        actionState = {};
                        actionState.actionSeq = pAction->sequence;
                        actionState.actionId = pAction->actionId;
                        if (bRecallActionActive)
                        {
                            StartNetworkRecallFx(e);
                        }
```

비 action pose 전환의 기존 코드:

```cpp
                else if (pPose && actionState.baseSeq != static_cast<u32_t>(pPose->startTick))
                {
                    actionState.baseSeq = static_cast<u32_t>(pPose->startTick);
```

아래로 교체:

```cpp
                else if (pPose && actionState.baseSeq != static_cast<u32_t>(pPose->startTick))
                {
                    StopNetworkRecallFx(e);
                    actionState.baseSeq = static_cast<u32_t>(pPose->startTick);
```

`RebaseNetworkTimeline`에서 기존 코드:

```cpp
    std::fill_n(m_uLastSkillCommandResultSeq, 5u, 0u);
    m_NetworkActionAnimStates.clear();
```

아래로 교체:

```cpp
    std::fill_n(m_uLastSkillCommandResultSeq, 5u, 0u);
    ClearNetworkRecallFx();
    m_NetworkActionAnimStates.clear();
```

### 2-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

기존 코드:

```cpp
void CScene_InGame::OnExit()
{
    Viego::Fx::StopAllSoulIdle(m_World);
```

아래로 교체:

```cpp
void CScene_InGame::OnExit()
{
    ClearNetworkRecallFx();
    Viego::Fx::StopAllSoulIdle(m_World);
```

### 2-5. C:/Users/user/Desktop/Winters/Server/Private/Game/Factory/ServerChampionEntityFactory.cpp

`BuildServerVisibleToAll()` 바로 위에 추가:

```cpp
namespace
{
#if defined(NDEBUG)
    constexpr u32_t kReleaseStartGold = 4000u;
#endif
}
```

기존 코드:

```cpp
    ctx.loadout = objectDefs.spawnLoadout;
    ctx.pDef = championDef;
```

아래로 교체:

```cpp
    ctx.loadout = objectDefs.spawnLoadout;
#if defined(NDEBUG)
    ctx.loadout.startGold = kReleaseStartGold;
#endif
    ctx.pDef = championDef;
```

### 2-6. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

IRELIA, KALISTA, SYLAS, ANNIE, ASHE, FIORA, EZREAL, JAX, MASTERYI, KINDRED의 기존 코드:

```json
"resourceKind": "Mana",
"resourceRegenPerSec": 0.0,
```

아래로 교체:

```json
"resourceKind": "Mana",
"resourceRegenPerSec": 3.0,
```

`Tools/ChampionData/build_champion_game_data.py`와 `Tools/LoLData/Build-LoLDefinitionPack.py`를 순서대로 실행해 Shared 생성물, ServerPrivate 투영 JSON, Server 생성 C++를 재생성한다. Energy 10, Flow/None 0은 유지한다.

### 2-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Stat/StatSystem.cpp

기존 코드:

```cpp
        if (stat.resourceKind != eChampionResourceKind::Energy ||
            stat.resourceRegenPerSec <= 0.f ||
```

아래로 교체:

```cpp
        const bool_t bRegeneratesOverTime =
            stat.resourceKind == eChampionResourceKind::Mana ||
            stat.resourceKind == eChampionResourceKind::Energy;
        if (!bRegeneratesOverTime ||
            stat.resourceRegenPerSec <= 0.f ||
```

### 2-8. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`BuildHealthBarScreenRects`의 기존 코드:

```cpp
    rects.BarMin = ImVec2(
        center.x - width * 0.5f,
        center.y - effectiveHeight * 0.5f);
    rects.BarMax = ImVec2(
        center.x + width * 0.5f,
        center.y + effectiveHeight * 0.5f);
```

아래로 교체:

```cpp
    rects.BarMin = ImVec2(
        center.x - width * 0.5f,
        center.y - height * 0.5f);
    rects.BarMax = ImVec2(
        center.x + width * 0.5f,
        rects.BarMin.y + effectiveHeight);
```

## 3. 검증

### 예측

- Recall action sequence가 시작되면 모든 관전자 클라이언트에서 챔피언 발밑에 파란 원 1개가 생기고, 완료·취소·사망·엔티티 제거·타임라인 재기준화에서 제거된다. 동일 sequence snapshot 반복은 중복 생성하지 않는다.
- Server Debug는 데이터 기준 시작 골드 10000을 유지하고 Server Release는 모든 플레이어/봇을 4000으로 조립한다.
- Mana 10명은 생존 중 1초에 3, Energy 2명은 기존 10을 회복하며 max clamp/사망 중 정지는 유지한다. Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.
- 무자원 챔피언은 리소스바 없이 0.68배 높이를 유지하되 체력바 상단이 Mana/Energy/Flow 챔피언과 일치한다. ImGui/RHI가 같은 rect helper를 사용하므로 둘 다 함께 보정된다.
- 게이트는 생성기 stale/스키마 오류, GameSim·Engine·Client·Server 양 구성 컴파일, 기존 resource probe 회귀를 잡는다. 귀환 링 실제 크기·색과 월드바 픽셀 위치는 자동 게이트가 없다.

### 검증 명령

```powershell
git diff --check
python Tools/LoLData/Test-ChampionGameDataSchema.py
python Tools/ChampionData/build_champion_game_data.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
$wfx = Get-Content Data/LoL/FX/recall.wfx -Raw | ConvertFrom-Json
if ($wfx.name -ne 'Recall.Channel' -or -not (Test-Path $wfx.emitters[0].texture)) { throw 'Recall WFX contract failed' }
msbuild Shared/GameSim/Include/GameSim.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
msbuild Engine/Include/Engine.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
msbuild Server/Include/Server.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64
msbuild Server/Include/Server.vcxproj /m:1 /p:Configuration=Release /p:Platform=x64
msbuild Client/Include/Client.vcxproj /m:1 /p:Configuration=Release /p:Platform=x64
Tools/Bin/Debug/SimLab.exe --resource-only
```

### 미검증

- 계획 시점에는 소스/생성물/빌드를 변경하지 않았다. 구현 후 위 명령을 실행한다.
- 실제 F5에서 귀환 시작/취소/완료, 파란 링 크기·색, 무자원 4명과 Mana/Energy 각 1명의 월드바 픽셀 정렬은 수동 화면 게이트다.

### 확인 필요

- 현재 다른 Codex가 수정 중인 `GameRoomChampionAI.cpp`, `ChampionAISystem.*`, `Tools/SimLab/main.cpp`는 이번 세션에서 편집하지 않는다. 빌드는 해당 변경까지 포함하므로 실패 시 이번 diff와 기존 dirty diff를 분리한다.
- Release startGold override는 요구대로 컴파일 구성 정책이다. 향후 Release에서도 핫 오버레이로 시작 골드를 바꿔야 하면 `SpawnLoadoutPolicyDef`에 구성별 필드를 데이터화해야 한다.

적용 후 `C:/Users/user/Desktop/Winters/.md/plan/2026-07-18_RECALL_FX_RELEASE_GOLD_MANA_REGEN_RESOURCELESS_BAR_OFFSET_RESULT.md`에 예측 vs 실측, 판결, ⑤ 갱신만 기록한다.

---

## 4. 2026-07-19 귀환 FX 가시성 복구 세션

Session - 서버 귀환 상태를 직접 복제하고 A키 공격 사거리 원과 같은 발밑 WFX로 표시한다

### 4-1. 결정 기록

- 문제·제약: 사용자 실기 검증에서 귀환 FX가 전혀 보이지 않았다. `RecallComponent`는 6초간 활성화되지만 `StartCommandActionState(Recall)`은 `IsReplicatedGameplayAction()` 대상이 아니어서 `lockEndTick == startTick`이 된다. 기존 클라이언트 조건 `serverTick < lockEndTick`은 첫 snapshot부터 거짓이므로 WFX 생성 자체가 실행되지 않는다.
- 실패한 접근: WFX JSON/texture 존재와 Debug/Release 컴파일만 확인해 화면 가시성을 PASS로 간주했다. `fiora_base_e_buff_mult.png` 자체는 유효한 256x256 청색 이중 링이고 동일한 전체 경로 표기와 GroundDecal은 다른 WFX에서도 사용되므로 asset 손상이나 경로 불일치가 1차 원인이 아니다.
- 선택한 메커니즘: 서버 snapshot `stateFlags`의 미사용 bit 31에 `kSnapshotStateRecallFlag`를 추가하고, 실제 `RecallComponent::bActive`만 flag로 직렬화한다. 클라이언트는 action lock 추정 대신 이 authoritative flag를 귀환 WFX 수명 gate로 사용한다. cancel, 피해, 사망, 완료, replay seek에서도 snapshot 하나로 일관된다.
- 크기·좌표: `StartNetworkRecallFx()`가 `GameplayQuery::ResolveAttackRangePreviewRadius()`를 호출하고 `diameter = radius * 2`를 WFX size override로 전달한다. 따라서 로컬 챔피언은 A키 원과 같은 공격 사거리+충돌 반경 preview 크기를 사용한다. WFX 발밑 offset은 A키 원과 같은 `+0.02f`로 맞춘다.
- 시각 asset: 기존 Fiora 청색 링 texture는 유지한다. WFX authoring preview 기본 직경은 Ashe A키 기준 `(6.0 + 0.75 + 1.2) * 2 = 15.9`로 올리되 runtime은 챔피언별 A키 직경으로 override한다.
- 관측성: Debug에서 귀환 FX spawn 성공/실패, entity, diameter, 위치를 최대 32회 `OutputDebugStringA`로 남긴다. Release 동작에는 로그 비용을 넣지 않는다.
- 권위 경계: `RecallComponent -> Snapshot flag -> Client WFX`이며 gameplay 결과나 귀환 완료 판정은 Client가 만들지 않는다. Shared에는 wire 의미의 상수만, Server에는 flag 작성만, Client에는 presentation만 둔다.
- 30% 상한 예산: 최대 30%는 링 색/크기/발밑 offset 조정에, 최소 70%는 authoritative gate 복구, 취소/완료/replay 수명 검증, Debug/Release 빌드에 사용한다.

### 4-2. 반영 코드

#### `Shared/GameSim/Definitions/SnapshotStateFlags.h`

기존 `kSnapshotStateKalistaOathswornRitualFlag` 아래에 추가:

```cpp
// Bit 31 is reserved for the authoritative RecallComponent snapshot state.
inline constexpr u32_t kSnapshotStateRecallFlag = 1u << 31;
```

#### `Server/Private/Game/SnapshotBuilder.cpp`

`RecallComponent.h` include를 추가하고 `stateFlags` 작성부에 아래를 추가:

```cpp
        if (world.HasComponent<RecallComponent>(entity) &&
            world.GetComponent<RecallComponent>(entity).bActive)
        {
            stateFlags |= kSnapshotStateRecallFlag;
        }
```

#### `Client/Private/Scene/Scene_InGameNetwork.cpp`

`StartNetworkRecallFx()`에서 챔피언별 A키 preview 직경을 계산해 cue에 전달하고 Debug trace를 남긴다:

```cpp
    eChampion champion = eChampion::NONE;
    if (m_World.HasComponent<ChampionComponent>(entity))
        champion = m_World.GetComponent<ChampionComponent>(entity).id;
    const StatComponent fallbackStat = BuildDefaultChampionStat(champion);
    const f32_t recallRadius = GameplayQuery::ResolveAttackRangePreviewRadius(
        m_World,
        entity,
        champion,
        fallbackStat.attackRange,
        m_bNetworkAuthoritativeGameplay);
    const f32_t recallDiameter = recallRadius * 2.f;

    cue.bOverrideSize = true;
    cue.fWidthOverride = recallDiameter;
    cue.fHeightOverride = recallDiameter;
```

spawn 결과 직후 아래 Debug 전용 trace를 추가:

```cpp
#if defined(_DEBUG)
    bool_t bTextureReady = false;
    if (fxEntity != NULL_ENTITY &&
        m_World.IsAlive(fxEntity) &&
        m_World.HasComponent<FxBillboardComponent>(fxEntity) &&
        m_pFxSystem)
    {
        const auto& recallFx =
            m_World.GetComponent<FxBillboardComponent>(fxEntity);
        bTextureReady = m_pFxSystem->PreloadTextureResource(recallFx.texturePath);
    }

    static u32_t s_recallFxSpawnTraceCount = 0u;
    if (s_recallFxSpawnTraceCount < 32u)
    {
        char message[256]{};
        sprintf_s(
            message,
            "[RecallFx] spawn owner=%u fx=%u spawned=%u texture=%u diameter=%.2f pos=(%.2f,%.2f,%.2f)\n",
            static_cast<u32_t>(entity),
            static_cast<u32_t>(fxEntity),
            fxEntity != NULL_ENTITY ? 1u : 0u,
            bTextureReady ? 1u : 0u,
            recallDiameter,
            cue.vWorldPos.x,
            cue.vWorldPos.y,
            cue.vWorldPos.z);
        OutputDebugStringA(message);
        ++s_recallFxSpawnTraceCount;
    }
#endif
```

`StopNetworkRecallFx()`는 handle 삭제 전에 아래 Debug trace를 남긴다:

```cpp
#if defined(_DEBUG)
    static u32_t s_recallFxStopTraceCount = 0u;
    if (s_recallFxStopTraceCount < 32u)
    {
        char message[160]{};
        sprintf_s(
            message,
            "[RecallFx] stop owner=%u alive=%u\n",
            static_cast<u32_t>(entity),
            m_World.IsAlive(it->second) ? 1u : 0u);
        OutputDebugStringA(message);
        ++s_recallFxStopTraceCount;
    }
#endif
```

기존 `pAction/actionId/lockEndTick` 기반 `bRecallActionActive` 계산을 아래 authoritative snapshot flag 계산과 매 프레임 handle 동기화로 교체:

```cpp
            const bool_t bRecallActionActive =
                m_World.HasComponent<ReplicatedStateComponent>(e) &&
                (m_World.GetComponent<ReplicatedStateComponent>(e).stateFlags &
                    kSnapshotStateRecallFlag) != 0u;

            const auto recallFxIt = m_NetworkRecallFxHandles.find(e);
            const bool_t bRecallFxAlive =
                recallFxIt != m_NetworkRecallFxHandles.end() &&
                m_World.IsAlive(recallFxIt->second);
            if (bRecallActionActive)
            {
                if (!bRecallFxAlive)
                    StartNetworkRecallFx(e);
            }
            else
            {
                StopNetworkRecallFx(e);
            }
```

action sequence 변경 분기 안의 `StopNetworkRecallFx(e);`와 조건부 `StartNetworkRecallFx(e);`는 삭제한다. action sequence는 애니메이션 재생에만 사용하고 FX는 위 authoritative flag 동기화만 소유한다.

#### `Data/LoL/FX/recall.wfx`

파일 전체를 아래로 교체:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Recall.Channel",
  "emitters": [
    {
      "name": "recall_ground_ring",
      "render_type": "GroundDecal",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_buff_mult.png",
      "lifetime": 6.0,
      "fade_in": 0.12,
      "fade_out": 0.35,
      "width": 15.9,
      "height": 15.9,
      "color": [0.45, 0.75, 1.35, 0.90],
      "attach_offset": [0.0, 0.02, 0.0],
      "billboard": false
    }
  ]
}
```

### 4-3. 성공 기준과 검증

- 서버 snapshot은 활성 `RecallComponent`에만 bit 31을 설정하고 cancel/피해/완료 다음 snapshot에서 제거한다.
- 클라이언트는 flag 상승 시 cue를 한 번 생성하고 flag 하강, 사망, entity 제거, visual swap, timeline rebase, scene exit에서 제거한다.
- 로컬 원거리 AD 챔피언의 귀환 링 직경은 같은 상태의 A키 공격 사거리 원 직경과 동일하고 중심은 챔피언 발밑 `y + 0.02f`다.
- WFX load, cue spawn, texture resource가 유효하며 Debug trace가 `spawned=1`과 양수 diameter를 남긴다.
- `git diff --check`, WFX JSON/resource contract, GameSim/Server/Client Debug 및 Release 빌드가 통과한다.
- 가능한 자동 smoke가 있으면 서버 1+클라이언트 runtime trace로 flag/spawn/stop을 확인한다. 자동 입력 경로가 없으면 화면 PASS를 주장하지 않고 Debug trace와 사용자 F5 체크 항목을 RESULT에 분리한다.

### 4-4. 독립 검토 게이트

- 상태: `CONDITIONAL PASS`, P0 없음. read-only 독립 검토 완료 후 아래 disposition을 반영했다.
- 수용: bit 31은 기존 snapshot bit 0~6 및 AI debug bit 7~30과 충돌하지 않는다. 의미 예약 주석을 추가한다.
- 수용: `RecallComponent::bActive` snapshot flag는 cancel/피해/CC/사망/완료와 replay payload를 함께 커버한다.
- 수용: P1 late join/누락 action/replay seek/만료 handle 문제를 막기 위해 FX spawn/stop을 action sequence 분기에서 제거하고 매 프레임 flag+handle로 동기화한다.
- 수용: A키와 같은 `ResolveAttackRangePreviewRadius() * 2`를 runtime 단일 크기 기준으로 유지한다.
- 수용: P2 WFX 기본 preview 12.0은 A키 동일 크기가 아니므로 Ashe 기준 15.9로 교체한다. `+0.02f`와 기존 texture는 유지한다.
- 수용: P1 계획 누락이던 `_DEBUG` 최대 32회 spawn/texture/stop trace의 정확한 코드를 추가한다.
- 수용: P1 화면 가시성은 빌드만으로 PASS 처리하지 않는다. runtime trace와 DX11 수동 화면 gate를 RESULT에 별도로 기록하고 기존 dirty hunk와 이번 변경을 분리한다.
