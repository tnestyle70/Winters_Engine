# 수업 8일차 vs Winters — 전수 비교 + 컨벤션 정렬 계획

> **목적**: 2026-04 수업 8일차 (`C:\Users\user\Downloads\8일차\`) 의 Prototype/Layer/Object/Level 4개 매니저 시스템이 Winters 엔진 ECS/Scene 체계와 어떻게 **겹치고/다르고/보완되는지** 전수 매핑. 각 항목에 대해 (A) 흡수 / (B) 미도입 / (C) 조건부 흡수 3개 분류 결정 + 필요 시 구현 경로.
>
> **배경 메모리**: `project_session_2026_04_22.md` 에 "수업 Prototype→EntityBlueprint 흡수" 이미 결정됨. 본 문서는 그 결정을 확정하고 추가 흡수/거절 항목을 체계화.

---

## 1. 수업 8일차 전체 구조 요약

```
CMainApp                                                       (Client EXE 엔트리)
  └─ CGameInstance (Singleton, Engine facade)
       ├─ CGraphic_Device      (DX11 디바이스/컨텍스트/스왑체인)
       ├─ CTimer_Manager        (고해상도 타이머 맵)
       ├─ CLevel_Manager        (단일 current CLevel)
       │    └─ CLevel (abstract) : Initialize/Update/Render
       └─ CPrototype_Manager    (map<tag, unique_ptr<CPrototype>>[levels])
            └─ CPrototype (abstract) : Initialize_Prototype/Initialize(void*)/Clone(void*)
                 └─ CGameObject : CPrototype + Priority_Update/Update/Late_Update/Render
                      └─ CBackGround : CGameObject (Client)

[별도]
CObject_Manager   (map<tag, shared_ptr<CLayer>>[levels])
  └─ CLayer       (list<shared_ptr<CGameObject>>)
       └─ cascade Priority_Update / Update / Late_Update

CLoader           (_beginthreadex + CRITICAL_SECTION, 레벨별 Ready_For_Level_XXX)
```

**핵심 패턴 3개**:
1. **2-단계 Prototype 초기화**: `Initialize_Prototype()` (등록 시 1회) + `Initialize(void* pArg)` (Clone 시 per-instance)
2. **Level-Layer-Object 트리 관리**: Level 진입 시 `Object_Manager` 의 해당 Level 슬롯에 Layer 추가 → Layer 가 GameObject 리스트 소유
3. **백그라운드 로딩**: `_beginthreadex` 로 다음 레벨 에셋 로드, `Finished` 플래그로 완료 감지

---

## 2. Winters 현행 대응표

| 수업 8일차 | Winters 현행 | 관계 | 결론 |
|---|---|---|---|
| `CGameInstance` | `CGameInstance` ([GameInstance.h](../../../Engine/Include/GameInstance.h)) | ✅ 동일 역할 (Tier-1 포워딩) | 유지 |
| `CPrototype` (abstract, 상속 기반) | `CEntityBlueprint` ([EntityBlueprint.h](../../../Engine/Public/ECS/Systems/EntityBlueprint.h)) composition 기반 | ⚠️ **개념 흡수, 방식 상이** | 2026-04-22 결정. Composition 유지. |
| `CPrototype_Manager` (level 별 map) | `CEntityBlueprintRegistry` (scene 별 map) | ✅ 동일 역할 | 유지 |
| `CGameObject` (abstract, 상속 기반 GO) | ECS Entity + Components (`CWorld` + `ComponentStore`) | ❌ **설계 철학 상충** | **미도입** (CLAUDE.md §183 명문화) |
| `CLayer` (`list<shared_ptr<CGameObject>>`) | `CWorld::ForEach<T1,T2,T3>` (SoA) | ❌ 설계 철학 상충 | **미도입** |
| `CObject_Manager` (level×layer 이중 맵) | `CWorld` 단일 (씬마다 소유) | ❌ 설계 철학 상충 | **미도입** |
| `CLevel` (abstract) | `IScene` ([IScene.h](../../../Engine/Include/IScene.h)) | ✅ 동일 역할 (Winters 가 OnImGui/OnLateUpdate 더 풍부) | 유지 |
| `CLevel_Manager` | `CScene_Manager` ([Scene_Manager.h](../../../Engine/Public/Scene/Scene_Manager.h)) | ✅ 동일 역할 | 유지 |
| `CLoader` (백그라운드 thread 로더) | **없음** (현재 `OnEnter` 3초 동기 블로킹) | 🟢 **흡수 가치 높음** | **조건부 흡수** (Part C `.wmesh` 완성 후) |
| `CGraphic_Device` | `CDX11Device` + `CEngineApp` | ✅ Winters 가 더 세분화 | 유지 |
| `ENGINE_DESC` (hWnd/Size/NumLevels) | `EngineConfig` ([EngineConfig.h](../../../Engine/Public/EngineConfig.h)) | ✅ 동일 역할 | 유지 |
| `CTimer_Manager` | `CTimer_Manager` ([Timer_Manager.h](../../../Engine/Public/Core/Timer_Manager.h)) | ✅ 동일 | 유지 |
| `void* pArg` (Clone 인자) | **없음** (Installer 함수에 캡처로 전달) | ⚠️ 학원 방식 타입 안전성 낮음 | **조건부 흡수** (typed) |

---

## 3. 분류별 상세 분석

### 3.A. 완전 일치 (변경 불필요)

이미 Winters 가 동일/우월한 대응을 보유. 추가 작업 없음.

- `CGameInstance` ✅
- `CTimer_Manager` ✅
- `CGraphic_Device` → `CDX11Device` ✅
- `CLevel` → `IScene` ✅
- `CLevel_Manager` → `CScene_Manager` ✅
- `ENGINE_DESC` → `EngineConfig` ✅

### 3.B. 흡수 완료 (2026-04-22 결정)

- **`CPrototype` → `CEntityBlueprint` composition 방식 채택**
  - 상속 계층 대신 `Installer = std::function<void(CWorld&, EntityID)>` 벡터
  - Add(Installer) 체이닝 + Spawn(world) 로 컴포넌트 주입
  - 장점: 상속 깊이 고정 안 됨, Entity 마다 조합 자유로움
  - 근거: ECS 철학과 일치, boilerplate 최소화

### 3.C. 명시적 미도입 (CLAUDE.md §183 명문화됨)

Winters 의 핵심 설계 결정 — 수업 방식과 철학적으로 상충. 흡수 불가.

#### 3.C.1. `CGameObject` 상속 계층 미도입
**수업 방식**:
```cpp
class CGameObject abstract : public CPrototype
{
    virtual void Priority_Update(f32_t dt);
    virtual void Update(f32_t dt);
    virtual void Late_Update(f32_t dt);
    virtual HRESULT Render();
};
class CBackGround final : public CGameObject { /* virtual override 4개 */ };
```

**Winters ECS**:
```cpp
EntityID e = world.Create();
world.Add<TransformComponent>(e, { /* pos */ });
world.Add<RenderComponent>(e, { /* renderer ptr */ });
world.Add<MinionStateComponent>(e, { /* state */ });
// 업데이트는 시스템이:
world.ForEach<TransformComponent, RenderComponent>(...)
```

**근거**: 
- 학원 `CGameObject + multimap<Component*>` 은 AoS (각 GO 가 자기 컴포넌트 소유) → 캐시 미스, 수직 탐색 O(N×M).
- ECS 는 SoA (컴포넌트 타입별 연속 배열) → SIMD 친화, `ForEach<A,B>` 가 교집합 순회.
- 150 챔프 타겟 + 미니언 60마리 + 구조물 30개 스케일에서 수업 방식은 성능 병목. 이미 [MinionAISystem.cpp:157](../../../Engine/Private/ECS/Systems/MinionAISystem.cpp:157) `FindClosestEnemy` O(N²) 조사 착수.

**결론**: **미도입 확정.** 학원 CGameObject 상속 패턴을 Winters 에 도입하지 않는다.

#### 3.C.2. `CLayer` + `CObject_Manager` 이중 맵 미도입
**수업 방식**: `map<tag, shared_ptr<CLayer>>[numLevels]` — 레벨마다 태그→레이어 맵, 레이어는 `list<shared_ptr<CGameObject>>`.

**Winters 방식**: `CWorld` 가 모든 엔티티 소유, `ComponentStore<T>` 가 타입별 배열. 태그 대신 **컴포넌트 존재 여부** 로 쿼리.

**근거**:
- 학원 Layer-태그 시스템은 "Player", "Monster", "UI" 같이 카테고리 나눌 때 유용. Winters 는 `TagComponent` 또는 전용 컴포넌트 (`MinionComponent`, `ChampionComponent`) 가 같은 역할.
- 이중 맵 순회는 cache-hostile. ECS 가 성능/메모리 전 지표 상위.

**결론**: **미도입 확정.**

### 3.D. 조건부 흡수 — 구체 경로 설계

이 항목들은 Winters 가 빈틈 있고 학원 방식이 해답을 제공. 단 학원 그대로 옮기지 않고 **Winters 스타일로 현대화**.

#### 3.D.1. ★ 2-단계 초기화 (Initialize_Prototype / Initialize(void* pArg))

**수업 개념**:
- `Initialize_Prototype()`: 원본 등록 시 1회, 불변 리소스 (메시/셰이더/사운드) 로드
- `Initialize(void* pArg)`: Clone 시마다 per-instance 초기 위치/스탯 주입

**Winters 현재**:
`CEntityBlueprint::Spawn(CWorld& world)` — 인자 없음. Installer 람다에 스폰 위치 등을 캡처하면 동일 위치에 여러 번 Spawn 시 전부 같은 자리에 생성 → 용도 제한.

**흡수 제안**:
```cpp
// Engine/Public/ECS/Systems/EntityBlueprint.h  확장
class WINTERS_ENGINE CEntityBlueprint
{
public:
    using Installer     = std::function<void(CWorld&, EntityID)>;            // 원본 초기화 (기존)
    using ArgsInstaller = std::function<void(CWorld&, EntityID, const void*)>; // per-instance (신규)

    CEntityBlueprint& Add(Installer fn);
    CEntityBlueprint& AddArgs(ArgsInstaller fn);    // ★ 신규

    EntityID Spawn(CWorld& world) const;                            // 기존 — pArg=nullptr
    EntityID Spawn(CWorld& world, const void* pArg) const;          // ★ 신규

private:
    std::vector<Installer>     m_vecInstallers;
    std::vector<ArgsInstaller> m_vecArgsInstallers;
};
```

**타입 안전 버전 (추천)**:
```cpp
template<typename TArgs>
class CEntityBlueprintT
{
public:
    using Installer     = std::function<void(CWorld&, EntityID)>;
    using ArgsInstaller = std::function<void(CWorld&, EntityID, const TArgs&)>;

    EntityID Spawn(CWorld& world, const TArgs& args) const;
};
```
- `void*` 대신 템플릿 `TArgs` — Winters 컨벤션 (타입 안전 > 유연성).
- 미니언 스폰에서 `MinionSpawnArgs{ team, lane, pos }` 처럼 사용.

**사용 예**:
```cpp
// 등록 시 (Loader 에서 1회)
CEntityBlueprint bp;
bp.Add([](CWorld& w, EntityID e) {           // ← Initialize_Prototype 등가
    w.Add<RenderComponent>(e, { .pRenderer = g_pSharedMinionRenderer });
    w.Add<HealthComponent>(e, { .fMax = 100.f, .fCurrent = 100.f });
});
bp.AddArgs([](CWorld& w, EntityID e, const void* pArg) {   // ← Initialize(pArg) 등가
    const auto& s = *static_cast<const MinionSpawnArgs*>(pArg);
    w.Add<TransformComponent>(e, { .pos = s.vStart });
    w.Add<MinionStateComponent>(e, { .team = s.team, .lane = s.lane });
});
CGameInstance::Get()->Add_Blueprint(sceneId, L"Minion_Melee_Blue", std::move(bp));

// Clone 시 (매번)
MinionSpawnArgs args{ .vStart = lanePos, .team = Blue, .lane = Top };
CGameInstance::Get()->Clone_Entity(sceneId, L"Minion_Melee_Blue", world, &args);
```

**우선순위**: 중 (미니언 스폰 스케일 커지면 필요). **Phase B-7b (ChampionSpawnSystem + MinionSpawnSystem)** 에 묶어 진행.

#### 3.D.2. ★ CLoader — 비동기 로딩 흡수 (Part C `.wmesh` 이후)

**수업 구조**:
```cpp
class CLoader {
    _beginthreadex(..., ThreadMain, this, ...);
    // 스레드 내부:
    //   EnterCriticalSection
    //   Ready_For_Level_Logo() → Add_Prototype(BackGround, ...)
    //   LeaveCriticalSection
    //   m_isFinished = true
    bool Finished() const { return m_isFinished; }
};
class CLevel_Loading : public CLevel {
    void Update(f32_t dt) override {
        if (GetKeyState(VK_RETURN) && m_pLoader->Finished())
            CGameInstance::Change_Level(target);
    }
};
```

**Winters 현재**: `Scene_InGame::OnEnter` 3초 블로킹 — 5챔프 + 맵 동기 로드.

**흡수 제안 — Winters 스타일 현대화**:

```cpp
// Engine/Public/Resource/AsyncLoader.h  신규
#include "Core/JobSystem.h"
#include <atomic>
#include <future>

class WINTERS_ENGINE CAsyncLoader
{
public:
    static std::unique_ptr<CAsyncLoader> Create(CJobSystem* pJobSystem);

    // 다음 씬 리소스 프리로드 — 내부 JobSystem 으로 병렬 디스패치
    void Begin_Preload(std::function<void()> job);

    bool IsFinished() const { return m_bFinished.load(std::memory_order_acquire); }
    f32_t GetProgress01() const { return m_fProgress.load(); }    // UI 게이지용
    const char* GetCurrentStep() const { return m_pCurrentStep.load(); }

    void SetProgress(f32_t v) { m_fProgress.store(v); }
    void SetStep(const char* p) { m_pCurrentStep.store(p); }

private:
    CJobSystem*               m_pJobSystem = nullptr;
    std::atomic<bool>         m_bFinished{false};
    std::atomic<f32_t>        m_fProgress{0.f};
    std::atomic<const char*>  m_pCurrentStep{"Idle"};
};
```

**대체 포인트**:
- `_beginthreadex` → `CJobSystem::Submit` 사용 (Phase 5-B 수정 후)
- `CRITICAL_SECTION` → 진행률/스텝은 `std::atomic` 만 쓰고 공유 데이터는 잡 내부 로컬
- 학원 `Finished()` polling + VK_RETURN → Winters 는 Scene 이 IsFinished() 체크 후 자동 전환 또는 ImGui 프로그레스 바 표시
- 학원 `Ready_For_Level_Logo()` 식 레벨별 분기 → 씬의 `OnEnter` 에서 직접 Submit (Scene 이 자기 리소스 책임)

**실사용 예**:
```cpp
// Scene_InGame::OnEnter
bool Scene_InGame::OnEnter()
{
    m_pAsyncLoader = CAsyncLoader::Create(CGameInstance::Get()->Get_JobSystem());

    m_pAsyncLoader->Begin_Preload([this]{
        // JobSystem 내부에서 .wmesh 병렬 로드
        LoadChampion(L"Irelia");     m_pAsyncLoader->SetProgress(0.2f);
        LoadChampion(L"Yasuo");      m_pAsyncLoader->SetProgress(0.4f);
        LoadChampion(L"Sylas");      m_pAsyncLoader->SetProgress(0.6f);
        LoadChampion(L"Viego");      m_pAsyncLoader->SetProgress(0.8f);
        LoadChampion(L"Kalista");    m_pAsyncLoader->SetProgress(1.0f);
        m_pAsyncLoader->SetStep("Done");
    });
    return true;
}

void Scene_InGame::OnUpdate(f32_t dt)
{
    if (!m_pAsyncLoader->IsFinished()) return;   // 게임 로직은 로드 후에만
    ...
}

void Scene_InGame::OnImGui()
{
    if (!m_pAsyncLoader->IsFinished()) {
        ImGui::ProgressBar(m_pAsyncLoader->GetProgress01());
        ImGui::Text("Step: %s", m_pAsyncLoader->GetCurrentStep());
    }
}
```

**의존성**:
- Phase 5-B JobSystem race 수정 필수 선행
- Part C `.wmesh` 완성 시 에셋 I/O 자체가 <1ms 라 비동기 실익은 텍스처/사운드 중심
- Scene_Loading 씬 도입 여부 — 현재 Winters 는 로고→로딩→게임플레이 체인 미구현. 학원 `CLevel_Loading` 처럼 별도 씬 만들지, Scene_InGame 이 자기 로딩 표시할지 결정 필요.

**우선순위**: 중-상. Phase 5-B + Part C 후 Phase B-5.5 로 편성 제안.

#### 3.D.3. 레벨 종료 시 리소스 정리 (`CGameInstance::Clear_Resources(levelID)`)

**수업 방식**: `CLevel_Manager::Change_Level` 이 이전 레벨 `Clear_Resources` 호출 → `CPrototype_Manager` 가 해당 레벨 슬롯 맵 비움.

**Winters 현재**: `CGameInstance::Clear_Resources(iPrevSceneID)` 존재하지만 **빈 바디**:
```cpp
void CGameInstance::Clear_Resources(uint32_t iPrevSceneID)
{
    (void)iPrevSceneID;   // 미구현
}
```

**흡수 제안**: CEntityBlueprintRegistry 에 씬별 clear API 추가:
```cpp
// EntityBlueprintRegistry.h
void Clear_Scene(uint32_t iSceneID);    // 해당 씬의 blueprint 전부 삭제

// GameInstance.cpp Clear_Resources 실구현
void CGameInstance::Clear_Resources(uint32_t iPrevSceneID)
{
    if (m_pBlueprintRegistry)
        m_pBlueprintRegistry->Clear_Scene(iPrevSceneID);
    // 추후: ResourceCache 에서 해당 씬 전용 리소스 언로드
}
```

**우선순위**: 낮음 (씬 전환 거의 없는 현 단계). B-7b + Loader 작업과 함께.

### 3.E. Winters 가 이미 앞서있는 영역 (수업에서 배울 것 없음)

- **JobSystem** (Fiber/Chase-Lev) — 학원 `_beginthreadex` 보다 상위.
- **ECS + SystemScheduler** — 학원 Layer 보다 상위.
- **RHI 추상화** (`CDX11Device` + 예정 `IRHIDevice`) — 학원 `CGraphic_Device` 보다 상위.
- **Profiler** (계층 스코프 타이머 + 카운터 예정).
- **ImGui 통합** — 학원은 `SetWindowText(g_hWnd)` 로 디버그.
- **FMOD 사운드** — 학원 미지원.
- **Network SDK** (예정) — 학원 미지원.
- **Tier-2 RHI 게터** (2026-04-24 신설) — 학원은 직접 ComPtr 주입.

### 3.F. 학원 방식 중 Winters 가 거절하는 관용구 (이유 박제)

| 학원 관용구 | Winters 거절 사유 | Winters 대체 |
|---|---|---|
| `__super::Method()` | MSVC 전용 확장, 표준 아님 | `CBase::Method()` 명시 |
| `_beginthreadex + CRITICAL_SECTION` | C-스타일, std 더 안전 | `std::thread + std::mutex` (또는 JobSystem) |
| `DBG_NEW new` 매크로 | 글로벌 `new` 재정의 → ImGui 등 충돌 | `push_macro/pop_macro` 로컬 보호 |
| `void* pArg` Clone 인자 | 타입 안전 없음, 캐스팅 실수 여지 | 템플릿 `TArgs` 또는 `std::any` |
| `ComPtr<ID3D11Device>` 생성자 주입 | 모든 객체가 디바이스 들고 다님, 결합도 상승 | `CGameInstance::Get()->Get_RHIDevice()` Tier-2 |
| `MSG_BOX` 모달 | 릴리스에서도 팝업 뜰 수 있음, 서버 환경 부적합 | `OutputDebugStringA` + 로그 파일 + Assert (Debug 만) |
| `using namespace std;` 전역 | 이름 충돌 위험 | 헤더 `std::` 명시, Private cpp 는 허용 (Winters 현재 혼재, 점진 정리) |
| `using namespace Engine;` 전역 (Engine_Defines.h) | 동상 | 동상 |

---

## 4. 즉시 적용 계획 — Winters 쪽 작업

### 4.1. CLAUDE.md 명문화 (코딩 컨벤션 섹션에 추가)

**파일**: `C:\Users\user\Desktop\Winters\CLAUDE.md`

**삽입 위치**: §483 (`## 서브시스템 설계 원칙`) **직전**.

**신규 섹션**:
```markdown
## 수업 DX11 × Winters 설계 결정 매트릭스 (2026-04-24)

상세: `.md/architecture/CLASS_DAY8_VS_WINTERS.md`

| 수업 개념 | Winters 결정 | 이유 |
|---|---|---|
| Prototype 패턴 | ✅ 흡수 (CEntityBlueprint composition) | 상속 깊이 고정 회피 |
| Prototype_Manager per-level | ✅ 흡수 (CEntityBlueprintRegistry per-scene) | Scene 단위 리소스 라이프사이클 |
| CGameObject 상속 계층 | ❌ 미도입 | ECS 철학과 상충, AoS→SoA 전환 목적 |
| CLayer list 관리 | ❌ 미도입 | ECS ForEach 가 대체 |
| CObject_Manager level×layer | ❌ 미도입 | CWorld 단일 소유가 충분 |
| CLevel / CLevel_Manager | ✅ 흡수 완료 (IScene + CScene_Manager) | - |
| CLoader 백그라운드 로딩 | 🟢 조건부 흡수 예정 (CAsyncLoader, Phase B-5.5) | JobSystem + atomic 현대화 |
| `Initialize_Prototype` + `Initialize(void*)` 2-phase | 🟢 조건부 흡수 예정 (CEntityBlueprint::AddArgs + Spawn(world,args)) | 미니언 스폰 스케일 대비 |
| `__super::Method` | ❌ 거절 | MSVC 전용 |
| `_beginthreadex + CRITICAL_SECTION` | ❌ 거절 | std::thread + std::mutex |
| `void* pArg` 타입 | ❌ 거절 | `TArgs` 템플릿 또는 `std::any` |
| `ComPtr<ID3D11Device>` 생성자 주입 | ❌ 거절 | Tier-2 RHI 게터 (GameInstance 경유) |
```

### 4.2. `CEntityBlueprint::AddArgs + Spawn(args)` 신규 (Phase B-7b)

**선행 조건**: Phase 5-B race 수정 + Part C `.wmesh` 완료.

**수정 대상**:
- `Engine/Public/ECS/Systems/EntityBlueprint.h` — `AddArgs`/`Spawn(world, pArg)` 추가
- `Engine/Public/ECS/Systems/EntityBlueprintRegistry.h` — `Clone_Entity(sceneID, key, world, const void* pArg)` 오버로드
- `Engine/Include/GameInstance.h` — 포워드 오버로드 추가

**타입 안전 버전 보류 근거**: 템플릿 blueprint 는 EntityBlueprintRegistry 맵에 담기 까다로움 (타입 소거 필요). MVP 는 `void*` 로 시작, 이후 typed 래퍼로 감싸는 방향.

### 4.3. `CAsyncLoader` 신규 (Phase B-5.5, Part C 완료 후)

**선행 조건**:
- Phase 5-B JobSystem race 수정 (Submit 가능 상태)
- Part C `.wmesh` 완료 (CPU 파싱 부담 제거된 후에도 남은 블로킹 = 텍스처 디코드/GPU 업로드)

**파일**:
- `Engine/Public/Resource/AsyncLoader.h/.cpp` 신규
- Scene_InGame `OnEnter` 개편 — 기존 동기 로드 → `Begin_Preload` 분기
- ImGui 로딩 UI (progress bar + step text)

### 4.4. `CGameInstance::Clear_Resources` 실구현 (Phase B-7b 와 묶어)

**수정 대상**:
- `Engine/Public/ECS/Systems/EntityBlueprintRegistry.h` — `Clear_Scene(sceneID)` 메서드 추가
- `Engine/Private/GameInstance.cpp:112` — `Clear_Resources` 빈 바디를 `m_pBlueprintRegistry->Clear_Scene(iPrevSceneID)` 로 채움
- 추후 `CResourceCache::Unload_Scene(sceneID)` 도 연계 (리소스 씬 귀속 추적 필요 → 별도 Phase)

---

## 5. 결정 요약 — 원탭 참조

**현재 세션에 할 것**: CLAUDE.md 에 §4.1 결정 매트릭스 추가 (지금).

**다음 세션 (Profiler 측정 후)**:
1. Part B Profiler 수치로 Nav O(N²) 범인 확정
2. Part C `.wmesh` 구현 → 로딩 3초 → <500ms
3. Phase 5-B JobSystem race 수정

**Phase B-5.5 / B-7b 에서**:
- CAsyncLoader (프로그레스 바 포함)
- CEntityBlueprint AddArgs + Spawn(args) 2-phase 초기화
- Clear_Resources 실구현

**영구 거절 (학원 방식)**:
- CGameObject / CLayer / CObject_Manager 상속-list-이중맵 패턴
- `__super`, `_beginthreadex`, `DBG_NEW new`, `void* pArg`, `ComPtr` 생성자 주입, `MSG_BOX`

---

## 6. 검증 체크리스트 (계획서 적용 시)

- [ ] CLAUDE.md §483 직전에 §4.1 매트릭스 삽입됨
- [ ] 본 문서(`CLASS_DAY8_VS_WINTERS.md`) 가 `.md/architecture/` 에 존재
- [ ] CLAUDE.md 문서 인덱스 섹션 (§947) 에 본 문서 링크 추가
- [ ] 수업 8일차 코드 (`C:\Users\user\Downloads\8일차\`) 는 **참조용 보관**, Winters 레포에 복사하지 않음 (AoS/Layer 방식 의도치 않게 섞이는 것 방지)
- [ ] 향후 "수업에서 X 했는데 Winters 는?" 질문 오면 본 표 참조로 답변

---

## 7. 문서 위치 규약

| 파일 | 역할 |
|---|---|
| `CLAUDE.md` | 최상위 설계 결정 매트릭스만 간략히 |
| `.md/architecture/CLASS_DAY8_VS_WINTERS.md` | 본 문서 — 전수 비교 + 구현 경로 |
| `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md` | Winters 자체 컨벤션 상세 (기존) |
| `.md/architecture/WINTERS_ENGINE_ARCHITECTURE_FINAL.md` | 7-Layer 아키텍처 (기존) |
