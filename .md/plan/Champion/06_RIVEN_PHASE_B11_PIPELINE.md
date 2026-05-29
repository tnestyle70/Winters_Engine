# Phase B-11 v2 (리븐) — 순수 ECS CreateECSChampion 도입 + winters binary 추출

**작성일**: 2026-04-28
**v2 갱신**: 2026-04-28 — 사용자 피드백 반영 ("Job system 해결됐고 순수 ECS CreateECSChampion 으로 진행하기로 했잖아"). v1 의 Scene 멤버 ModelRenderer 패턴 (가렌·제드·이렐리아·야스오·칼리스타 레거시) 답습 폐기.

**목표 (2 트랙)**:
1. **순수 ECS Champion 경로 도입** — `m_Riven` Scene 멤버 / `m_Riven.Update/Render` 호출 0. ECS 가 ModelRenderer 소유, ForEach 로 driving. Riven 1체로 패턴 검증 후 후속 챔프 일괄 마이그레이션.
2. **winters binary 자체 추출** — `.wmesh / .wskel / .wanim` 변환 + info 검증 학습.

**전제**:
- Phase 5-A JobSystem race 해결 완료 (사용자 확인). Set_JobSystem 활성, Scheduler 정상 동작.
- Phase B-9 자체 포맷 + B-10 가렌·제드 (레거시 패턴) 통과
- ★ **Reference implementation**: `Client/Private/Manager/Minion_Manager.cpp` L219-302, L339-444 — 미니언이 이미 "ECS 가 ModelRenderer unique_ptr 소유 + RenderComponent raw view + ForEach Tick/Render" 패턴으로 동작 중. **동일 패턴 챔프에 적용**.

**참조 코드**:
- `Client/Private/Manager/Minion_Manager.cpp:339-444` — Spawn_Minion (entity+renderer alloc+map 저장)
- `Client/Private/Manager/Minion_Manager.cpp:267-282` — Tick AnimUpdate (`ForEach<RenderComponent>`)
- `Client/Private/Manager/Minion_Manager.cpp:290-304` — Render (`ForEach<MinionStateComponent, RenderComponent, TransformComponent>`)
- `Engine/Public/ECS/Components/GameplayComponents.h:43-51` — RenderComponent (이미 ECS-friendly, 변경 X)

---

## 0. 자원 확인 ✅ (이미 추출 완료)

```
Client/Bin/Resource/Texture/Character/Riven/
├── riven.fbx                      ✓ 입력
├── riven.skl / riven.skn          ✓
├── riven_base_tx_cm.png           ✓ 바디 텍스처
├── animations/                    ✓ 26 anim
│   ├── riven_idle1/2/3.anm        riven_run.anm
│   ├── riven_attack1/2/3.anm      riven_crit.anm
│   ├── riven_spell1a/b/c.anm      ★ Q 3단
│   ├── riven_spell2.anm           ★ W
│   ├── riven_spell3.anm           ★ E
│   ├── riven_spell4a/b.anm        ★ R / R2
│   ├── riven_idle1_ult.anm        riven_run_ult.anm        ★ R 변신 후
│   └── riven_attack1_ult.anm      riven_attack2_ult.anm
└── particles/
    ├── riven_base_blood_splash.png  blast_nova_bunny_05.png
    ├── glove_rune_quad_02.png       exile_crimson_sword_profile_glow.png
    └── fbx/  (Phase G FX 메쉬용 — B-11 1차 미사용)
```

---

## 1. 핵심 설계 — 순수 ECS Champion 경로

### 1.1 v1 (레거시) vs v2 (순수 ECS)

| 항목 | v1 (가렌·제드 패턴, 폐기) | v2 (Riven, 신규) |
|------|--------------------------|--------------------|
| ModelRenderer 소유 | Scene 멤버 (`m_Riven`) | ECS map (`m_ChampionRenderers[entity]`) |
| Transform 소유 | Scene 멤버 (`m_RivenTransform`) | 100% ECS (`TransformComponent`) |
| EntityID 매핑 | `m_RivenEntity` 별도 멤버 | `m_PlayerEntity` 1개로 통합 |
| Anim Update | `m_Riven.Update(dt)` 직접 호출 | `ForEach<RenderComponent>` Tick |
| Render | `m_Riven.UpdateCamera/Transform/Render` 3 줄 직접 | `ForEach<ChampionComponent, RenderComponent, TransformComponent>` |
| Player binding | `m_pPlayerRenderer = &m_Riven` 포인터 | `m_pPlayerRenderer = m_World.Get<RenderComponent>(m_PlayerEntity).pRenderer` (캐시 후 사용) |
| Anim swap (R) | `m_Riven.PlayAnimationByName(...)` | 동일 — pRenderer 가 가리키는 ModelRenderer 에 호출 |
| Scene_InGame 변경 영역 | h 멤버 2 + cpp 12 곳 | h 멤버 1 (map) + cpp 4 곳 (Init/Shutdown/Tick/Render) |

### 1.2 미니언 패턴과 100% 동일

```cpp
// Minion_Manager 가 이미 하고 있는 것 (참고용):
auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
pRenderer->Init(pPath, L"Shaders/Mesh3D.hlsl");
EntityID id = m_pWorld->CreateEntity();
m_pWorld->AddComponent<TransformComponent>(id);
m_pWorld->AddComponent<RenderComponent>(id).pRenderer = pRenderer.get();   // raw view
m_mapRenderers[id] = std::move(pRenderer);                                  // ECS 소유
```

```cpp
// Tick:
m_pWorld->ForEach<RenderComponent>([dt](EntityID, RenderComponent& rc){
    if (!rc.pRenderer || !rc.bAnimated) return;
    rc.pRenderer->Update(dt);
});

// Render:
m_pWorld->ForEach<MinionStateComponent, RenderComponent, TransformComponent>(
    [&](EntityID, ..., RenderComponent& rc, TransformComponent& xform){
        if (!rc.bVisible || !rc.pRenderer) return;
        rc.pRenderer->UpdateCamera(matVP);
        rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
        rc.pRenderer->Render();
    });
```

→ Riven 도 똑같이. 단 `MinionStateComponent` 자리에 `ChampionComponent` 사용 (챔프만 순회하기 위함).

