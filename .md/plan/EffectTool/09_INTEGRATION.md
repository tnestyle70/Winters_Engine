# FX 통합 — SkillHook / AnimationEvent / Sound / Network / Scene

## 목표

FX 시스템을 **Winters 엔진의 기존 시스템** (스킬, 애니메이션, 사운드, 네트워크, 씬) 과 접붙여,
스킬 한 번에 "애니메이션 + FX + 사운드 + 데미지" 가 같은 이벤트로 동기화되게 만든다.

## 연동 지도

```
┌─────── Game Thread (60 fps) ───────┐
│                                    │
│  Scene_InGame                      │
│   ├─ Input (CInput)                │
│   ├─ Skill Dispatch (B-6.6)        │
│   │    ├─→ AnimationEvent ──┐      │
│   │    ├─→ SkillHook ──────┐│      │
│   │    └─→ SoundManager ───┼┼──▶ CGameInstance::PlaySoundOn
│   │                         ││      │
│   └─ ECS World              ││      │
│       └─ FxSimSystem        ││      │
│           └─ FxSpawnRequest ◀┘│      │
│                              │      │
└──────────────────────────────┼──────┘
                               │
                               ▼
                  CGameInstance::SpawnFx(asset, pos, dir)
```

## FxAssetLibrary — 에셋 태그 시스템

```cpp
// Engine/Public/FX/Asset/FxAssetLibrary.h
#pragma once
#include "FxAsset.h"
#include <unordered_map>
#include <string>

namespace Engine::FX {

class CFxAssetLibrary
{
public:
    static CFxAssetLibrary& Get() { static CFxAssetLibrary inst; return inst; }

    // 에셋 로드 + 태그 등록. key 는 "Irelia/Q_Hit" 같은 slash 계층
    FxAssetHandle Load(const std::wstring& path, const std::string& tag);

    // 태그로 조회. 없으면 0 반환.
    FxAssetHandle Find(const std::string& tag) const;

    // 핸들 → 에셋 포인터
    CFxAsset* Resolve(FxAssetHandle h);

    // 프리로드 (맵 로드 시 한 번에 warm-up)
    void PreloadDirectory(const std::wstring& dirPath,
                          const std::string& tagPrefix);

private:
    CFxAssetLibrary() = default;

    std::unordered_map<std::string, FxAssetHandle>      m_tagToHandle;
    std::unordered_map<FxAssetHandle, std::unique_ptr<CFxAsset>> m_assets;
    FxAssetHandle m_nextHandle = 1;
};

} // namespace Engine::FX
```

**사용 예**:
```cpp
CFxAssetLibrary::Get().PreloadDirectory(L"Resource/FX/Champions/Irelia", "Irelia");
// 로드 파일:
//   Resource/FX/Champions/Irelia/Q_Hit.fxg    → tag "Irelia/Q_Hit"
//   Resource/FX/Champions/Irelia/W_Shield.fxg → tag "Irelia/W_Shield"
//   Resource/FX/Champions/Irelia/R_Blade.fxg  → tag "Irelia/R_Blade"

auto h = CFxAssetLibrary::Get().Find("Irelia/Q_Hit");
CGameInstance::Get()->SpawnFx(h, hitPos, hitNormal);
```

## SkillHook 연동 (Phase B-10 예정)

B-10 에서 스킬마다 `uint32_t hookId` + Registry 도입 예정. FX 스폰을 그 훅의 **콜백** 으로 등록:

```cpp
// Client/Private/Champions/Irelia/IreliaSkillHooks.cpp
#include "SkillHookRegistry.h"
#include "FxAssetLibrary.h"
#include "GameInstance.h"

namespace {

struct IreliaHitFxHook
{
    FxAssetHandle m_qHitHandle = 0;

    void OnRegister()
    {
        m_qHitHandle = CFxAssetLibrary::Get().Find("Irelia/Q_Hit");
    }

    void OnSkillHit(const SkillHitEvent& ev)
    {
        if (ev.skillId != SKILL_IRELIA_Q) return;
        Vec3 normal = (GetPos(ev.attacker) - GetPos(ev.victim)).Normalized();
        CGameInstance::Get()->SpawnFx(m_qHitHandle, ev.hitPos, normal, ev.victim);
    }
};

// self-register (CLAUDE.md B-10 패턴)
static IreliaHitFxHook g_hook;
static SkillHookRegistrar g_registrar(&g_hook);

} // namespace
```

