# 04. World & Subsystem -- UE5-Style WWorld / WLevel / WWorldSubsystem / WGameInstance

> **UE5 대응**: `UWorld`, `ULevel`, `UWorldSubsystem`, `UGameInstance`, `SpawnActor<T>`, `GetSubsystem<T>`, Tick Groups
> **현재 Winters**: `CScene_InGame` 3000줄 god-object (렌더/입력/네트워크/UI/ECS/FX/전투 로직 일체), `CGameInstance` 싱글턴 모든 매니저 포워딩
> **목표**: WWorld 가 액터 라이프사이클 관리, WWorldSubsystem 으로 기능 분산, Scene_InGame 100줄 이하

---

## 1. Architecture Overview

### 1.1 UE5 World/Subsystem 핵심

```
UGameInstance (persistent across levels)
  └── UWorld (per-level active world)
       ├── ULevel (map geometry + placed actors)
       ├── SpawnActor<T>() → actor lifecycle
       ├── Tick(deltaTime) → tick groups (PrePhysics / DuringPhysics / PostPhysics)
       └── GetSubsystem<T>()
            ├── UNavigationSubsystem
            ├── UProjectileSubsystem
            ├── UCombatSubsystem
            └── UFxSubsystem
```

### 1.2 현재 Scene_InGame 문제 분석

```
CScene_InGame (634줄 헤더, ~3000줄 cpp):
  - 7개 챔프 ModelRenderer + CTransform 멤버 (14개)
  - 7개 챔프 EntityID 멤버
  - 100+ 튜닝 파라미터 (Irelia/Yasuo/Kalista 각각)
  - FxSystem, FxMeshSystem, IreliaBladeSystem, UltWaveSystem 소유
  - WindWallSystem, YasuoProjectileSystem, PendingHitSystem 소유
  - KalistaProjectileSystem, KalistaRendSystem 소유
  - CNavGrid 소유
  - CClientNetwork, CSnapshotApplier, CCommandSerializer 소유
  - CDynamicCamera 소유
  - NormalPass, SSAOPass 소유
  - ECS CWorld 소유 + CSystemSchedular

이 모든 것이 OnUpdate/OnRender/OnImGui 에서 직접 호출.
→ 단일 파일 수정 = 항상 충돌, 기능 추가 = 파일 비대화, 테스트 불가능
```

### 1.3 Winters World/Subsystem 설계

```
WGameInstance (persistent, replaces current CGameInstance gateway)
  └── WWorld (replaces Scene_InGame as god object)
       ├── WLevel (map data: NavGrid + static meshes)
       ├── SpawnActor<T>() → WActor lifecycle
       ├── Tick(deltaTime) → tick groups
       ├── FlushPendingDestroy()
       └── GetSubsystem<T>()
            ├── WNavigationSubsystem (CNavGrid + pathfinding)
            ├── WProjectileSubsystem (Yasuo/Kalista projectiles)
            ├── WCombatSubsystem (targeting, damage, hit detection)
            ├── WFxSubsystem (FxSystem + FxMeshSystem + billboards)
            ├── WNetworkSubsystem (CClientNetwork + snapshot apply)
            ├── WCameraSubsystem (CDynamicCamera)
            └── WRenderSubsystem (NormalPass + SSAOPass + RenderGraph)
```

---

## 2. 파일 구조

```
Engine/
├── Public/World/
│   ├── WWorld.h                  -- 월드 (액터 관리 + 서브시스템 호스트)
│   ├── WLevel.h                  -- 레벨 (맵 데이터 컨테이너)
│   ├── WWorldSubsystem.h         -- 서브시스템 기반 클래스
│   ├── WGameInstance.h           -- 레벨 간 persistent 상태
│   └── TickGroups.h              -- 틱 그룹 enum
├── Private/World/
│   ├── WWorld.cpp
│   ├── WLevel.cpp
│   ├── WWorldSubsystem.cpp
│   └── WGameInstance.cpp
```

---

## 3. 코드 전문

### `Engine/Public/World/TickGroups.h`

```cpp
#pragma once

#include "WintersTypes.h"

/// UE5 Tick Group 대응
/// 시스템/액터/컴포넌트의 Tick 순서 보장.
/// 낮은 값이 먼저 실행.
enum class eTickGroup : u8_t
{
    PrePhysics    = 0,   // 입력, AI, 이동 의도 (Nav 요청)
    DuringPhysics = 1,   // 물리 시뮬레이션, 충돌 감지
    PostPhysics   = 2,   // 충돌 후 반응, 히트 감지
    PreRender     = 3,   // 카메라 업데이트, LOD 결정
    Count         = 4,
};
```

### `Engine/Public/World/WWorldSubsystem.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/ObjectMacros.h"
#include "World/TickGroups.h"

#include <string>

class WWorld;