### 1.3 Scene_InGame 의 v1 잔재는 그대로 둠 (회귀 0)

이렐리아/야스오/칼리스타/사일러스/비에고/가렌/제드 = 기존 Scene 멤버 + 직접 호출 그대로 유지. Riven 만 새 ECS 경로로. 통과 후 후속 사이클 (B-11c) 에서 7 챔프 일괄 마이그레이션.

---

## 2. 6 레이어 매핑 (v2)

| Layer | 책임 | v2 변경 |
|-------|------|---------|
| 1. 자원 (RAII) | ECS owned `unique_ptr<ModelRenderer>` | **Scene 멤버 X**. `m_ChampionRenderers[entity]` 맵 1개 |
| 2. 상태 (ECS) | `ChampionComponent` + `RenderComponent` + `TransformComponent` + `RivenStateComponent` | 모두 ECS Component. R 토글/Q 스택/E 쉴드 전부 Component |
| 3. 정의 (Table) | `ChampionDef` 확장 (fbxPath/textures/scale/spawnPos) + Riven 행 + SkillTable 5 행 | ChampionDef 가 데이터 소스 — CreateECSChampion 이 그걸 보고 자동 Init |
| 4. 로직 (System) | 야스오 Q L2134 미러 ApplyLocalPrediction key swap + post-cast + castFrame hook + ApplyRivenHit + R 토글 | Player binding 만 m_pPlayerRenderer ← lookup 으로 변경 |
| 5. 연출 (FxPreset) | RivenFxPresets.h/.cpp + vcxproj | 변경 0 (FX 는 ECS 와 무관) |
| 6. 통합 (Scene) | OnEnter: `m_PlayerEntity = CreateECSChampion(RIVEN, Blue, {24,1,0})` 1줄 + `m_pPlayerRenderer = m_World.Get<RenderComponent>(m_PlayerEntity).pRenderer` 캐시. OnUpdate/OnRender: ForEach loop 1개씩 추가 (이미 있으면 ChampionComponent 필터 추가). BanPick 버튼 | **신설 함수 1개 + 멤버 1개 + 5 줄 수정**. v1 의 12 곳 수정 사라짐 |

> **순수 ECS 검증 체크**:
> - `m_Riven` / `m_RivenTransform` / `m_RivenEntity` 멤버 0 ✅
> - `m_Riven.Update(dt)` 호출 0 ✅
> - `m_Riven.Render()` 호출 0 ✅
> - Scene_InGame.cpp 신규 라인 < 50 ✅
> - 미니언 패턴 1:1 미러 ✅

---

## 3. 계단식 검증 마일스톤 (v2)

```
Stage 1 (Asset)            — wmesh/wskel/wanim 변환 + info 검증 (★ winters binary)
Stage 2 (ChampionDef 확장) — 필드 추가 (fbxPath/textures/spawnPos/spawnScale) + Riven 행
Stage 3 (CreateECSChampion) — Scene_InGame 신규 함수 (entity+renderer alloc+map)
Stage 4 (ECS Tick/Render 통합) — OnUpdate AnimUpdate ForEach + OnRender ForEach 추가
Stage 5 (Player binding 리팩터) — m_PlayerEntity → m_pPlayerRenderer 캐시
Stage 6 (BanPick 버튼)     — Riven 버튼
   ↓ F5 #1: 리븐 모델 (24,1,0) 표시 + idle/run + 카메라 follow + 회귀 7 챔프 0 영향
Stage 7 (RivenStateComponent) — GameplayComponents.h 에 struct 추가 (야스오 옆)
Stage 8 (SkillTable Riven 5 행) — ★ Q Conditional + 야스오 패턴 미러
Stage 9 (RivenFxPresets) — 신규 2 파일 + vcxproj
Stage 10 (Q key swap + post-cast + castFrame hook + ApplyRivenHit + R 토글) — 야스오 L2134/L2179/L1103 1:1 미러
   ↓ F5 #2: 리븐 풀 동작 + 회귀 7 챔프 0 영향
```

---

## 4. Stage 1 — winters binary 추출 ★

### 4.1 convert_all_assets.bat

`Tools/convert_all_assets.bat:31` Zed 다음 1줄:
```bat
call :convert_champ "Riven"   "riven.fbx"
```

### 4.2 변환 + 검증

```bat
cd Tools
convert_all_assets.bat champions
:: 기대: OK=8 FAIL=0

WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Riven\riven.wskel
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Riven\riven.wmesh
WintersAssetConverter.exe info Client\Bin\Resource\Texture\Character\Riven\anims\riven_idle1.wanim
```

| 검증 | 합격 |
|---|---|
| `wmesh.stride` == 76 | ✅ |
| `wmesh.bone_count` == `wskel.bone_count` | ✅ |
| `wanim.skel_hash` == `wskel.hash` | ✅ |

---

## 5. Stage 2 — ChampionDef 확장 + Riven 행 (Layer 3)

### 5.1 ChampionDef.h — 필드 추가 (★ 데이터 드리븐)

**before** (`Client/Public/GameObject/ChampionDef.h:4-12`):
```cpp
struct ChampionDef
{
    eChampion   id = eChampion::END;
    const char* animPrefix = "";
    const char* idleAnimKey = "idle1";
    const char* runAnimKey = "run";
    const char* basicAttackKey = "attack_01";
    f32_t basicAttackRange = 6.f;
};
```

**after**:
```cpp
struct ChampionDef
{
    eChampion   id = eChampion::END;
    const char* animPrefix = "";
    const char* idleAnimKey = "idle1";
    const char* runAnimKey = "run";
    const char* basicAttackKey = "attack_01";
    f32_t basicAttackRange = 6.f;

    // ★ Phase B-11 — CreateECSChampion 가 사용하는 데이터 소스
    //   nullptr 이면 v1 (Scene 멤버) 경로 호환 (기존 7 챔프 영향 0)
    const char*    fbxPath = nullptr;          // "Client/Bin/Resource/Texture/Character/Riven/riven.fbx"
    const wchar_t* shaderPath = L"Shaders/Mesh3D.hlsl";
    const wchar_t* texturePaths[4] = { nullptr, nullptr, nullptr, nullptr };  // mesh slot 0~3
    Vec3  spawnPosition = { 0.f, 1.f, 0.f };
    f32_t spawnScale = 0.01f;
};
```