## AnimationEvent 연동 (Phase C-3 예정)

`.wanim` 에 이벤트 트랙. `FOOTSTEP` / `VFX_SPAWN` / `DAMAGE_NUMBER` 등.

```cpp
// Engine/Public/ECS/Components/AnimationEventComponent.h (예정)
enum class eAnimEventType : std::uint8_t
{
    Footstep,
    HitStart,
    HitEnd,
    VFXSpawn,       // ← 여기
    SFXPlay,
    DamageNumber
};

struct AnimationEvent
{
    f32_t          triggerTime;   // 로컬 애니 시간 [0, duration]
    eAnimEventType type;
    // type 별 payload
    union Payload {
        struct { char fxTag[48]; } vfx;           // "Irelia/Q_SwordTrail"
        struct { char soundKey[64]; f32_t volume; } sfx;
        struct { f32_t amount; std::uint32_t color; } dmg;
    } payload;
};
```

`CAnimator` 가 매 프레임 `currentTime` 비교 후 이벤트 dispatch:

```cpp
void CAnimator::Tick(f32_t dt)
{
    const f32_t prev = m_time;
    m_time += dt;
    // ...

    // 이벤트 트리거
    for (const auto& ev : m_activeClip->events) {
        if (prev < ev.triggerTime && m_time >= ev.triggerTime) {
            DispatchEvent(ev);
        }
    }
}

void CAnimator::DispatchEvent(const AnimationEvent& ev)
{
    switch (ev.type) {
    case eAnimEventType::VFXSpawn: {
        std::string tag(ev.payload.vfx.fxTag);
        auto h = CFxAssetLibrary::Get().Find(tag);
        if (h) {
            Mat4 boneWorld = GetBoneWorldMatrix(ev.boneIndex);
            Vec3 pos = ExtractPos(boneWorld);
            Vec3 dir = ExtractForward(boneWorld);
            CGameInstance::Get()->SpawnFx(h, pos, dir);
        }
    } break;
    case eAnimEventType::SFXPlay: {
        CGameInstance::Get()->PlayEffect(
            std::wstring(ev.payload.sfx.soundKey, ev.payload.sfx.soundKey + 64),
            ev.payload.sfx.volume);
    } break;
    // ...
    }
}
```

**즉 아티스트는 애니메이션 클립에 이벤트만 심으면 FX+사운드가 자동 스폰** — Niagara 의 "Anim Notify" 방식.

## Sound 동기 — FxAsset::AudioLink

FxAsset 생성 시 사운드 링크를 같이 지정하면 스폰 시 자동 재생:

```cpp
// SpawnFx 구현 (Engine/Private/GameInstance.cpp)
FxInstanceID CGameInstance::SpawnFx(FxAssetHandle asset,
                                     const Vec3& worldPos,
                                     const Vec3& worldDir,
                                     EntityID attachTo)
{
    auto* pAsset = CFxAssetLibrary::Get().Resolve(asset);
    if (!pAsset) return 0;

    // FX 인스턴스 생성
    FxInstanceID id = m_pFxSystem->Spawn(*pAsset, worldPos, worldDir, attachTo);

    // AudioLink 자동 재생
    for (const auto& link : pAsset->GetAudioLinks()) {
        if (link.delaySec > 0.f) {
            ScheduleDelayedSound(link);   // 내부 타이머 큐
        } else {
            PlaySoundOn(link.soundKey, link.channel, link.volume);
        }
    }

    return id;
}
```

### 지연 재생 큐