/// UE5 UWorldSubsystem 대응
/// WWorld 에 등록되어 특정 기능을 모듈화.
/// Scene_InGame 의 3000줄을 10~20개 서브시스템으로 분산.
///
/// 사용법:
///   1. WWorldSubsystem 상속
///   2. WWorld::RegisterSubsystem<T>() 또는 자동 등록
///   3. WWorld::GetSubsystem<T>() 로 접근
WCLASS()
class WINTERS_API WWorldSubsystem : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(WWorldSubsystem)

public:
    virtual ~WWorldSubsystem();

    /// 서브시스템 초기화 (WWorld::Initialize 에서 호출)
    virtual void Initialize(WWorld* pWorld);

    /// 서브시스템 종료 (WWorld::Shutdown 에서 호출)
    virtual void Deinitialize();

    /// 프레임 틱
    virtual void Tick(f32_t fDeltaTime);

    /// ImGui 디버그 UI (WINTERS_EDITOR 빌드에서만)
    virtual void OnImGui();

    /// 이 서브시스템의 틱 그룹 (기본 PrePhysics)
    virtual eTickGroup GetTickGroup() const { return eTickGroup::PrePhysics; }

    /// 서브시스템 이름 (디버그/로그용)
    virtual const char* GetSubsystemName() const { return "WWorldSubsystem"; }

    /// 소유 월드 접근
    WWorld* GetWorld() const { return m_pWorld; }

    /// 활성 상태
    bool IsEnabled() const { return m_bEnabled; }
    void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

protected:
    WWorldSubsystem();

private:
    WWorld* m_pWorld = nullptr;
    bool    m_bEnabled = true;
};
```

### `Engine/Private/World/WWorldSubsystem.cpp`

```cpp
#include "World/WWorldSubsystem.h"

void WWorldSubsystem::RegisterProperties(WClass* cls)
{
    // base 서브시스템은 에디터 프로퍼티 없음
}

WWorldSubsystem::WWorldSubsystem()
{
}

WWorldSubsystem::~WWorldSubsystem()
{
}

void WWorldSubsystem::Initialize(WWorld* pWorld)
{
    m_pWorld = pWorld;
}

void WWorldSubsystem::Deinitialize()
{
    m_pWorld = nullptr;
}

void WWorldSubsystem::Tick(f32_t fDeltaTime)
{
    // 서브클래스 오버라이드
}

void WWorldSubsystem::OnImGui()
{
    // 서브클래스 오버라이드
}
```

### `Engine/Public/World/WLevel.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/ObjectMacros.h"

#include <string>
#include <vector>
#include <memory>

class WActor;

/// UE5 ULevel 대응
/// 맵의 정적 데이터 컨테이너 (지형, 구조물, NavMesh 등).
/// WWorld 가 WLevel 을 로드하면 그 안의 배치 액터가 WWorld 에 등록.
WCLASS()
class WINTERS_API WLevel : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(WLevel)

public:
    virtual ~WLevel();

    /// 레벨 데이터 로드 (향후 .wlevel 파일)
    bool Load(const std::string& strLevelPath);

    /// 레벨 언로드
    void Unload();

    /// 레벨 이름
    const std::string& GetLevelName() const { return m_LevelName; }

    /// 배치된 액터 목록 (WWorld 에 스폰 전 데이터)
    const std::vector<WActor*>& GetPlacedActors() const { return m_PlacedActors; }

    /// 레벨 로드 여부
    bool IsLoaded() const { return m_bLoaded; }

protected:
    WLevel();

private:
    std::string m_LevelName;
    std::vector<WActor*> m_PlacedActors;
    bool m_bLoaded = false;
};
```

### `Engine/Private/World/WLevel.cpp`

```cpp
#include "World/WLevel.h"

void WLevel::RegisterProperties(WClass* cls)
{
    // 레벨 프로퍼티: 향후 에디터 노출
}

WLevel::WLevel()
{
}

WLevel::~WLevel()
{
    Unload();
}

bool WLevel::Load(const std::string& strLevelPath)
{
    m_LevelName = strLevelPath;
    // 향후: .wlevel 파싱 → 배치 액터 데이터 로드
    // 현재: 맵 메시 로드는 외부에서 WMeshComponent 로 직접
    m_bLoaded = true;
    return true;
}

void WLevel::Unload()
{
    m_PlacedActors.clear();
    m_bLoaded = false;
}
```

### `Engine/Public/World/WWorld.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/ObjectMacros.h"
#include "World/WWorldSubsystem.h"
#include "World/WLevel.h"
#include "World/TickGroups.h"
#include "Actor/WActor.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <algorithm>