→ 기존 7 챔프 행은 새 필드 default (`fbxPath=nullptr`) → v1 경로 강제. Riven 만 fbxPath 채움 → v2 경로 활성.

### 5.2 ChampionTable.cpp — Riven 행 (★ 사용자가 이미 추가함, 필드만 확장)

**현재** (사용자 수정):
```cpp
{ eChampion::RIVEN, "riven_", "idle1", "run", "attack1", 1.5f },
```

**after** (B-11 v2 — fbx/texture/spawn 추가):
```cpp
// ── Riven (★ Phase B-11 v2 — 순수 ECS 경로 활성화) ──
{
    eChampion::RIVEN, "riven_", "idle1", "run", "attack1", 1.5f,
    "Client/Bin/Resource/Texture/Character/Riven/riven.fbx",
    L"Shaders/Mesh3D.hlsl",
    {
        L"Client/Bin/Resource/Texture/Character/Riven/riven_base_tx_cm.png",
        nullptr, nullptr, nullptr,
    },
    { 24.f, 1.f, 0.f },   // Zed(21) 옆
    0.01f,
},
```

**검증**: `FindChampionDef(eChampion::RIVEN)->fbxPath != nullptr`. Stage 3 의 CreateECSChampion 진입 조건.

---

## 6. Stage 3 — Scene_InGame::CreateECSChampion 신규 함수 (Layer 1+2)

### 6.1 Scene_InGame.h — 멤버 1개 + 메서드 1개 추가 (총 2 줄)

**삽입 위치**: `Scene_InGame.h:285` `m_ZedEntity` 다음 (기존 v1 EntityID 들과 같은 영역).

```cpp
EntityID m_GarenEntity = NULL_ENTITY;
EntityID m_ZedEntity = NULL_ENTITY;
// ★ Phase B-11 v2 — Riven 은 별도 EntityID 멤버 X. m_PlayerEntity 만으로 충분.
//    챔프 ModelRenderer 는 ECS 소유 (Minion_Manager 패턴):
std::unordered_map<EntityID, std::unique_ptr<ModelRenderer>> m_ChampionRenderers;
```

private 메서드 영역 (`ApplyZedHit` 다음):
```cpp
void ApplyGarenHit(EntityID target, f32_t fDamage);
void ApplyZedHit(EntityID target, f32_t fDamage);
void ApplyRivenHit(EntityID target, f32_t fDamage);

// ★ Phase B-11 v2 — ChampionDef 데이터 드리븐 entity+renderer 생성
EntityID CreateECSChampion(eChampion id, eTeam team);
```

**include 추가** (`Scene_InGame.h` 상단):
```cpp
#include <unordered_map>
// ModelRenderer.h 는 이미 포함되어있음 (m_Irelia 등 멤버 때문)
```

### 6.2 Scene_InGame.cpp — CreateECSChampion 정의

**삽입 위치**: `Scene_InGame.cpp:464` `CreateChampionEntity` 다음, `CreateChampionEntity_FromBlueprint` 앞 (관련 함수끼리 모음).

```cpp
// ─────────────────────────────────────────────────────────────
// Phase B-11 v2 — 순수 ECS Champion 생성
//
//  Scene 멤버 ModelRenderer 0. ChampionDef.fbxPath 로 데이터 드리븐.
//  Reference: Minion_Manager::Spawn_Minion (L339-444) 동일 패턴.
// ─────────────────────────────────────────────────────────────
EntityID CScene_InGame::CreateECSChampion(eChampion id, eTeam team)
{
    const ChampionDef* cd = FindChampionDef(id);
    if (!cd || !cd->fbxPath)
    {
        char m[160];
        sprintf_s(m, "[CreateECSChampion] FAIL: ChampionDef missing or fbxPath null (id=%u)\n",
            static_cast<u32_t>(id));
        OutputDebugStringA(m);
        return NULL_ENTITY;
    }

    // 1) ModelRenderer 자체 alloc + Init + 텍스처 로드 (ECS 가 소유)
    auto pRenderer = std::unique_ptr<ModelRenderer>(new ModelRenderer());
    if (!pRenderer->Init(cd->fbxPath, cd->shaderPath))
    {
        char m[256];
        sprintf_s(m, "[CreateECSChampion] FAIL: ModelRenderer::Init for %s\n", cd->fbxPath);
        OutputDebugStringA(m);
        return NULL_ENTITY;
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
        if (cd->texturePaths[i])
            pRenderer->LoadMeshTexture(i, cd->texturePaths[i]);
    }
    // bind pose 탈출 — idle 즉시 기동 (가렌 학습 박제)
    {
        std::string idleFull = std::string(cd->animPrefix) + cd->idleAnimKey;
        pRenderer->PlayAnimationByName(idleFull, true);
    }

    // 2) Entity 생성 + ECS 컴포넌트 부착
    EntityID e = m_World.CreateEntity();

    // Transform — ChampionDef 의 spawn 좌표/스케일
    TransformComponent tf;
    tf.SetPosition(cd->spawnPosition);
    tf.SetScale(cd->spawnScale);
    m_World.AddComponent<TransformComponent>(e, tf);

    // Render — raw view (소유는 m_ChampionRenderers map)
    RenderComponent rc;
    rc.pRenderer = pRenderer.get();
    rc.bVisible  = true;
    rc.bAnimated = true;
    m_World.AddComponent<RenderComponent>(e, rc);

    // Champion 메타
    ChampionComponent cc;
    cc.id        = id;
    cc.team      = team;
    cc.hp        = 600.f;
    cc.maxHp     = 600.f;
    cc.mana      = 300.f;
    cc.maxMana   = 300.f;
    cc.moveSpeed = m_fPlayerSpeed;
    m_World.AddComponent<ChampionComponent>(e, cc);

    HealthComponent hp;
    hp.fCurrent = cc.hp;
    hp.fMaximum = cc.maxHp;
    hp.bIsDead  = false;
    m_World.AddComponent<HealthComponent>(e, hp);

    m_World.AddComponent<ServerIdComponent>(e);

    NavAgentComponent agent;
    agent.fSpeed        = m_fPlayerSpeed;
    agent.fArriveRadius = m_fArriveRadius;
    agent.bHasGoal      = false;
    agent.bPathDirty    = false;
    m_World.AddComponent<NavAgentComponent>(e, agent);

    m_World.AddComponent<VelocityComponent>(e);
    m_World.AddComponent<SkillStateComponent>(e);

    // Riven specific — RivenStateComponent (Stage 7 정의)
    if (id == eChampion::RIVEN)
        m_World.AddComponent<RivenStateComponent>(e);

    // 3) 소유권 이전 — map 에 박제 (Minion_Manager 패턴)
    m_ChampionRenderers[e] = std::move(pRenderer);

    {
        char m[160];
        sprintf_s(m, "[CreateECSChampion] OK id=%u entity=%u team=%u pos=(%.1f,%.1f,%.1f)\n",
            static_cast<u32_t>(id), static_cast<u32_t>(e), static_cast<u32_t>(team),
            cd->spawnPosition.x, cd->spawnPosition.y, cd->spawnPosition.z);
        OutputDebugStringA(m);
    }

    return e;
}
```