```cpp
struct DelayedSoundItem {
    f32_t         remaining;
    std::wstring  soundKey;
    eSoundChannel channel;
    f32_t         volume;
};

// CGameInstance::Tick_Engine 에서
void CGameInstance::Tick_Engine()
{
    // ... 기존 ...
    const f32_t dt = Get_TimeDelta(L"Default");
    for (auto it = m_delayedSounds.begin(); it != m_delayedSounds.end(); ) {
        it->remaining -= dt;
        if (it->remaining <= 0.f) {
            PlaySoundOn(it->soundKey, it->channel, it->volume);
            it = m_delayedSounds.erase(it);
        } else ++it;
    }
}
```

## Scene 통합

### Scene_InGame

```cpp
// Client/Private/Scene/Scene_InGame.cpp
HRESULT Scene_InGame::OnEnter()
{
    // 1. FX 에셋 프리로드
    auto& lib = Engine::FX::CFxAssetLibrary::Get();
    lib.PreloadDirectory(L"Resource/FX/Champions/Irelia",  "Irelia");
    lib.PreloadDirectory(L"Resource/FX/Champions/Yasuo",   "Yasuo");
    lib.PreloadDirectory(L"Resource/FX/Champions/Sylas",   "Sylas");
    lib.PreloadDirectory(L"Resource/FX/Champions/Viego",   "Viego");
    lib.PreloadDirectory(L"Resource/FX/Champions/Kalista", "Kalista");
    lib.PreloadDirectory(L"Resource/FX/Map",               "Map");

    // 2. ECS 시스템 등록
    auto& world = CGameInstance::Get()->GetWorld();   // ECS 월드 접근
    world.AddSystem<Engine::FX::CFxSimSystem>();
    world.AddSystem<Engine::FX::CFxRenderSystem>();
    world.AddSystem<Engine::FX::CFxLifetimeSystem>();

    return S_OK;
}
```

### Scene_Editor (Stage 6 노드 에디터 통합)

이미 존재하는 맵 에디터 씬에 "FX Editor" 탭 추가:

```cpp
void Scene_Editor::OnImGui()
{
    if (ImGui::BeginTabBar("Editor Tabs")) {
        if (ImGui::BeginTabItem("Map")) {
            DrawMapEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("FX Node Editor")) {
            if (m_pFxEditor) m_pFxEditor->OnImGui();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
```

## 네트워크 — 결정적 FX vs 시각 FX

Phase 4 (네트워크) 이후 가장 복잡한 통합.

### 두 경로

```
┌──── 서버 권위 스킬 (결정적 FX) ────┐
│                                    │
│  서버: SkillHit 판정              │
│    └─ SkillHitEvent 브로드캐스트 │
│         (포함: skillId, seed,    │
│          hitPos, hitNormal)       │
│                                    │
│  클라: 이벤트 수신                │
│    └─ CGameInstance::SpawnFx(    │
│         h, pos, dir,              │
│         deterministic=true,       │
│         seed=서버seed)            │
│                                    │
│  → 모든 클라이언트에 동일 FX     │
└────────────────────────────────────┘

┌──── 시각 전용 FX (Non-deterministic) ──┐
│                                          │
│  로컬 마우스 호버 시 커서 FX            │
│    └─ 즉시 스폰 (seed = 로컬 난수)      │
│                                          │
│  → 클라마다 다르게 보여도 무관         │
└──────────────────────────────────────────┘
```

### 이벤트 직렬화

```cpp
// Shared/PacketDef.h (예정)
struct PKT_FxSpawnEvent
{
    std::uint16_t msgId;              // FX_SPAWN
    std::uint32_t fxAssetHandle;      // 서버-클라 동일 handle 테이블 필요
    float         worldPos[3];
    float         worldDir[3];
    std::uint32_t rngSeed;
    std::uint32_t attachEntity;       // 0 이면 월드 고정
    std::uint32_t timestampTick;      // 동기화용
};
```

### 서버-클라 에셋 핸들 동기

서버 부팅 시 **동일한 정렬 순서** 로 에셋을 로드하면 handle 이 일치:

```cpp
// 서버와 클라 양쪽에서 같은 순서로 로드
std::vector<std::wstring> list = EnumerateDirectorySorted(L"Resource/FX");
for (const auto& p : list) {
    lib.Load(p, PathToTag(p));
}
```