/// UE5 UWorld 대응
/// 게임 세계의 루트 컨테이너. 액터 라이프사이클, 서브시스템 관리, 틱 디스패치.
/// Scene_InGame 3000줄의 대체. Scene 은 WWorld 인스턴스를 소유하고 위임만 하면 됨.
///
/// Usage:
///   WWorld* pWorld = WWorld::Create();
///   pWorld->RegisterSubsystem<WNavigationSubsystem>();
///   pWorld->RegisterSubsystem<WCombatSubsystem>();
///   pWorld->Initialize();
///   auto* actor = pWorld->SpawnActor<WChampionActor>({0,0,0});
///   pWorld->Tick(dt);  // actors + subsystems tick in group order
WCLASS()
class WINTERS_API WWorld : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(WWorld)

public:
    virtual ~WWorld();

    /// 팩토리
    static std::unique_ptr<WWorld> Create();

    // ---- Lifecycle ----

    /// 서브시스템 초기화 + 레벨 로드
    void Initialize();

    /// 메인 틱: 틱 그룹 순서대로 서브시스템 → 액터 틱
    void Tick(f32_t fDeltaTime);

    /// 렌더: 모든 가시 액터의 메시 컴포넌트 Render 호출
    void Render();

    /// ImGui: 모든 서브시스템 OnImGui 호출
    void OnImGui();

    /// 종료: 모든 액터 파괴 + 서브시스템 해제
    void Shutdown();

    // ---- Actor Lifecycle ----

    /// UE5 SpawnActor<T> 대응
    /// T 인스턴스 생성 → InitializeComponents → BeginPlay → m_Actors 에 등록
    template<typename T>
    T* SpawnActor(const Vec3& vPosition = {0,0,0}, const Vec3& vRotation = {0,0,0})
    {
        static_assert(std::is_base_of_v<WActor, T>,
                      "T must derive from WActor");

        auto actor = std::make_unique<T>();
        T* pRaw = actor.get();

        pRaw->m_pWorld = this;
        pRaw->InitializeComponents();

        if (pRaw->GetRootComponent())
        {
            pRaw->GetRootComponent()->SetRelativePosition(vPosition);
            pRaw->GetRootComponent()->SetRelativeRotation(vRotation);
        }

        pRaw->BeginPlay();
        m_PendingSpawnActors.push_back(std::move(actor));

        return pRaw;
    }

    /// 액터 파괴 (현재 프레임 끝에 제거)
    void DestroyActor(WActor* pActor);

    /// 모든 액터 순회
    template<typename Fn>
    void ForEachActor(Fn&& fn)
    {
        for (auto& actor : m_Actors)
        {
            if (!actor->IsPendingDestroy())
                fn(actor.get());
        }
    }

    /// 타입별 액터 검색 (첫 번째)
    template<typename T>
    T* FindActorByClass() const
    {
        for (auto& actor : m_Actors)
        {
            T* casted = dynamic_cast<T*>(actor.get());
            if (casted && !casted->IsPendingDestroy())
                return casted;
        }
        return nullptr;
    }

    /// 타입별 모든 액터 수집
    template<typename T>
    void GetAllActorsOfClass(std::vector<T*>& outActors) const
    {
        for (auto& actor : m_Actors)
        {
            T* casted = dynamic_cast<T*>(actor.get());
            if (casted && !casted->IsPendingDestroy())
                outActors.push_back(casted);
        }
    }

    /// 네트워크 ID 로 액터 검색
    WActor* FindActorByNetID(u32_t netId) const;

    /// 액터 수
    u32_t GetActorCount() const { return static_cast<u32_t>(m_Actors.size()); }

    // ---- Subsystem Management ----

    /// 서브시스템 등록 (Initialize 전에 호출)
    template<typename T>
    T* RegisterSubsystem()
    {
        static_assert(std::is_base_of_v<WWorldSubsystem, T>,
                      "T must derive from WWorldSubsystem");

        auto key = std::type_index(typeid(T));
        if (m_SubsystemMap.count(key) > 0)
            return static_cast<T*>(m_SubsystemMap[key]);

        auto sub = std::make_unique<T>();
        T* pRaw = sub.get();
        m_SubsystemMap[key] = pRaw;
        m_Subsystems.push_back(std::move(sub));
        return pRaw;
    }

    /// 서브시스템 접근
    template<typename T>
    T* GetSubsystem() const
    {
        auto key = std::type_index(typeid(T));
        auto it = m_SubsystemMap.find(key);
        return (it != m_SubsystemMap.end()) ? static_cast<T*>(it->second) : nullptr;
    }

    // ---- Level ----

    /// 레벨 로드
    void LoadLevel(const std::string& strLevelPath);

    /// 현재 레벨
    WLevel* GetCurrentLevel() const { return m_pCurrentLevel.get(); }

    // ---- ECS Bridge (과도기 호환) ----

    /// 기존 ECS CWorld 접근 (마이그레이션 기간 중 병존)
    CWorld* GetECSWorld() { return m_pECSWorld; }
    void SetECSWorld(CWorld* pWorld) { m_pECSWorld = pWorld; }

    // ---- Time ----

    f32_t GetDeltaTime() const { return m_fDeltaTime; }
    f64_t GetWorldTime() const { return m_fWorldTime; }