### 6.3 Scene_InGame.cpp::CreateECSEntities — Riven 1줄

**삽입 위치**: `Scene_InGame.cpp:533` Zed CreateChampionEntity 다음 (이번엔 v2 경로):
```cpp
m_ZedEntity = CreateChampionEntity(m_Zed, m_ZedTransform, eChampion::ZED, eTeam::Blue);

// ★ Phase B-11 v2 — 순수 ECS 경로 (Scene 멤버 X)
EntityID rivenEntity = CreateECSChampion(eChampion::RIVEN, eTeam::Blue);
```

**Player 분기** (`Scene_InGame.cpp:600` 영역):
```cpp
else if (champ == eChampion::GAREN) m_PlayerEntity = m_GarenEntity;
if (champ == eChampion::ZED)        m_PlayerEntity = m_ZedEntity;
if (champ == eChampion::RIVEN)      m_PlayerEntity = rivenEntity;   // ★ 로컬 변수
```

### 6.4 Scene_InGame.cpp::Shutdown — map clear

기존 `m_Riven.Shutdown()` 같은 줄 0. map clear 만 (`OnExit` 또는 destructor 영역):
```cpp
m_ChampionRenderers.clear();   // unique_ptr destructor → ModelRenderer::Shutdown 자동 호출
```

→ Scene_InGame.cpp 신규 라인 약 95 (CreateECSChampion 본문 75 + 기타 20). v1 의 12 곳 수정 (~50 줄) 대비 가독성 ↑ + 인프라 자산 1.

---

## 7. Stage 4 — ECS Tick / Render 통합 (Layer 6)

미니언 패턴 1:1 미러. 챔피언만 순회하기 위해 `ChampionComponent` 필터 추가.

### 7.1 OnUpdate — AnimUpdate ForEach

**삽입 위치**: `Scene_InGame.cpp:1291` 영역, 기존 `m_Irelia.Update(dt) ... m_Zed.Update(dt)` 7 줄 **다음** (병행, 회귀 0).

```cpp
m_Irelia.Update(dt);
m_Yasuo.Update(dt);
m_Sylas.Update(dt);
m_Viego.Update(dt);
m_Kalista.Update(dt);
m_Garen.Update(dt);
m_Zed.Update(dt);

// ★ Phase B-11 v2 — ECS owned ModelRenderer 들 anim update
//    Reference: Minion_Manager.cpp:267-282 동일 패턴
{
    WINTERS_PROFILE_SCOPE("Champion::AnimUpdate");
    m_World.ForEach<ChampionComponent, RenderComponent>(
        [dt](EntityID, ChampionComponent&, RenderComponent& rc)
        {
            if (!rc.pRenderer || !rc.bAnimated) return;
            if (!rc.pRenderer->HasSkeleton()) return;
            rc.pRenderer->Update(dt);
        });
}
```

> **주의 — 이중 Update 회피**: 기존 Scene 멤버 챔프 (`m_Irelia` 등) 도 `RenderComponent.pRenderer` 가 그들 주소 가리킴. ForEach 가 같은 ModelRenderer 를 두 번 Update → anim 2 배속. **방지**: v1 챔프는 `CreateChampionEntity` 시 RenderComponent 추가하지 않거나, ForEach 에서 Riven 만 필터. 가장 안전한 방법:
>
> ```cpp
> // CreateECSChampion 만 RenderComponent 추가, CreateChampionEntity (v1) 는 미추가.
> // → Scene_InGame.cpp:429-432 의 RenderComponent 추가 라인 v1 함수에서 제거.
> ```
>
> 또는 (덜 침습): RenderComponent 에 `bManagedByScene` 플래그 추가, v1 = true 로 ForEach skip.
>
> **권장**: v1 의 RenderComponent 부착 라인 (`Scene_InGame.cpp:429-432`) 주석 처리. v1 챔프는 anim/render 가 Scene 직접 호출이라 RenderComponent 가 어차피 unused (`pRenderer->Update` 호출 위치 0). 회귀 0.

### 7.2 OnRender — Render ForEach

**삽입 위치**: `Scene_InGame.cpp:1502` 영역, 기존 `m_X.UpdateCamera/UpdateTransform/Render` 21 줄 **다음**.

```cpp
m_Zed.UpdateCamera(vp);
m_Zed.UpdateTransform(m_ZedTransform.GetWorldMatrix());
m_Zed.Render();

// ★ Phase B-11 v2 — ECS owned 챔프 렌더
//    Reference: Minion_Manager.cpp:290-304 동일 패턴
{
    WINTERS_PROFILE_SCOPE("Champion::Render");
    m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
        [&](EntityID, ChampionComponent&, RenderComponent& rc, TransformComponent& xform)
        {
            if (!rc.bVisible || !rc.pRenderer) return;
            rc.pRenderer->UpdateCamera(vp);
            rc.pRenderer->UpdateTransform(xform.GetWorldMatrix());
            rc.pRenderer->Render();
        });
}
```

→ Riven 만 ForEach 통해 렌더. 7 v1 챔프는 위 21 줄로 직접 렌더 (RenderComponent 부착 안 했으므로 ForEach 진입 X).

---

## 8. Stage 5 — Player binding 리팩터 (Layer 6)

기존 player 분기는 `m_pPlayerRenderer = &m_Riven` 같은 직접 포인터. v2 는 entity 의 RenderComponent 에서 추출.