또는 안전하게 **tag 문자열** 을 네트워크로 전송 (대역폭 비용 약간 증가).

## Phase D Physics 연동 (선택)

CollisionNode — 파티클이 NavMesh / Collider 와 충돌:

```hlsl
// Stage 7 GPU 에서
[numthreads(64, 1, 1)]
void CSUpdateWithCollision(uint3 gid : SV_DispatchThreadID)
{
    uint i = gid.x;
    // ... 일반 update ...

    // Collision: BVH 샘플링
    if (TestRayAgainstBVH(g_Position[i], g_Velocity[i] * g_DeltaTime, hit)) {
        g_Velocity[i] = reflect(g_Velocity[i], hit.normal) * 0.5;  // 탄성
        g_Position[i] = hit.point;
    }
}
```

**Phase D 의 BVH 가 준비된 후** 가능. 지금 단계는 Scope 밖.

## Gameplay 연동 — 버프 / 상태 이펙트 (B-11 예정)

```cpp
struct BuffComponent
{
    std::vector<BuffInstance> active;
};

struct BuffInstance
{
    std::uint32_t  buffId;
    f32_t          remaining;
    FxInstanceID   attachedFxId = 0;   // 버프 시각화 FX
};

// 버프 적용 시
void ApplyBuff(EntityID target, const BuffDef& def)
{
    BuffInstance inst;
    inst.buffId    = def.id;
    inst.remaining = def.duration;

    // 버프가 시각 FX 를 요구하면 자동 스폰
    if (!def.fxTag.empty()) {
        auto h = CFxAssetLibrary::Get().Find(def.fxTag);
        Vec3 pos = GetPos(target);
        inst.attachedFxId = CGameInstance::Get()->SpawnFx(h, pos, {0,1,0}, target);
    }
    GetStore<BuffComponent>().Get(target).active.push_back(inst);
}

// 버프 해제
void RemoveBuff(EntityID target, std::uint32_t buffId)
{
    auto& buffs = GetStore<BuffComponent>().Get(target).active;
    for (auto& b : buffs) {
        if (b.buffId == buffId) {
            if (b.attachedFxId)
                CGameInstance::Get()->KillFx(b.attachedFxId);
        }
    }
    // ... erase ...
}
```

## 실행 순서 — 한 프레임 (통합 최종)

```
CEngineApp::Tick (dt)
  ├─ CGameInstance::Tick_Engine
  │   ├─ Timer_Manager::Tick
  │   └─ DelayedSound queue 소비
  │
  ├─ Scene::OnUpdate(dt)
  │   ├─ CInput::Update
  │   ├─ SkillDispatchSystem (B-9)       ← Input → SkillFire
  │   │   └─ SkillHook::OnFire           ← FX / Sound 스폰
  │   ├─ SkillSimulationSystem           ← Hit 판정
  │   │   └─ SkillHook::OnHit            ← FX / Sound / Damage
  │   ├─ AnimatorSystem                  ← AnimEvent → VFX_SPAWN/SFX
  │   ├─ BuffSystem (B-11)               ← 만료 → Fx Kill
  │   ├─ FxSimSystem                     ← 모든 FxInstance Tick
  │   │   └─ EmitterInstance::Tick
  │   │       └─ Executor::Run[Stage]
  │   └─ FxLifetimeSystem                ← 수명 종료 인스턴스 회수
  │
  └─ Scene::OnRender()
      ├─ Opaque (Mesh, Static)
      ├─ Skinned (Champion)
      ├─ FxRenderSystem                  ← 반투명 FX 마지막
      │   ├─ 카메라 정렬 + 소팅
      │   ├─ FxBillboardRenderer::Render (CPU)
      │   └─ FxBillboardRenderer::RenderGpu (Indirect, Stage 7)
      └─ ImGui
```

## 데이터 디렉토리