protected:
    WWorld();

private:
    /// 스폰 대기 액터를 메인 배열로 이동
    void FlushPendingSpawn();

    /// 파괴 예약 액터 정리
    void FlushPendingDestroy();

    // ---- Actors ----
    std::vector<std::unique_ptr<WActor>> m_Actors;
    std::vector<std::unique_ptr<WActor>> m_PendingSpawnActors;

    // ---- Subsystems ----
    std::vector<std::unique_ptr<WWorldSubsystem>> m_Subsystems;
    std::unordered_map<std::type_index, WWorldSubsystem*> m_SubsystemMap;

    // ---- Level ----
    std::unique_ptr<WLevel> m_pCurrentLevel;

    // ---- ECS Bridge ----
    CWorld* m_pECSWorld = nullptr;

    // ---- Time ----
    f32_t m_fDeltaTime = 0.f;
    f64_t m_fWorldTime = 0.0;
    bool  m_bInitialized = false;
};
```

### `Engine/Private/World/WWorld.cpp`

```cpp
#include "World/WWorld.h"
#include "Actor/WMeshComponent.h"

#ifdef _DEBUG
#include <crtdbg.h>
#define WORLD_LOG(fmt, ...) do {                                  \
    char _buf[512];                                               \
    snprintf(_buf, sizeof(_buf), "[WWorld] " fmt "\n",            \
             ##__VA_ARGS__);                                      \
    OutputDebugStringA(_buf);                                     \
} while(0)
#else
#define WORLD_LOG(fmt, ...) ((void)0)
#endif

void WWorld::RegisterProperties(WClass* cls)
{
    // WWorld 자체 에디터 프로퍼티는 향후 추가
}

WWorld::WWorld()
{
}

WWorld::~WWorld()
{
    Shutdown();
}

std::unique_ptr<WWorld> WWorld::Create()
{
    return std::unique_ptr<WWorld>(new WWorld());
}

void WWorld::Initialize()
{
    if (m_bInitialized) return;

    // 서브시스템 초기화 (등록 순서)
    for (auto& sub : m_Subsystems)
    {
        sub->Initialize(this);
        WORLD_LOG("Subsystem initialized: %s", sub->GetSubsystemName());
    }

    m_bInitialized = true;
    WORLD_LOG("WWorld initialized with %u subsystems",
              static_cast<u32_t>(m_Subsystems.size()));
}

void WWorld::Tick(f32_t fDeltaTime)
{
    m_fDeltaTime = fDeltaTime;
    m_fWorldTime += static_cast<f64_t>(fDeltaTime);

    // 1. 스폰 대기 액터 반영
    FlushPendingSpawn();

    // 2. 틱 그룹 순서대로 서브시스템 → 액터 틱
    for (u8_t group = 0; group < static_cast<u8_t>(eTickGroup::Count); ++group)
    {
        eTickGroup currentGroup = static_cast<eTickGroup>(group);

        // 서브시스템 틱 (해당 그룹)
        for (auto& sub : m_Subsystems)
        {
            if (sub->IsEnabled() && sub->GetTickGroup() == currentGroup)
                sub->Tick(fDeltaTime);
        }

        // 액터 틱 (PrePhysics 그룹에서만, 향후 액터별 틱 그룹 지정)
        if (currentGroup == eTickGroup::PrePhysics)
        {
            for (auto& actor : m_Actors)
            {
                if (!actor->IsPendingDestroy())
                    actor->Tick(fDeltaTime);
            }
        }
    }

    // 3. 파괴 대기 액터 정리
    FlushPendingDestroy();
}

void WWorld::Render()
{
    // 모든 액터의 WMeshComponent 자동 수집 + Render
    // 05_RENDERING_PIPELINE 에서 SceneProxy + RenderGraph 로 교체 예정.
    // 현재: 직접 순회
    for (auto& actor : m_Actors)
    {
        if (actor->IsPendingDestroy()) continue;

        // 메시 컴포넌트 수집
        for (auto& comp : actor->GetComponents())
        {
            auto* meshComp = dynamic_cast<WMeshComponent*>(comp.get());
            if (meshComp && meshComp->IsVisible() && meshComp->IsActive())
                meshComp->Render();
        }
    }
}

void WWorld::OnImGui()
{
#ifdef WINTERS_EDITOR
    for (auto& sub : m_Subsystems)
    {
        if (sub->IsEnabled())
            sub->OnImGui();
    }
#endif
}

void WWorld::Shutdown()
{
    if (!m_bInitialized) return;

    // 액터 EndPlay + 파괴
    for (auto& actor : m_Actors)
    {
        actor->EndPlay();
    }
    m_Actors.clear();
    m_PendingSpawnActors.clear();

    // 서브시스템 역순 종료
    for (auto it = m_Subsystems.rbegin(); it != m_Subsystems.rend(); ++it)
    {
        (*it)->Deinitialize();
        WORLD_LOG("Subsystem deinitialized: %s", (*it)->GetSubsystemName());
    }
    m_Subsystems.clear();
    m_SubsystemMap.clear();

    m_pCurrentLevel.reset();
    m_pECSWorld = nullptr;
    m_bInitialized = false;

    WORLD_LOG("WWorld shutdown");
}