### 8.1 player 분기 변경

**before** (가렌·제드 패턴, `Scene_InGame.cpp:235-250`):
```cpp
else if (champ == eChampion::ZED)
{
    m_pPlayerRenderer = &m_Zed;
    m_pPlayerTransform = &m_ZedTransform;
    m_pPlayerIdleAnim = "zed_idle1";
    m_pPlayerRunAnim = "zed_run";
}
```

**after** (Riven 만 entity 기반):
```cpp
else if (champ == eChampion::ZED) { /* 기존 v1 그대로 */ }

else if (champ == eChampion::RIVEN)
{
    // m_PlayerEntity 가 CreateECSChampion 으로 이미 생성됨 (Stage 6.3)
    if (m_PlayerEntity != NULL_ENTITY
        && m_World.HasComponent<RenderComponent>(m_PlayerEntity)
        && m_World.HasComponent<TransformComponent>(m_PlayerEntity))
    {
        auto& rc = m_World.GetComponent<RenderComponent>(m_PlayerEntity);
        // m_pPlayerTransform 은 CTransform (레거시) 기준이라 직접 못 가리킴.
        //   카메라 follow 는 Stage 8.2 에서 entity TransformComponent 기준으로 갱신.
        m_pPlayerRenderer  = rc.pRenderer;     // raw view OK (수명은 m_ChampionRenderers 가 보장)
        m_pPlayerTransform = nullptr;          // ★ 의도적 — 야스오·가렌 체크하는 분기 다 걸림
        m_pPlayerIdleAnim  = "riven_idle1";
        m_pPlayerRunAnim   = "riven_run";
    }
}
```

### 8.2 카메라 follow — entity TransformComponent 기반

**삽입 위치**: `Scene_InGame.cpp:252-256` 영역 (기존 `m_pCamera->SetFollowTarget(m_pPlayerTransform)` 영역).

**before**:
```cpp
if (m_pPlayerTransform)
{
    m_pCamera->SetFollowTarget(m_pPlayerTransform);
}
```

**after**:
```cpp
if (m_pPlayerTransform)
{
    m_pCamera->SetFollowTarget(m_pPlayerTransform);
}
else if (m_PlayerEntity != NULL_ENTITY
    && m_World.HasComponent<TransformComponent>(m_PlayerEntity))
{
    // ★ Phase B-11 v2 — ECS Transform 기반 카메라 follow
    //   매 프레임 OnUpdate 에서 카메라에 entity 위치 push (CDynamicCamera 가 CTransform* 만 받으면
    //   adapter 필요. 1차는 OnUpdate 인라인으로 카메라 위치 직접 set)
    // 1차: SetFollowTarget 인터페이스 그대로 두고, OnUpdate 마다 m_World.Get<TransformComponent>(m_PlayerEntity).vWorldPosition
    //      를 카메라에 push. 또는 CDynamicCamera::SetFollowEntity(EntityID, CWorld*) 신설 (정도가 큼 — 보류).
}
```

→ **1차 단순화**: `CDynamicCamera::SetFollowTarget` 시그니처 변경 부담 큼. **OnUpdate 인라인 카메라 위치 push** 가 가장 적은 수정:

```cpp
// OnUpdate 적당한 위치 (m_pCamera 갱신 직전):
if (m_pPlayerTransform == nullptr
    && m_PlayerEntity != NULL_ENTITY
    && m_World.HasComponent<TransformComponent>(m_PlayerEntity))
{
    auto& tf = m_World.GetComponent<TransformComponent>(m_PlayerEntity);
    // CTransform 어댑터 캐시 — Scene 멤버 1개 추가
    m_PlayerEntityTransformCache.SetPosition(tf.GetWorldPosition());
    m_PlayerEntityTransformCache.SetRotation(tf.GetWorldRotation());
    m_PlayerEntityTransformCache.SetScale(tf.GetWorldScale());
    m_pCamera->SetFollowTarget(&m_PlayerEntityTransformCache);
}
```

`CTransform m_PlayerEntityTransformCache` 멤버 1개 추가 (`Scene_InGame.h`). 이게 v2 의 어댑터.

> **TODO (B-11b)**: `CDynamicCamera` 에 `SetFollowEntity(EntityID, CWorld*)` 신설하면 cache 멤버 제거. 1차는 어댑터로 진행.

---

## 9. Stage 6 — BanPick Riven 버튼 (Layer 6, v1 동일)

`Scene_BanPick.cpp:119` Zed 다음:
```cpp
ImGui::SameLine();
if (ImGui::Button("Riven", ImVec2(150.f, 60.f)))
{
    CGameInstance::Get()->Get_GameContext().SelectedChampion = eChampion::RIVEN;
    auto pLoadingMatch = CScene_MatchLoading::Create(
        []()->std::unique_ptr<IScene> {
            return std::unique_ptr<IScene>(new CScene_InGame());
        }, 3.f);
    CGameInstance::Get()->Change_Scene((uint32_t)eSceneID::MatchLoading,
        std::move(pLoadingMatch));
    ImGui::End();
    return;
}
```

---

## ⏸ F5 #1 검증 (Stage 1~6 후)

| 항목 | 합격 |
|------|------|
| 빌드 통과 | RenderComponent / ChampionComponent / Vec3 등 모두 includes 정상 |
| `[CreateECSChampion] OK id=9 entity=N team=0 pos=(24.0,1.0,0.0)` 로그 | ECS entity 정상 생성 |
| `[CModel] .wmesh+.wskel fast-path: ...riven.wmesh` | winters binary 로드 |
| 리븐 모델 (24,1,0) 표시 | TransformComponent 좌표 적용 |
| idle 무한 재생 | `PlayAnimationByName("riven_idle1", true)` |
| 우클릭 이동 → run | NavAgent + run anim (run anim 자동 swap 은 기존 입력 시스템이 m_pPlayerRenderer 호출) |
| 카메라 follow | adapter `m_PlayerEntityTransformCache` 동작 |
| **회귀 7 챔프 0 변화** | v1 경로 그대로 — Irelia/Yasuo/Sylas/Viego/Kalista/Garen/Zed 시각·동작 동일 |
| anim 2배속 0 | 위의 "이중 Update 회피" 가드 — v1 챔프는 RenderComponent 부착 안 함 |