```
Client/Bin/Resource/FX/
├── Champions/
│   ├── Irelia/
│   │   ├── Q_Hit.fxg
│   │   ├── Q_Trail.fxg
│   │   ├── W_Shield.fxg
│   │   ├── E_Stun.fxg
│   │   └── R_Blade.fxg
│   ├── Yasuo/
│   ├── Sylas/
│   ├── Viego/
│   └── Kalista/
├── Map/
│   ├── BaronPit.fxg
│   ├── DragonPit.fxg
│   └── RiftSpawn.fxg
├── Common/
│   ├── HitGeneric.fxg
│   ├── LevelUp.fxg
│   └── Recall.fxg
└── Atlas/
    ├── T_FxAtlas_01.dds     (불꽃/연기)
    ├── T_FxAtlas_02.dds     (마법 아이콘)
    └── T_FxAtlas_03.dds     (빛줄기)
```

## Gotchas

- **FxAssetLibrary 싱글턴 vs GameInstance Tier**: Library 는 Tier 2 (직접 접근). GameInstance Tier 1 은 `SpawnFx(handle, ...)` 만. Client 가 Library 에서 handle 찾아서 Tier 1 호출
- **에셋 handle 0 안전 처리**: `SpawnFx(0, ...)` 는 no-op. 로드 실패한 에셋으로 crash 방지
- **AudioLink 의 SoundChannel enum**: Sound_Manager 의 `eSoundChannel` enum 과 정확히 일치해야. JSON 에서 "Effect0" 문자열 → enum 매핑 필요
- **Scene 전환 시 FxInstance 전부 kill**: `Scene_InGame::OnExit` 에서 `CFxSystem::KillAll()` 호출. 안 하면 다음 씬에서 좀비 인스턴스
- **네트워크 이벤트 지연**: 서버가 FxSpawn 을 1 프레임 뒤에 내려주면 클라에서 "히트 타이밍 ≠ FX 타이밍" 어긋남. 해결: SkillHitEvent 에 `timestampTick` 포함, 클라가 보간 또는 지연 재생
- **attachTo 엔티티가 중간에 파괴**: `FxInstanceComponent::bFollowEntity = true` 인데 엔티티 파괴 → 다음 FxSimSystem 에서 엔티티 존재 확인 후 `bAutoKill` 적용
- **같은 스킬 연속 2 회 SpawnFx**: 인스턴스 풀 고갈 가능. 풀 용량 512 이상 권장
- **AnimEvent payload 크기**: char[48] 은 tag 로 충분하나, 더 복잡한 payload 가 필요하면 이벤트 ID → lookup 테이블로 전환
- **Delayed Sound 큐 Scene 교체 시 정리**: Scene 을 넘어 유지되면 사라진 scene 의 사운드 재생됨

## 단위 테스트

- **AssetLibrary roundtrip**: Load → Find → Resolve 가 동일 포인터
- **AnimEvent → FX 스폰**: 타임라인 0.5s 에 VFX_SPAWN 이벤트 → FxInstance 카운트 +1
- **Buff apply/remove**: 시각 FX 가 함께 스폰/파괴
- **NetworkPacket roundtrip**: PKT_FxSpawnEvent 직렬화 → 역직렬화 → SpawnFx 호출 → 동일 seed 로 결정적 결과

## 구현 순서

1. `FxAssetLibrary` 구현 (Load / PreloadDirectory / Find)
2. `CGameInstance::SpawnFx` Tier 1 API 추가
3. `CFxSystem` (Tier 2) 의 FxInstance 관리 + Tick
4. AudioLink 자동 재생 (delayed queue 포함)
5. Scene_InGame 에서 Preload
6. 5 스킬 × 3 FX = 약 15 개 `.fxg` 에셋 수동 작성 (JSON 직접)
7. SkillHook 샘플 (IreliaQHit) — B-10 전에 임시 핸들러로 시험
8. AnimationEvent VFX_SPAWN 핸들러 (C-3 전에 스텁)
9. BuffSystem FX attach/kill (B-11 전에 스텁)
10. Scene_Editor 에 FX Editor 탭 병합
11. Phase 4 네트워크 패킷 정의 후 서버-클라 동기 테스트

## 다음 문서

[10_DEBUG_TOOLS.md](10_DEBUG_TOOLS.md) — FX Debugger, DebugDraw, Replay, 프로파일러 연동.