void WWorld::DestroyActor(WActor* pActor)
{
    if (pActor)
        pActor->Destroy();
}

WActor* WWorld::FindActorByNetID(u32_t netId) const
{
    for (auto& actor : m_Actors)
    {
        if (actor->GetNetID() == netId && !actor->IsPendingDestroy())
            return actor.get();
    }
    return nullptr;
}

void WWorld::LoadLevel(const std::string& strLevelPath)
{
    if (m_pCurrentLevel)
        m_pCurrentLevel->Unload();

    m_pCurrentLevel = std::make_unique<WLevel>();
    m_pCurrentLevel->Load(strLevelPath);

    WORLD_LOG("Level loaded: %s", strLevelPath.c_str());
}

void WWorld::FlushPendingSpawn()
{
    if (m_PendingSpawnActors.empty()) return;

    for (auto& actor : m_PendingSpawnActors)
    {
        WORLD_LOG("Actor spawned: %s (class: %s)",
                  actor->GetObjectName().c_str(),
                  actor->GetClass()->GetName());
        m_Actors.push_back(std::move(actor));
    }
    m_PendingSpawnActors.clear();
}

void WWorld::FlushPendingDestroy()
{
    auto it = std::remove_if(m_Actors.begin(), m_Actors.end(),
        [](const std::unique_ptr<WActor>& actor)
        {
            if (actor->IsPendingDestroy())
            {
                actor->EndPlay();
                WORLD_LOG("Actor destroyed: %s", actor->GetObjectName().c_str());
                return true;
            }
            return false;
        });
    m_Actors.erase(it, m_Actors.end());
}
```

### `Engine/Public/World/WGameInstance.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Object/ObjectMacros.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>

class WWorld;

/// UE5 UGameInstance 대응
/// 레벨 전환에도 유지되는 persistent 상태.
/// 현재 CGameInstance (Engine::CGameInstance) 의 게이트웨이 역할을 점진 흡수.
///
/// CGameInstance 와의 관계:
///   - CGameInstance: 엔진 내부 매니저 포워딩 (Timer, Sound, Scene, RHI getter)
///   - WGameInstance: 게임 수준 persistent 상태 (로비 정보, 플레이어 세팅, 월드 전환)
///   - 마이그레이션 기간 중 공존. CGameInstance 의 게임 로직을 점진 이전.
WCLASS()
class WINTERS_API WGameInstance : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(WGameInstance)

public:
    virtual ~WGameInstance();

    /// 싱글턴 접근
    static WGameInstance* Get();

    /// 초기화 (CEngineApp::Initialize 에서 호출)
    virtual void Initialize();

    /// 종료
    virtual void Shutdown();

    /// 월드 전환 (레벨 로드)
    void ChangeWorld(const std::string& strLevelName);

    /// 현재 활성 월드
    WWorld* GetWorld() const { return m_pWorld.get(); }

    // ---- Persistent 게임 데이터 ----

    /// 플레이어 선택 챔피언 (로비 → 인게임 전달)
    void SetSelectedChampion(u32_t iChampionId) { m_iSelectedChampionId = iChampionId; }
    u32_t GetSelectedChampion() const { return m_iSelectedChampionId; }

    /// 플레이어 팀
    void SetPlayerTeam(u8_t eTeam) { m_ePlayerTeam = eTeam; }
    u8_t GetPlayerTeam() const { return m_ePlayerTeam; }

    /// 서버 접속 정보
    void SetServerAddress(const std::string& addr) { m_ServerAddress = addr; }
    const std::string& GetServerAddress() const { return m_ServerAddress; }

    /// 클라이언트 세션 ID
    void SetSessionId(u32_t id) { m_SessionId = id; }
    u32_t GetSessionId() const { return m_SessionId; }

    // ---- Game Instance Subsystem (UE5 UGameInstanceSubsystem 대응) ----
    // 향후: RegisterSubsystem / GetSubsystem 패턴 추가

protected:
    WGameInstance();

private:
    static WGameInstance* s_pInstance;

    std::unique_ptr<WWorld> m_pWorld;

    // Persistent game state
    u32_t       m_iSelectedChampionId = 0;
    u8_t        m_ePlayerTeam = 0;
    std::string m_ServerAddress;
    u32_t       m_SessionId = 0;
};
```

### `Engine/Private/World/WGameInstance.cpp`

```cpp
#include "World/WGameInstance.h"
#include "World/WWorld.h"

WGameInstance* WGameInstance::s_pInstance = nullptr;

void WGameInstance::RegisterProperties(WClass* cls)
{
    // persistent 프로퍼티: 향후 세이브/로드 대상
}