**실패 패턴**:
| 증상 | 원인 |
|------|------|
| anim 2 배속 (Riven) | RenderComponent 가 m_Riven 도 가리킴 — v1 함수 RenderComponent 부착 라인 주석 누락 |
| 카메라 안 따라옴 | `m_PlayerEntityTransformCache` 멤버 추가 누락 또는 OnUpdate 갱신 누락 |
| `[CreateECSChampion] FAIL ChampionDef missing` | Stage 5.1/5.2 ChampionDef.fbxPath 추가 누락 |
| `[CModel] falling back to Assimp` | Stage 1 wskel 미생성 |

**1차 통과 = 순수 ECS Champion 패턴 검증 완료**. Riven Scene 멤버 0.

---

## 10. Stage 7 — RivenStateComponent (Layer 2 ★ 순수 ECS)

`Engine/Public/ECS/Components/GameplayComponents.h:130` `YasuoStateComponent` 다음:

```cpp
struct YasuoStateComponent
{
    uint8_t qStackCount = 0;
    f32_t qStackTimer = 0.f;
    bool bEActive = false;
    f32_t eActiveTimer = 0.f;
};

// ─────────────────────────────────────────────────────────────
//  RivenStateComponent — Phase B-11 v2 (★ 순수 ECS, 야스오 1:1 미러)
//   Q 3단: qStackCount/qStackTimer = 야스오와 동일 (필드명·의미 일치)
//   R 토글: bUlted/fUltTimer (Riven specific)
//   E 쉴드: fShieldRemaining/fShieldTimer (Riven specific)
//
//  접근: Tier 2 ECS 직접 — Scene_InGame::OnUpdate / ApplyLocalPrediction 에서 read/write.
// ─────────────────────────────────────────────────────────────
struct RivenStateComponent
{
    // Q — Broken Wings (야스오 Q 1:1 미러)
    uint8_t qStackCount = 0;
    f32_t   qStackTimer = 0.f;

    // R — Blade of the Exile
    bool  bUlted = false;
    f32_t fUltTimer = 0.f;

    // E — Valor (Dash + Shield)
    f32_t fShieldRemaining = 0.f;
    f32_t fShieldTimer = 0.f;
};
```

CreateECSChampion 안에서 이미 `if (id == eChampion::RIVEN) m_World.AddComponent<RivenStateComponent>(e);` 추가됨 (Stage 6.2).

**vcxproj 작업 0** — 기존 헤더 수정.

---

## 11. Stage 8 — SkillTable Riven 5 행 (Layer 3 — v1 과 동일)

★ Q 는 `eTargetMode::Conditional` + key `"spell1"` (야스오 L102 미러). 정확한 코드 블록은 v1 §9 참조 (변경 0). 요약:

```cpp
// BA: "attack1" / 평타 1.5s rang
// Q : Conditional + "spell1" / 1.5s 쿨 (스택 윈도우)
// W : Self + "spell2"
// E : Self + "spell3"
// R : Self + "spell4a"
```

`SkillTable.cpp:262` Zed R 다음 5 행 추가. v1 의 §9.1 코드 블록 그대로.

---

## 12. Stage 9 — RivenFxPresets (Layer 5 — v1 과 동일)

신규 2 파일 + vcxproj 등록. v1 의 §10 참조 (변경 0).

- `Client/Public/GameObject/Champion/Riven/RivenFxPresets.h`
- `Client/Private/GameObject/Champion/Riven/RivenFxPresets.cpp`
- `Client/Include/Client.vcxproj` + `.vcxproj.filters` 등록

---

## 13. Stage 10 — Conditional key swap + castFrame hook + R 토글 + ApplyRivenHit (Layer 4·6)

야스오 L2134-2152 / L2179-2231 / L1103-1109 1:1 미러. 단 `m_Riven.PlayAnimationByName` 같은 Scene 멤버 호출은 `m_pPlayerRenderer->PlayAnimationByName` 으로 변경 (player binding 캐시).

### 13.1 ApplyLocalPrediction Q key swap

**삽입 위치**: `Scene_InGame.cpp:2152` 야스오 if 블록 닫는 `}` 다음 (v1 §11.2 동일).
```cpp
if (def.champ == eChampion::RIVEN
    && def.slot == static_cast<uint8_t>(eSkillSlot::Q)
    && m_World.HasComponent<RivenStateComponent>(m_PlayerEntity))
{
    const auto& rs = m_World.GetComponent<RivenStateComponent>(m_PlayerEntity);
    if (rs.qStackCount >= 2)      key = "spell1c";
    else if (rs.qStackCount == 1) key = "spell1b";
    else                          key = "spell1a";

    char dbg[96]{};
    sprintf_s(dbg, "[Riven Q Anim] stack=%u key=%s\n",
        static_cast<u32_t>(rs.qStackCount), key.c_str());
    ::OutputDebugStringA(dbg);
}
```

→ `cd->animPrefix + key` = `"riven_" + "spell1a/b/c"` ✅ (animPrefix="riven_" 사용).

### 13.2 ApplyLocalPrediction post-cast

**삽입 위치**: `Scene_InGame.cpp:2233` 야스오 Q 블록 다음 (v1 §11.3 동일).

```cpp
if (def.champ == eChampion::RIVEN
    && m_World.HasComponent<RivenStateComponent>(m_PlayerEntity))
{
    auto& rs = m_World.GetComponent<RivenStateComponent>(m_PlayerEntity);
    if (def.slot == static_cast<uint8_t>(eSkillSlot::Q))
    {
        const uint8_t stackIdx = rs.qStackCount;
        RivenFx::SpawnQSlash(m_World, m_PlayerEntity, stackIdx, 0.4f);

        const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
        if (target != NULL_ENTITY)
            ApplyRivenHit(target, 40.f + 15.f * stackIdx);

        if (stackIdx >= 2) { rs.qStackCount = 0; rs.qStackTimer = 0.f; }
        else               { rs.qStackCount += 1; rs.qStackTimer = 1.5f; }
    }
}
```

### 13.3 castFrame hook — BA / W / E / R

**삽입 위치**: `Scene_InGame.cpp:937` ZED 분기 다음 (v1 §11.4 동일, 단 `m_Riven` → `m_pPlayerRenderer` 로 변경).

```cpp
else if (champCur == eChampion::RIVEN && m_pActiveSkillDef)
{
    const i32_t slot = m_pActiveSkillDef->slot;
    auto* pRivenState = m_World.HasComponent<RivenStateComponent>(m_PlayerEntity)
        ? &m_World.GetComponent<RivenStateComponent>(m_PlayerEntity) : nullptr;

    if (slot == 0)
    {
        const EntityID target = m_ActiveSkillCommandStorage.targetEntityId;
        f32_t dmg = (pRivenState && pRivenState->bUlted) ? 75.f : 50.f;
        if (target != NULL_ENTITY) ApplyRivenHit(target, dmg);
    }
    else if (slot == 2)
    {
        RivenFx::SpawnWNova(m_World, m_PlayerEntity, 0.6f);
    }
    else if (slot == 3)
    {
        RivenFx::SpawnEShield(m_World, m_PlayerEntity, 1.5f);
        if (pRivenState) { pRivenState->fShieldRemaining = 70.f; pRivenState->fShieldTimer = 1.5f; }
    }
    else if (slot == 4)
    {
        RivenFx::SpawnRActivate(m_World, m_PlayerEntity, 0.8f);
        if (pRivenState) { pRivenState->bUlted = true; pRivenState->fUltTimer = 15.f; }

        // ★ R 변신 anim swap — m_pPlayerRenderer (entity 기반 캐시) 사용
        if (m_pPlayerRenderer)
        {
            m_pPlayerIdleAnim = "riven_idle1_ult";
            m_pPlayerRunAnim  = "riven_run_ult";
            m_pPlayerRenderer->PlayAnimationByName("riven_idle1_ult", true);
        }
    }
}
```

### 13.4 OnUpdate Riven 타이머 (★ 야스오 L1103 미러)

**삽입 위치**: `Scene_InGame.cpp:1109` 야스오 qStackTimer 블록 다음. 단 EntityID 는 `m_PlayerEntity` (v2 는 `m_RivenEntity` 멤버 X).

```cpp
// Phase B-11 v2 — Riven 타이머 (★ 야스오 패턴 미러 + R/쉴드)
if (m_PlayerEntity != NULL_ENTITY
    && m_World.HasComponent<RivenStateComponent>(m_PlayerEntity))
{
    auto& rs = m_World.GetComponent<RivenStateComponent>(m_PlayerEntity);

    // Q 스택 감쇠 (야스오와 동일)
    if (rs.qStackTimer > 0.f)
    {
        rs.qStackTimer -= dt;
        if (rs.qStackTimer <= 0.f) rs.qStackCount = 0;
    }

    // R 변신 만료
    if (rs.bUlted)
    {
        rs.fUltTimer -= dt;
        if (rs.fUltTimer <= 0.f)
        {
            rs.bUlted = false;
            rs.fUltTimer = 0.f;
            if (m_pPlayerRenderer)
            {
                m_pPlayerIdleAnim = "riven_idle1";
                m_pPlayerRunAnim  = "riven_run";
                m_pPlayerRenderer->PlayAnimationByName("riven_idle1", true);
            }
        }
    }

    // 쉴드 만료
    if (rs.fShieldTimer > 0.f)
    {
        rs.fShieldTimer -= dt;
        if (rs.fShieldTimer <= 0.f) { rs.fShieldRemaining = 0.f; rs.fShieldTimer = 0.f; }
    }
}
```

### 13.5 ApplyRivenHit (v1 §11.6 동일)

`Scene_InGame.cpp:2720` ApplyZedHit 다음 함수 정의 (변경 0):
```cpp
void CScene_InGame::ApplyRivenHit(EntityID target, f32_t fDamage)
{
    if (target == NULL_ENTITY || target == m_PlayerEntity) return;
    if (!m_World.HasComponent<ChampionComponent>(target)) return;

    auto& champion = m_World.GetComponent<ChampionComponent>(target);
    if (champion.team == m_PlayerTeam) return;

    champion.hp = (champion.hp > fDamage) ? (champion.hp - fDamage) : 0.f;

    f32_t hpCur = champion.hp, hpMax = champion.maxHp;
    if (m_World.HasComponent<HealthComponent>(target))
    {
        auto& hp = m_World.GetComponent<HealthComponent>(target);
        hp.fCurrent = champion.hp;
        hp.fMaximum = champion.maxHp;
        hp.bIsDead = (hp.fCurrent <= 0.f);
        hpCur = hp.fCurrent; hpMax = hp.fMaximum;
    }

    char buf[128];
    sprintf_s(buf, "[RivenHit] target=%u dmg=%.1f hp=%.1f/%.1f\n",
        static_cast<u32_t>(target), fDamage, hpCur, hpMax);
    OutputDebugStringA(buf);
}
```

---

## ⏸ F5 #2 검증 (Stage 7~10 후)

| 항목 | 합격 |
|------|------|
| BA | 우클릭 → `riven_attack1` + `[RivenHit] dmg=50` (R 활성 시 75) |
| Q (1번) | `[Riven Q Anim] stack=0 key=spell1a` + `riven_spell1a` + 흰색 + `dmg=40` |
| Q (2번) | `[Riven Q Anim] stack=1 key=spell1b` + `riven_spell1b` + 황색, `dmg=55` |
| Q (3번) | `[Riven Q Anim] stack=2 key=spell1c` + `riven_spell1c` 도약 + 적색, `dmg=70`, stack→0 |
| Q (1.5s 대기) | OnUpdate qStackTimer → qStackCount=0 → 다음 Q `spell1a` |
| W | `riven_spell2` + 보라색 nova |
| E | `riven_spell3` + 청색 쉴드 + RivenStateComponent.fShieldTimer=1.5 |
| R | `riven_spell4a` + 적색 검 + idle/run anim → `_ult` swap + 15s 후 자동 복귀 |
| R 후 BA | dmg=75 |
| 회귀 7 챔프 | 시각 0 변화 |
| **순수 ECS 검증** | `m_Riven` / `m_RivenTransform` / `m_RivenEntity` Scene 멤버 검색 결과 0 |

---

## 14. 사이클 종료 후 갱신할 파일