WGameInstance::WGameInstance()
{
    s_pInstance = this;
}

WGameInstance::~WGameInstance()
{
    Shutdown();
    if (s_pInstance == this)
        s_pInstance = nullptr;
}

WGameInstance* WGameInstance::Get()
{
    return s_pInstance;
}

void WGameInstance::Initialize()
{
    // 기본 월드 생성 (레벨 없는 빈 월드)
    m_pWorld = WWorld::Create();
}

void WGameInstance::Shutdown()
{
    if (m_pWorld)
    {
        m_pWorld->Shutdown();
        m_pWorld.reset();
    }
}

void WGameInstance::ChangeWorld(const std::string& strLevelName)
{
    // 기존 월드 종료
    if (m_pWorld)
        m_pWorld->Shutdown();

    // 새 월드 생성 + 레벨 로드
    m_pWorld = WWorld::Create();
    m_pWorld->LoadLevel(strLevelName);
    // 서브시스템 등록은 게임 모듈이 콜백으로 처리
    // 향후: OnWorldCreated 이벤트 → 게임 모듈이 서브시스템 등록 + Initialize
}
```

---

## 4. 사용 예시

### 4.1 Before: Scene_InGame (3000줄 발췌)

```cpp
// Scene_InGame.cpp — OnEnter (~200줄)
bool CScene_InGame::OnEnter()
{
    // 카메라 생성
    m_pCamera = std::make_unique<CDynamicCamera>();
    m_pCamera->Initialize(/*...*/);

    // 네트워크 초기화
    m_pNetwork = std::make_unique<CClientNetwork>();
    m_pSnapshotApplier = std::make_unique<CSnapshotApplier>();
    m_pCommandSerializer = std::make_unique<CCommandSerializer>();

    // NavGrid 생성
    m_pNavGrid = std::make_unique<CNavGrid>(200, 200, 0.5f);

    // FX 시스템 초기화
    m_pFxSystem = std::make_unique<CFxSystem>();
    m_pFxMeshRenderer = std::make_unique<CFxStaticMeshRenderer>();
    m_pFxMeshSystem = std::make_unique<CFxMeshSystem>();
    m_pIreliaBladeSystem = std::make_unique<CIreliaBladeSystem>();
    m_pUltWaveSystem = std::make_unique<CUltWaveSystem>();
    m_pWindWallSystem = std::make_unique<CWindWallSystem>();
    m_pYasuoProjectileSystem = std::make_unique<CYasuoProjectileSystem>();
    m_pPendingHitSystem = std::make_unique<CPendingHitSystem>();
    m_pKalistaProjectileSystem = std::make_unique<CKalistaProjectileSystem>();
    m_pKalistaRendSystem = std::make_unique<CKalistaRendSystem>();

    // SSAO
    m_pNormalPass = std::make_unique<Engine::CNormalPass>();
    m_pSSAOPass = std::make_unique<Engine::CSSAOPass>();

    // 7개 챔프 수동 초기화 (각 30줄)
    m_Irelia.Init("Irelia/irelia.wmesh");
    m_Irelia.LoadMeshTexture(0, L"Irelia/irelia_base.png");
    m_IreliaTransform.SetPosition({0, 0, 0});
    m_IreliaTransform.SetScale({0.01f, 0.01f, 0.01f});
    // ... ×7 champions

    CreateECSEntities();
    return true;
}

// OnUpdate (~500줄)
void CScene_InGame::OnUpdate(f32_t dt)
{
    m_pCamera->Update(dt);
    SyncECSTransformsFromLegacy();
    UpdateTargeting();
    UpdateCombatInput(/*...*/);
    UpdateDash(dt);
    UpdateYasuoDash(dt);
    UpdateYasuoR(dt);
    UpdateKalistaPassiveDash(dt);
    UpdateFlashCooldown(dt);
    m_pScheduler->Update(m_World, dt);
    m_pFxSystem->Update(dt);
    m_pFxMeshSystem->Update(dt, /*...*/);
    m_pIreliaBladeSystem->Update(dt, /*...*/);
    m_pUltWaveSystem->Update(dt, /*...*/);
    m_pWindWallSystem->Update(dt);
    m_pYasuoProjectileSystem->Update(dt, /*...*/);
    m_pPendingHitSystem->Update(dt, /*...*/);
    m_pKalistaProjectileSystem->Update(dt, /*...*/);
    m_pKalistaRendSystem->Update(dt, /*...*/);
    UpdateNetworkChampionLocomotion(dt);
    // ... 500줄 더
}

// OnRender (~100줄)
void CScene_InGame::OnRender()
{
    m_Map.Render();
    m_Irelia.Render();
    m_Yasuo.Render();
    m_Sylas.Render();
    m_Viego.Render();
    m_Kalista.Render();
    m_Garen.Render();
    m_Zed.Render();
    // + FX render + UI render + debug render...
}
```

### 4.2 After: Scene_InGame 100줄 (WWorld + Subsystems)

```cpp
// Scene_InGame_New.cpp — WWorld 기반 (~80줄)
#include "World/WWorld.h"
#include "World/WGameInstance.h"