1. **CLAUDE.md** L11-18 — 진행/다음 갱신 (B-11 v2 완료 → B-11c 7 챔프 일괄 ECS 마이그레이션 또는 B-12 메쉬 분리)
2. **CLAUDE.md** Phase B-11 v2 Gotchas:
   - **R-G1**: v1 챔프 (`m_Irelia` 등) 의 RenderComponent 부착 라인 (`Scene_InGame.cpp:429-432`) 주석 처리 필수 — 누락 시 anim 2 배속 (ForEach + Scene 직접 호출 이중)
   - **R-G2**: ChampionDef.fbxPath = nullptr 가드로 v1/v2 경로 분기. 신규 챔프는 fbxPath 채워서 v2 경로 진입
   - **R-G3**: Player 카메라 follow — CTransform 어댑터 (`m_PlayerEntityTransformCache`) 매 프레임 갱신. CDynamicCamera::SetFollowEntity 신설은 B-11c
3. **MEMORY.md** + 신규 메모 `project_phase_b11_riven_ecs.md`
4. **본 계획서** — 사이클 종료 후 학습 결과 부록
5. **`.md/architecture/CLASS_DAY8_VS_WINTERS.md`** — Champion ECS 경로 박제

---

## 15. 예상 소요

| 단계 | 시간 |
|---|---|
| Stage 1 (변환 + info 검증) | 5분 |
| Stage 2 (ChampionDef 확장 + Riven 행) | 3분 |
| Stage 3 (CreateECSChampion 신규) | 12분 |
| Stage 4 (ECS Tick/Render ForEach + v1 RenderComponent 가드) | 5분 |
| Stage 5 (Player binding adapter) | 5분 |
| Stage 6 (BanPick) | 1분 |
| F5 #1 검증 + 디버깅 | 10분 |
| Stage 7 (RivenStateComponent) | 2분 |
| Stage 8 (SkillTable 5 행) | 5분 |
| Stage 9 (RivenFxPresets + vcxproj) | 8분 |
| Stage 10 (Q swap + post-cast + hook + 타이머 + ApplyRivenHit) | 12분 |
| F5 #2 검증 | 8분 |
| 회귀 7 챔프 검증 | 5분 |
| 메모/CLAUDE 박제 | 5분 |
| **합계** | **~86분** |

(v1 64분 대비 +22분 — CreateECSChampion + adapter + 가드 추가, 한 번 만들면 후속 챔프는 ChampionTable 1행 + BanPick 1줄 = 2분/챔프)

---

## 16. 학습 체크리스트 (★ 6 레이어 + ECS 즉답)

| Q | 모범 답안 |
|---|----------|
| `CreateECSChampion` 가 미니언 패턴과 어떻게 같나? | (1) `unique_ptr<ModelRenderer>` ECS 가 소유 (2) `RenderComponent.pRenderer = .get()` raw view (3) `unordered_map<EntityID, unique_ptr<ModelRenderer>>` 에 박제 — Minion_Manager.cpp:339-444 와 동일 |
| 왜 v1 챔프는 RenderComponent 부착 라인을 주석해야 하나? | OnUpdate 의 `ForEach<ChampionComponent, RenderComponent>` 가 같은 ModelRenderer 를 순회 → Scene 직접 `m_X.Update(dt)` 와 이중 호출 → anim 2 배속. v1 은 RenderComponent 없으면 ForEach 진입 X |
| ChampionDef 확장 (fbxPath/textures/spawnPos) 이유? | CreateECSChampion 이 데이터만 보고 entity 생성 가능 = 신규 챔프 = ChampionTable 1행 + BanPick 1줄. 코드 변경 0 |
| RivenStateComponent 가 ECS 인 이유 (Q stack/R 토글/쉴드)? | 시스템에서 일괄 순회 + 직렬화 (Phase 4 네트워크) + 인스턴스별 상태. POD 라 zero-init 안전. 야스오와 100% 동일 |
| 카메라 follow 어댑터 (`m_PlayerEntityTransformCache`) 가 잠시 필요한 이유? | CDynamicCamera::SetFollowTarget(CTransform*) 시그니처 = 레거시. ECS Transform 직접 못 받음. B-11c 에서 SetFollowEntity 추가 시 어댑터 제거 |
| 야스오 Q 패턴 vs Riven Q 차이 1가지? | 야스오는 `bEActive` (E 시전 중 EQ 분기) 추가 필드. Riven 은 E-Q 콤보 없어서 if 분기 1개 적음 |
| `.wmesh / .wskel / .wanim` 의 책임 분담? | wmesh=정점/IL/머티리얼, wskel=bone DFS+rest+offset+GlobalInverseRoot, wanim=tick 단위 키프레임+skel_hash trailer. 셋 다 정합 (skel_hash 매칭) 시 fast-path |

---

## 17. 의존 그래프 (B-11 v2 전후)

```
B-9 wmesh/wskel/wanim 인프라 ──┐
B-10 가렌·제드 (Scene 멤버 v1) ─┤
미니언 ECS 패턴 (Minion_Manager) ──┴─→ B-11 v2 Riven (★ 순수 ECS Champion 경로 도입) ─┐
                                                                                     │
                        ┌────────────────────────────────────────────────────────────┘
                        │
                        ├─→ B-11b Riven 본격 (부채꼴 영역 / 대시 / R2 wind)
                        │
                        ├─→ B-11c 7 챔프 일괄 ECS 마이그레이션 (Irelia/Yasuo/Kalista/Sylas/Viego/Garen/Zed)
                        │     — Scene 멤버 7쌍 + Update/Render 14 줄 + EntityID 7개 전부 제거
                        │     — 챔프당 ChampionTable.cpp 1행 fbxPath/textures 채움
                        │
                        └─→ B-12 메쉬 분리 (요네 R / 엘든링 보스)
```

---

## 한 줄

**v2 = Minion_Manager 1:1 미러 + ChampionDef 데이터 드리븐 CreateECSChampion + ForEach Tick/Render. Scene 멤버 0 / `m_Riven.X()` 호출 0 / EntityID 별도 멤버 0. v1 챔프 RenderComponent 부착 가드만 신경. F5 #1 (모델+follow) → Q Conditional + R 토글 → F5 #2. 86분 목표. 통과 = 후속 챔프 30초/체 (ChampionTable 1행) + B-11c 7 챔프 일괄 마이그레이션 가능.**