// 서브시스템 include
#include "Subsystem/WCameraSubsystem.h"
#include "Subsystem/WNavigationSubsystem.h"
#include "Subsystem/WCombatSubsystem.h"
#include "Subsystem/WFxSubsystem.h"
#include "Subsystem/WNetworkSubsystem.h"
#include "Subsystem/WRenderSubsystem.h"
#include "Subsystem/WChampionSpawnSubsystem.h"
#include "GameObject/Champion/WChampionActor.h"

class CScene_InGame_v2 final : public IScene
{
public:
    bool OnEnter() override
    {
        m_pWorld = WWorld::Create();

        // 서브시스템 등록 (기존 3000줄의 초기화 → 각 서브시스템 내부)
        m_pWorld->RegisterSubsystem<WCameraSubsystem>();
        m_pWorld->RegisterSubsystem<WNavigationSubsystem>();
        m_pWorld->RegisterSubsystem<WCombatSubsystem>();
        m_pWorld->RegisterSubsystem<WFxSubsystem>();
        m_pWorld->RegisterSubsystem<WNetworkSubsystem>();
        m_pWorld->RegisterSubsystem<WRenderSubsystem>();
        m_pWorld->RegisterSubsystem<WChampionSpawnSubsystem>();

        m_pWorld->Initialize();

        // 레벨 로드 (맵 메시 + NavGrid)
        m_pWorld->LoadLevel("SummonersRift");

        // 챔프 스폰 = 서브시스템이 담당
        // WChampionSpawnSubsystem::Initialize 에서 ChampionTable 기반 자동 스폰
        return true;
    }

    void OnExit() override
    {
        m_pWorld->Shutdown();
        m_pWorld.reset();
    }

    void OnUpdate(f32_t dt) override
    {
        m_pWorld->Tick(dt);
    }

    void OnLateUpdate(f32_t dt) override
    {
        // 필요 시 late update 서브시스템 호출
    }

    void OnRender() override
    {
        m_pWorld->Render();  // 자동 수집 + 자동 렌더
    }

    void OnImGui() override
    {
        m_pWorld->OnImGui();
    }

private:
    std::unique_ptr<WWorld> m_pWorld;
};
```

### 4.3 서브시스템 예시: WNavigationSubsystem

```cpp
// Client/Public/Subsystem/WNavigationSubsystem.h
#pragma once
#include "World/WWorldSubsystem.h"
#include "Manager/Navigation/NavGrid.h"
#include <memory>

WCLASS()
class WNavigationSubsystem : public WWorldSubsystem
{
    using Super = WWorldSubsystem;
    WINTERS_GENERATED_BODY(WNavigationSubsystem)

public:
    void Initialize(WWorld* pWorld) override
    {
        WWorldSubsystem::Initialize(pWorld);
        m_pNavGrid = std::make_unique<CNavGrid>(200, 200, 0.5f);
    }

    void Deinitialize() override
    {
        m_pNavGrid.reset();
        WWorldSubsystem::Deinitialize();
    }

    void Tick(f32_t fDeltaTime) override
    {
        // NavGrid 업데이트 (구조물 마킹 등)
    }

    const char* GetSubsystemName() const override { return "Navigation"; }
    eTickGroup GetTickGroup() const override { return eTickGroup::PrePhysics; }

    CNavGrid* GetNavGrid() const { return m_pNavGrid.get(); }

    void OnImGui() override
    {
        // NavGrid 디버그 시각화 설정
    }

private:
    WNavigationSubsystem() = default;
    std::unique_ptr<CNavGrid> m_pNavGrid;
};
```

### 4.4 서브시스템 예시: WFxSubsystem

```cpp
// Client/Public/Subsystem/WFxSubsystem.h
#pragma once
#include "World/WWorldSubsystem.h"
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "Renderer/FxStaticMeshRenderer.h"
#include <memory>

WCLASS()
class WFxSubsystem : public WWorldSubsystem
{
    using Super = WWorldSubsystem;
    WINTERS_GENERATED_BODY(WFxSubsystem)

public:
    void Initialize(WWorld* pWorld) override
    {
        WWorldSubsystem::Initialize(pWorld);
        m_pFxSystem = std::make_unique<CFxSystem>();
        m_pFxMeshRenderer = std::make_unique<Engine::CFxStaticMeshRenderer>();
        m_pFxMeshSystem = std::make_unique<CFxMeshSystem>();
    }

    void Tick(f32_t fDeltaTime) override
    {
        m_pFxSystem->Update(fDeltaTime);
        m_pFxMeshSystem->Update(fDeltaTime, m_pFxMeshRenderer.get());
    }

    const char* GetSubsystemName() const override { return "FX"; }
    eTickGroup GetTickGroup() const override { return eTickGroup::PostPhysics; }

    CFxSystem* GetFxSystem() const { return m_pFxSystem.get(); }
    Engine::CFxStaticMeshRenderer* GetFxMeshRenderer() const { return m_pFxMeshRenderer.get(); }

private:
    WFxSubsystem() = default;
    std::unique_ptr<CFxSystem> m_pFxSystem;
    std::unique_ptr<Engine::CFxStaticMeshRenderer> m_pFxMeshRenderer;
    std::unique_ptr<CFxMeshSystem> m_pFxMeshSystem;
};
```

---

## 5. Scene_InGame 책임 분산 매핑

| Scene_InGame 책임 (현재) | 대상 서브시스템 | 줄 수 추정 |
|--------------------------|---------------|-----------|
| CDynamicCamera 생성/업데이트 | WCameraSubsystem | ~50줄 |
| CNavGrid 생성/마킹/쿼리 | WNavigationSubsystem | ~80줄 |
| FxSystem + FxMeshSystem + IreliaBladeSystem + UltWaveSystem | WFxSubsystem | ~150줄 |
| WindWallSystem + ProjectileSystem + PendingHitSystem | WProjectileSubsystem | ~100줄 |
| CClientNetwork + SnapshotApplier + CommandSerializer | WNetworkSubsystem | ~120줄 |
| UpdateTargeting + UpdateCombatInput + 히트 감지 | WCombatSubsystem | ~200줄 |
| NormalPass + SSAOPass + 렌더 순서 | WRenderSubsystem | ~80줄 |
| 7 챔프 ModelRenderer + CTransform + 스폰 | WChampionSpawnSubsystem (→ WActor) | ~100줄 |
| 100+ 튜닝 파라미터 Get/Set | WPROPERTY 자동 노출 (각 서브시스템) | 0줄 (자동) |
| ImGui 디버그 패널 | 각 서브시스템::OnImGui | 분산 |
| **Scene_InGame 잔여** | **WWorld 생성 + 서브시스템 등록** | **~80줄** |

---

## 6. Verification Checklist

```
[ ] WWorld::Create() -> unique_ptr 반환
[ ] WWorld::RegisterSubsystem<T>() -> 타입별 단일 등록
[ ] WWorld::GetSubsystem<T>() -> 등록된 서브시스템 반환
[ ] WWorld::Initialize() -> 모든 서브시스템 Initialize 호출
[ ] WWorld::SpawnActor<T>(pos) -> 액터 생성 + InitializeComponents + BeginPlay
[ ] WWorld::Tick(dt) -> 틱 그룹 순서: PrePhysics → DuringPhysics → PostPhysics → PreRender
[ ] WWorld::Tick(dt) -> 서브시스템 Tick + 액터 Tick 모두 호출
[ ] WWorld::Render() -> 모든 WMeshComponent 자동 수집 + Render
[ ] WWorld::DestroyActor(actor) -> EndPlay + 프레임 끝 제거
[ ] WWorld::Shutdown() -> 액터 전부 EndPlay + 서브시스템 역순 Deinitialize
[ ] WWorld::FindActorByClass<T>() -> 타입 검색
[ ] WWorld::FindActorByNetID(id) -> 네트워크 ID 검색
[ ] WWorldSubsystem::GetTickGroup() -> 올바른 그룹에서 Tick
[ ] WGameInstance::Get() -> 싱글턴 반환
[ ] WGameInstance::ChangeWorld("level") -> 기존 월드 Shutdown + 새 월드 생성
[ ] ECS bridge: WWorld::GetECSWorld() -> 기존 CWorld 접근 (과도기)
[ ] Scene_InGame v2 가 80~100줄 이내
[ ] LoL 빌드 통과 (기존 Scene_InGame 무변경, v2 병존)
```

---

## 7. Migration Strategy

### 7.1 Phase 1: 인프라 구축 (변경 0줄)

- WWorld, WWorldSubsystem, WGameInstance 엔진에 추가
- 기존 코드 무변경, 신규 파일만 추가
- 검증: 빌드 통과

### 7.2 Phase 2: 서브시스템 추출 (점진)

- Scene_InGame 에서 하나의 책임씩 서브시스템으로 추출
- 순서: Camera → Navigation → FX → Projectile → Combat → Network → Render
- 각 추출 후 빌드 + 런타임 검증

### 7.3 Phase 3: 액터 전환

- 7 챔프 ModelRenderer+CTransform → WChampionActor 전환
- SyncECSTransformsFromLegacy 제거
- 렌더 수동 나열 → WWorld::Render 자동 수집

### 7.4 Phase 4: Scene_InGame 교체

- CScene_InGame_v2 도입 (80줄)
- Feature flag 로 전환: `#define WINTERS_USE_NEW_SCENE 1`
- 검증 완료 후 기존 Scene_InGame 아카이브
