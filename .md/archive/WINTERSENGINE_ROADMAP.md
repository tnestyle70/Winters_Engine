# WintersEngine — 개발 로드맵 (Agent 지시문)

> **엔진**: 범용 DX11 C++20 게임 엔진 (하나의 DLL → 여러 Game EXE)
> **첫 번째 타겟**: LoL 30일 모작 — 상세: `.md/.plan/LOL_30DAY_MASTER_PLAN.md`
> **두 번째 타겟**: 엘든링 모작 — LoL 완료 후 진행
> **원칙**: 각 Phase는 독립적으로 검증 가능한 산출물을 가진다.
> 다음 Phase로 넘어가기 전 반드시 체크리스트를 통과해야 한다.
> Agent는 절대 두 Phase를 동시에 진행하지 않는다.
> **전체 Phase 계획**: `winters-skills/TODO.md` 참조

---

## 전체 구조

```
[엔진 기초 ✅]  Phase 0 삼각형 / Phase 1 JobSystem / ECS — 완료
                                    ↓
[LoL 30일 모작]  LOL Phase 0~8 (에셋→렌더→네트워크→백엔드→안티치트→테스트)
                                    ↓
[엘든링 모작]    3인칭카메라, 레벨스트리밍, 보스AI, IK, 세이브/로드
```

아래 Phase 0~1은 **엔진 기초** 구축 당시의 Agent 지시문 (이미 완료).
LoL 30일 모작 Phase는 `LOL_30DAY_MASTER_PLAN.md`에 상세 정의되어 있음.

---

## Phase 0 — 삼각형 띄우기

> **목표**: DX11 컨텍스트 초기화 + 화면에 삼각형 렌더링 확인
> **완료 기준**: 창이 뜨고, 삼각형이 보이고, ESC로 종료된다.

### 0-A. 프로젝트 골격 세팅

**Agent 지시문**
```
Engine/Platform/Window.h|cpp       생성 — Win32 창 생성, 메시지 루프
Engine/RHI/DX11/DX11Device.h|cpp  생성 — ID3D11Device + IDXGISwapChain 초기화
Engine/RHI/DX11/DX11Context.h|cpp 생성 — ID3D11DeviceContext 래핑
WintersEngine.sln / Engine.vcxproj 생성
```

**제약 조건**
- DX11 SDK: Windows SDK 내장본만 사용 (외부 의존성 없음)
- 창 해상도: 1280×720 고정 (이 단계에서 리사이즈 불필요)
- 에러 처리: `HRESULT` 실패 시 `__debugbreak()` + 로그 출력

**구현 순서 (Agent는 이 순서를 지킨다)**
1. `Window` 클래스 — `Create()`, `ProcessMessages()`, `IsRunning()`
2. `DX11Device` — `Init(HWND)`, `GetDevice()`, `GetSwapChain()`
3. `DX11Context` — `GetContext()`, `ClearRenderTarget(color)`, `Present()`
4. `main.cpp` — 세 클래스를 조립하는 최소 게임루프

```cpp
// main.cpp 최종 형태 (Agent 참고용)
int main() {
    Window   window;   window.Create(1280, 720, L"WintersEngine");
    DX11Device  dev;   dev.Init(window.GetHWND());
    DX11Context ctx;   ctx.Init(dev);

    while (window.IsRunning()) {
        window.ProcessMessages();
        ctx.ClearRenderTarget({0.1f, 0.1f, 0.15f, 1.0f});
        ctx.Present();
    }
}
```

**체크리스트**
- [ ] 검은 창이 열린다
- [ ] 지정한 배경색이 보인다
- [ ] ALT+F4 / ESC 로 정상 종료된다
- [ ] Debug Layer 경고 0건

---

### 0-B. 삼각형 렌더링

**Agent 지시문**
```
Engine/RHI/DX11/DX11Shader.h|cpp    생성 — VS/PS 컴파일 및 바인딩
Engine/RHI/DX11/DX11Buffer.h|cpp    생성 — VertexBuffer 생성/바인딩
Engine/RHI/DX11/DX11Pipeline.h|cpp  생성 — InputLayout, RasterizerState
Shaders/Triangle.hlsl               생성 — VS + PS (색상 하드코딩)
```

**제약 조건**
- Vertex 구조체: `{float3 Position, float4 Color}` 고정
- 셰이더 컴파일: 런타임 `D3DCompileFromFile` (오프라인 컴파일 나중에)
- ConstantBuffer: 이 단계에서 불필요 (MVP 행렬 없이 NDC 직접 사용)

```hlsl
// Triangle.hlsl (Agent 참고용)
struct VSInput  { float3 pos : POSITION; float4 col : COLOR; };
struct PSInput  { float4 pos : SV_POSITION; float4 col : COLOR; };

PSInput VS(VSInput v) {
    PSInput o;
    o.pos = float4(v.pos, 1.0f);  // NDC 직접 — 행렬 없음
    o.col = v.col;
    return o;
}
float4 PS(PSInput p) : SV_TARGET { return p.col; }
```

**체크리스트**
- [ ] 흰색(또는 RGB) 삼각형이 화면 중앙에 보인다
- [ ] GPU 메모리 누수 없음 (`DXGIDebug` 종료 시 확인)
- [ ] 셰이더 컴파일 에러 없음

---

### 0-C. RHI 추상화 인터페이스 씌우기

> Phase 0의 DX11 직접 호출을 인터페이스 뒤로 숨긴다.
> 이후 모든 상위 코드는 인터페이스만 본다.

**Agent 지시문**
```
Engine/RHI/RHI_Interface/IRHIDevice.h   생성 — 순수 가상 인터페이스
Engine/RHI/RHI_Interface/IRHIContext.h  생성
Engine/RHI/RHI_Interface/IRHIBuffer.h   생성
Engine/RHI/RHI_Interface/IRHIShader.h   생성
DX11Device, DX11Context 등              IRHIxxx 상속하도록 수정
```

**제약 조건**
- `main.cpp`는 인터페이스 포인터만 사용한다 (`IDX11*` 직접 참조 금지)
- DX11 헤더는 `DX11/` 폴더 내부에서만 `#include`

**체크리스트**
- [ ] `main.cpp`에 `d3d11.h` include가 없다
- [ ] 삼각형이 여전히 보인다 (리그레션 없음)

---

## Phase 1 — JobSystem 구축

> **목표**: 멀티스레드 작업 큐. ECS SystemScheduler가 사용할 기반.
> **완료 기준**: 8개 워커가 Job을 처리하고, 의존성 있는 Job이 올바른 순서로 완료된다.

### 1-A. 기본 스레드 풀

**Agent 지시문**
```
Engine/Core/JobSystem/JobSystem.h|cpp  생성
```

**구현 스펙 (Agent는 이 인터페이스를 정확히 구현한다)**

```cpp
class JobSystem {
public:
    explicit JobSystem(uint32_t threadCount = std::thread::hardware_concurrency());
    ~JobSystem();   // 소멸자에서 모든 워커 join

    // Job 제출 → future 반환
    template<typename Fn>
    std::future<void> Submit(Fn&& fn);

    // 현재 큐의 모든 Job이 완료될 때까지 블로킹
    void WaitAll();

    uint32_t GetThreadCount() const;

private:
    void WorkerLoop();

    std::vector<std::thread>           m_Workers;
    std::queue<std::function<void()>>  m_Queue;
    std::mutex                         m_Mutex;
    std::condition_variable            m_CV;
    std::atomic<int>                   m_ActiveJobs{0};
    bool                               m_bShutdown = false;
};
```

**제약 조건**
- `std::thread`, `std::mutex`, `std::condition_variable` 만 사용 (플랫폼 API 금지)
- Job 실행은 반드시 락 밖에서 한다
- 소멸자에서 `m_bShutdown = true` → `notify_all()` → 모든 워커 `join()`

**체크리스트**
- [ ] `WaitAll()` 이후 미완료 Job이 없다
- [ ] 소멸자에서 데드락 없음
- [ ] 스레드 수 1~16 범위에서 모두 동작

---

### 1-B. Job 의존성 체인 (Counter 기반)

**Agent 지시문**
```
Engine/Core/JobSystem/JobCounter.h  생성 — 완료 카운터 + 대기 메커니즘
JobSystem                           WaitForCounter() 메서드 추가
```

```cpp
// 사용 예시 (Agent가 이 패턴을 통과시켜야 함)
JobCounter* counter = jobs.Submit([]{ /* Job A */ });
jobs.Submit([counter]{
    jobs.WaitForCounter(counter);  // A 완료 후에 실행
    /* Job B — A에 의존 */
}, newCounter);
```

**체크리스트**
- [ ] 의존 Job이 항상 선행 Job보다 늦게 완료된다 (1000회 반복 테스트)
- [ ] 순환 의존성 검출 시 assert 발생

---

### 1-C. JobSystem 단위 테스트

**Agent 지시문**
```
Tests/JobSystemTest.cpp  생성
```

**테스트 항목 (Agent가 모두 작성한다)**

| 테스트명 | 검증 내용 |
|---|---|
| `BasicSubmit` | 100개 Job 제출 → 모두 실행됨 |
| `OrderedCounter` | A→B 의존 체인 순서 보장 |
| `ConcurrentWrite` | 1000개 Job이 atomic counter 증가 → 정확히 1000 |
| `ShutdownClean` | `WaitAll()` 없이 소멸자 호출 → 데드락 없음 |

**체크리스트**
- [ ] 4개 테스트 모두 통과
- [ ] ThreadSanitizer(TSan) 경고 없음

---

## Phase 2 — ECS 구축

> **목표**: Entity/Component/System 전체 구현 + JobSystem 연동
> **완료 기준**: 10,000개 Entity가 PhysicsSystem + RenderSystem으로 60fps 이상 처리된다.

### 2-A. Entity & ComponentStore

**Agent 지시문**
```
Engine/ECS/Entity.h              생성
Engine/ECS/ComponentStore.h      생성  (템플릿, 헤더 온리)
Engine/ECS/World.h|cpp           생성
```

**구현 스펙**

```cpp
// Entity — 단순 ID
using EntityID = uint32_t;
constexpr EntityID NULL_ENTITY = 0;

class EntityManager {
public:
    EntityID Create();
    void     Destroy(EntityID id);
    bool     IsAlive(EntityID id) const;
private:
    uint32_t          m_NextID = 1;
    std::vector<EntityID> m_FreeList;  // 재활용
};

// ComponentStore — Sparse Set 구조
template<typename T>
class ComponentStore {
public:
    void  Add(EntityID id, T component);
    void  Remove(EntityID id);
    T*    Get(EntityID id);          // 없으면 nullptr
    bool  Has(EntityID id) const;

    // 전체 Dense 배열 순회 (캐시 친화적)
    std::span<T>        GetAll();
    std::span<EntityID> GetEntities();

private:
    std::unordered_map<EntityID, size_t> m_Sparse;
    std::vector<T>                       m_Dense;
    std::vector<EntityID>                m_DenseEntities;
};
```

**제약 조건**
- `ComponentStore::Add`는 기존 컴포넌트가 있으면 덮어쓴다 (assert 금지)
- Dense 배열은 `Remove` 시 swap-and-pop으로 구멍을 메운다

**체크리스트**
- [ ] Add → Get → Remove → Get(nullptr) 순서 정상
- [ ] 10,000 Entity Add/Remove 후 메모리 누수 없음
- [ ] swap-and-pop 후 다른 Entity의 데이터가 손상되지 않음

---

### 2-B. Component 정의 (마인크래프트용)

**Agent 지시문**
```
Engine/ECS/Components/Transform.h    생성
Engine/ECS/Components/MeshRenderer.h 생성
Engine/ECS/Components/RigidBody.h    생성
Engine/ECS/Components/Health.h       생성
Engine/ECS/Components/AIState.h      생성
Engine/ECS/Components/PlayerTag.h    생성
Engine/ECS/Components/ChunkData.h    생성
```

**스펙 (모든 Component는 순수 데이터 — 함수 없음)**

```cpp
// Transform.h
struct Transform {
    Vec3 position = {0.f, 0.f, 0.f};
    Vec3 rotation = {0.f, 0.f, 0.f};  // Euler (라디안)
    Vec3 scale    = {1.f, 1.f, 1.f};
};

// Health.h
struct Health {
    float hp    = 20.f;
    float maxHp = 20.f;
    bool  isDead() const { return hp <= 0.f; }  // 유일하게 허용되는 const 함수
};

// RigidBody.h
struct RigidBody {
    Vec3  velocity     = {0.f, 0.f, 0.f};
    Vec3  acceleration = {0.f, -9.8f, 0.f};  // 기본 중력
    float mass         = 1.f;
    bool  isKinematic  = false;
};
```

**제약 조건**
- `#include` 는 표준 타입과 `Math/Vec3.h` 만 허용
- 다른 Component를 포함하지 않는다 (순환 의존 방지)

---

### 2-C. World & Query

**Agent 지시문**
```
Engine/ECS/World.h|cpp  확장 — ComponentStore 통합, Query 구현
```

```cpp
class World {
public:
    // Entity 생성/삭제
    EntityID CreateEntity();
    void     DestroyEntity(EntityID id);

    // Component 접근
    template<typename T> void  AddComponent(EntityID id, T comp);
    template<typename T> void  RemoveComponent(EntityID id);
    template<typename T> T*    GetComponent(EntityID id);
    template<typename T> bool  HasComponent(EntityID id) const;

    // Query: 두 컴포넌트 조합을 가진 Entity 목록 반환
    // 예: world.Query<Transform, RigidBody>()
    template<typename A, typename B>
    std::vector<EntityID> Query();

    // 각 ComponentStore 직접 접근 (System이 내부 순회용)
    ComponentStore<Transform>&    Transforms;
    ComponentStore<RigidBody>&    RigidBodies;
    ComponentStore<MeshRenderer>& Renderers;
    ComponentStore<Health>&       Healths;
    ComponentStore<AIState>&      AIStates;
    // ... 나머지
};
```

**체크리스트**
- [ ] `Query<A, B>()` 가 A와 B 둘 다 없는 Entity를 반환하지 않는다
- [ ] 10,000 Entity Query가 16ms 이내 완료된다

---

### 2-D. System 인터페이스 + SystemScheduler

**Agent 지시문**
```
Engine/ECS/ISystem.h              생성
Engine/ECS/SystemScheduler.h|cpp  생성
```

**구현 스펙**

```cpp
// ISystem.h
using ComponentTypeID = size_t;

struct SystemAccess {
    std::vector<ComponentTypeID> reads;
    std::vector<ComponentTypeID> writes;
};

class ISystem {
public:
    virtual SystemAccess GetAccess() const = 0;
    virtual void         Update(World& world, float dt) = 0;
    virtual ~ISystem() = default;
};

// 타입 ID 생성 유틸
template<typename T>
ComponentTypeID TypeID() {
    static const ComponentTypeID id = reinterpret_cast<ComponentTypeID>(&TypeID<T>);
    return id;
}
```

```cpp
// SystemScheduler.h
class SystemScheduler {
public:
    void Register(std::shared_ptr<ISystem> system);

    // 등록 완료 후 한 번만 호출 — Phase 자동 분류
    // Write-Write 충돌, Write-Read 충돌을 감지해 Phase를 나눈다
    void Build();

    // 매 프레임: Phase 순서대로 실행, 같은 Phase는 JobSystem으로 병렬화
    void Update(World& world, float dt, JobSystem& jobs);

    // 디버그용: 분류된 Phase 목록 출력
    void PrintPhases() const;

private:
    bool HasConflict(const SystemAccess& a, const SystemAccess& b) const;

    std::vector<std::shared_ptr<ISystem>>  m_Systems;
    std::vector<std::vector<ISystem*>>     m_Phases;
};
```

**체크리스트**
- [ ] `Build()` 후 `PrintPhases()` 출력이 아래와 일치한다
  ```
  Phase 0: PhysicsSystem, AISystem, AnimationSystem
  Phase 1: CollisionSystem, AudioSystem
  Phase 2: RenderSystem
  ```
- [ ] Phase 간 barrier가 정확히 동작한다 (Phase 0 완료 전 Phase 1 시작 없음)

---

### 2-E. 마인크래프트 System 구현

**Agent 지시문**
```
Engine/ECS/Systems/PhysicsSystem.h|cpp    생성
Engine/ECS/Systems/RenderSystem.h|cpp     생성
Engine/ECS/Systems/AISystem.h|cpp         생성
Engine/ECS/Systems/HealthSystem.h|cpp     생성
Engine/ECS/Systems/CollisionSystem.h|cpp  생성
```

**각 System의 Access 선언 (Agent는 이를 정확히 선언한다)**

| System | Reads | Writes |
|---|---|---|
| `PhysicsSystem` | — | Transform, RigidBody |
| `AISystem` | Transform | AIState |
| `AnimationSystem` | Transform | AnimState |
| `CollisionSystem` | Transform | CollisionResult |
| `HealthSystem` | CollisionResult | Health |
| `RenderSystem` | Transform, MeshRenderer | — |

**CommandBuffer 패턴 (Entity 생성/삭제는 반드시 이걸 통해)**

```cpp
// System 내부에서 Entity를 지우고 싶을 때
void HealthSystem::Update(World& world, float dt) {
    CommandBuffer cmd;
    for (EntityID id : world.Query<Health>()) {
        Health* hp = world.GetComponent<Health>(id);
        if (hp->isDead()) {
            cmd.DestroyEntity(id);           // 즉시 삭제 X
            cmd.SpawnEffect(id, FX_DEATH);   // 이펙트도 예약
        }
    }
    cmd.Flush(world);   // 프레임 끝에 메인 스레드에서 일괄 처리
}
```

**체크리스트**
- [ ] 10,000 Entity에서 전체 System 1프레임이 16.6ms 이내
- [ ] CommandBuffer Flush 중 다른 System이 실행되지 않음
- [ ] Entity 삭제 후 해당 ID로 Get 시 nullptr 반환

---

### 2-F. ECS 통합 테스트

**Agent 지시문**
```
Tests/ECSTest.cpp  생성
```

**테스트 항목**

| 테스트명 | 검증 내용 |
|---|---|
| `EntityLifecycle` | Create → AddComponent → Query → Destroy |
| `PhysicsIntegration` | RigidBody velocity 적분 후 Transform 변경 확인 |
| `SchedulerPhaseOrder` | Phase 0이 Phase 1보다 항상 먼저 완료 |
| `CommandBufferSafety` | Update 중 Destroy 후 같은 프레임 내 Get → nullptr |
| `PerfBenchmark` | 10,000 Entity × 6 System × 100프레임 < 총 5초 |

**체크리스트**
- [ ] 5개 테스트 모두 통과
- [ ] Valgrind 또는 ASAN 에러 없음

---

## Phase 3 — 에디터 구축

> **목표**: 기존 프로젝트 리소스를 로드해서 씬을 편집할 수 있는 ImGui 기반 에디터
> **완료 기준**: .fbx/.obj 메시와 텍스처를 로드하고, Entity를 씬에 배치하고, Transform을 편집하고, 저장/불러오기가 된다.

### 3-A. ImGui 통합

**Agent 지시문**
```
Tools/WorldEditor/             폴더 생성
External/imgui/                ImGui 소스 추가 (docking branch)
Engine/RHI/DX11/DX11ImGui.h|cpp  생성 — ImGui DX11 백엔드 래핑
```

**제약 조건**
- ImGui 버전: `docking` 브랜치 (멀티 뷰포트, 도킹 지원)
- DX11 백엔드: `imgui_impl_win32.cpp`, `imgui_impl_dx11.cpp` 사용
- ImGui 코드는 `Engine/` 이 아닌 `Tools/` 에서만 참조

**초기 레이아웃 (Agent가 구현할 에디터 창 구성)**

```
┌──────────────────────────────────────────┐
│  Menu: File | Edit | View | Build        │
├─────────────┬────────────────┬───────────┤
│  Hierarchy  │   Viewport     │ Inspector │
│  (씬 트리)  │  (3D 뷰)      │ (속성)    │
│             │                │           │
├─────────────┴────────────────┴───────────┤
│  Asset Browser  (하단)                   │
└──────────────────────────────────────────┘
```

**체크리스트**
- [ ] ImGui 창이 뜨고 드래그/도킹이 된다
- [ ] 뷰포트에 삼각형(Phase 0 결과물)이 보인다
- [ ] `io.WantCaptureMouse` 가 true일 때 씬 카메라가 반응하지 않는다

---

### 3-B. Asset 로드 시스템

**Agent 지시문**
```
Engine/Asset/AssetLoader.h|cpp    생성
Engine/Asset/MeshAsset.h          생성
Engine/Asset/TextureAsset.h       생성
Engine/Asset/AssetRegistry.h|cpp  생성  — 경로 → 에셋 ID 매핑
```

**지원 포맷**

| 종류 | 포맷 | 라이브러리 |
|---|---|---|
| 메시 | .obj, .fbx | Assimp |
| 텍스처 | .png, .jpg, .dds | stb_image / DirectXTex |
| 씬 | .scene (자체 JSON) | nlohmann/json |

**기존 프로젝트 리소스 마이그레이션 규칙**
- 기존 `.x` 파일 → Assimp로 로드 후 `MeshAsset` 으로 변환
- 기존 텍스처 → 경로 그대로 `TextureAsset` 래핑
- 기존 `VIBuffer` 데이터 → `MeshAsset::vertices`, `MeshAsset::indices` 로 이전
- **기존 코드 직접 참조 금지** — 반드시 Asset 시스템을 통해 접근

```cpp
// AssetRegistry 사용 예시
AssetID meshID = AssetRegistry::Load("Resources/Meshes/player.fbx");
MeshAsset* mesh = AssetRegistry::Get<MeshAsset>(meshID);
```

**체크리스트**
- [ ] .obj 파일이 뷰포트에 표시된다
- [ ] 같은 에셋을 두 번 로드해도 메모리에 한 번만 올라간다 (캐싱)
- [ ] 존재하지 않는 경로 → assert + 에러 로그 (크래시 없음)

---

### 3-C. Hierarchy 패널

**Agent 지시문**
```
Tools/WorldEditor/Panels/HierarchyPanel.h|cpp  생성
```

**기능 스펙**

| 기능 | 설명 |
|---|---|
| Entity 목록 표시 | World의 모든 EntityID + 이름 컴포넌트 |
| 선택 | 클릭 → Inspector에 연동 |
| 생성 | 우클릭 → "빈 Entity 생성" / "메시 Entity 생성" |
| 삭제 | Delete 키 → CommandBuffer 통해 삭제 |
| 검색 | 상단 입력창 → 이름 필터링 |

```cpp
// HierarchyPanel.h
class HierarchyPanel {
public:
    void Draw(World& world);
    EntityID GetSelected() const { return m_Selected; }

private:
    void DrawEntity(World& world, EntityID id);
    EntityID m_Selected = NULL_ENTITY;
    char     m_SearchBuf[128] = {};
};
```

**체크리스트**
- [ ] Entity 1,000개에서 패널 렌더링이 60fps 유지 (ImGui clipper 사용)
- [ ] 삭제 후 Hierarchy에서 즉시 사라진다
- [ ] 검색 필터가 실시간으로 동작한다

---

### 3-D. Inspector 패널

**Agent 지시문**
```
Tools/WorldEditor/Panels/InspectorPanel.h|cpp  생성
```

**컴포넌트별 편집 UI**

| Component | 표시 항목 | 편집 방법 |
|---|---|---|
| `Transform` | Position, Rotation, Scale | `DragFloat3` |
| `Health` | HP / MaxHP | `SliderFloat` |
| `RigidBody` | Velocity, Mass, isKinematic | `DragFloat3` + `Checkbox` |
| `MeshRenderer` | Mesh, Material | `AssetPicker` 드롭다운 |
| `AIState` | 현재 상태, 타겟 ID | 읽기 전용 표시 |

```cpp
// 컴포넌트 편집 블록 예시
void InspectorPanel::DrawTransform(Transform* tr) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &tr->position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &tr->rotation.x, 0.5f);
        ImGui::DragFloat3("Scale",    &tr->scale.x,    0.01f, 0.01f, 100.f);
    }
}
```

**체크리스트**
- [ ] Transform 편집 → 뷰포트에 즉시 반영
- [ ] NULL_ENTITY 선택 시 Inspector 비어있음 (크래시 없음)
- [ ] 컴포넌트 없는 항목은 "Add Component" 버튼 표시

---

### 3-E. Viewport 패널 (씬 카메라)

**Agent 지시문**
```
Tools/WorldEditor/Panels/ViewportPanel.h|cpp  생성
Engine/Core/Camera.h|cpp                      생성
```

**카메라 스펙**
- 투영: Perspective, FOV 60°, Near 0.1, Far 10000
- 조작: RMB 홀드 + WASD 이동, 마우스 회전 (에디터 카메라)
- Framebuffer: 별도 RTV에 씬을 렌더링 → ImGui `Image()`로 표시

```cpp
// ViewportPanel.h
class ViewportPanel {
public:
    void Draw(World& world, SystemScheduler& scheduler, JobSystem& jobs);

private:
    void HandleCameraInput(float dt);
    void ResizeIfNeeded(uint32_t w, uint32_t h);

    Camera                    m_Camera;
    IRHITexture*              m_RenderTarget = nullptr;
    ImVec2                    m_LastSize     = {0, 0};
    bool                      m_IsFocused    = false;
};
```

**체크리스트**
- [ ] 뷰포트 리사이즈 시 RTV가 재생성된다
- [ ] 뷰포트 포커스 아닐 때 카메라 입력 무시
- [ ] 씬에 배치된 Entity들이 뷰포트에 보인다

---

### 3-F. 씬 저장/불러오기

**Agent 지시문**
```
Engine/Asset/SceneSerializer.h|cpp  생성  — World → JSON, JSON → World
```

**저장 포맷 (`.scene` — JSON)**

```json
{
  "scene_version": 1,
  "entities": [
    {
      "id": 1,
      "name": "Player",
      "components": {
        "Transform": { "position": [0,64,0], "rotation": [0,0,0], "scale": [1,1,1] },
        "Health":    { "hp": 20, "maxHp": 20 },
        "MeshRenderer": { "mesh": "Resources/Meshes/player.fbx", "material": "Resources/Materials/default.mat" }
      }
    }
  ]
}
```

**제약 조건**
- 저장 시 Asset 경로는 프로젝트 루트 기준 상대경로
- 불러오기 시 Asset이 없으면 경고 로그 + 빈 MeshRenderer로 대체
- 기존 프로젝트 씬 데이터 → Python 변환 스크립트로 `.scene` 포맷 마이그레이션

**체크리스트**
- [ ] 저장 → 재시작 → 불러오기 후 Entity 위치가 동일하다
- [ ] 알 수 없는 컴포넌트 키는 무시하고 나머지는 정상 로드
- [ ] 파일 손상(JSON 문법 오류) 시 크래시 없이 에러 반환

---

### 3-G. 에디터 통합 테스트

**체크리스트**
- [ ] 기존 프로젝트 메시 1개 이상을 씬에 배치하고 저장/불러오기 성공
- [ ] Entity 100개 씬에서 에디터 60fps 유지
- [ ] Hierarchy → Inspector → Viewport 연동이 끊김 없이 동작
- [ ] 메모리 누수 없음 (장시간 실행 후 확인)

---

## 공통 규칙 (Agent 준수 사항)

### 코드 스타일
```
클래스명  : CPascalCase  (예: CGameObject → 기존 방식은 Phase 3 이후 점진 제거)
새 코드   : PascalCase   (예: JobSystem, ComponentStore)
멤버변수  : m_camelCase
정적/전역 : s_camelCase, g_camelCase
상수      : ALL_CAPS
```

### 금지 사항
- `new` / `delete` 직접 사용 → `std::make_unique`, `std::make_shared` 사용
- 전역 싱글톤 → 필요 시 `Engine` 클래스에서 소유하고 참조로 전달
- 헤더에서 `using namespace std` 선언
- Phase 완료 전 다음 Phase 코드 작성

### 브랜치 전략
```
main          — 각 Phase 완료본만 머지
develop       — 일상 작업
feature/ph0-* — Phase 0 작업
feature/ph1-* — Phase 1 작업
feature/ph2-* — Phase 2 작업
feature/ph3-* — Phase 3 작업
```

### 커밋 컨벤션
```
feat(ph0): DX11 디바이스 초기화
feat(ph1): JobSystem 스레드 풀 구현
fix(ph2): ComponentStore swap-and-pop 버그 수정
test(ph2): ECS 통합 테스트 추가
docs: 로드맵 업데이트
```

---

## 빠른 참조 — Phase별 핵심 파일

| Phase | 핵심 파일 | 의존 |
|---|---|---|
| 0-A | `Platform/Window`, `RHI/DX11/DX11Device` | 없음 |
| 0-B | `RHI/DX11/DX11Shader`, `Shaders/Triangle.hlsl` | 0-A |
| 0-C | `RHI/RHI_Interface/IRHI*` | 0-B |
| 1-A | `Core/JobSystem/JobSystem` | 없음 |
| 1-B | `Core/JobSystem/JobCounter` | 1-A |
| 2-A | `ECS/Entity`, `ECS/ComponentStore`, `ECS/World` | 없음 |
| 2-B | `ECS/Components/*` | 2-A |
| 2-C | `ECS/World` (Query 확장) | 2-B |
| 2-D | `ECS/ISystem`, `ECS/SystemScheduler` | 1-A, 2-C |
| 2-E | `ECS/Systems/*` | 2-D |
| 3-A | `Tools/WorldEditor`, ImGui | 0-C |
| 3-B | `Asset/AssetLoader`, Assimp | 3-A |
| 3-C | `Panels/HierarchyPanel` | 3-B, 2-A |
| 3-D | `Panels/InspectorPanel` | 3-C |
| 3-E | `Panels/ViewportPanel`, `Core/Camera` | 3-D |
| 3-F | `Asset/SceneSerializer` | 3-E |

---

## Phase 4 — PvP 게임플레이 (M22-M26)

> **목표**: 멀티플레이어 PvP(vE) 게임 프로토타입
> **타겟**: LoL(MOBA), 이터널 리턴(서바이벌), 배그(배틀로얄)
> **완료 기준**: 3종 챔피언 × 4스킬(Q/W/E/R), AI 미니언, MOBA 게임 루프 1판 플레이 가능

| 태스크 | 산출물 | 의존성 |
|--------|--------|--------|
| 4-A | 3D 레이어 시스템 (Underground/Ground/Aerial) | 2-A (ECS) |
| 4-B | MOBA 게임 루프 (미니언/타워/넥서스/골드) | 4-A |
| 4-C | 챔피언 3종 × Q/W/E/R 스킬 (Lua 5.4) | 4-B |
| 4-D | BehaviorTree AI (미니언/서번트) | 4-B |
| 4-E | NavMesh (레이어별) | 4-A, Physics |
| 4-F | HUD (체력/마나/미니맵/스킬아이콘) | 4-C |
| 4-G | FMOD 3D 사운드 통합 | 독립 |

---

## Phase 5 — 네트워크 + 백엔드 (M27-M30)

> **목표**: 온라인 멀티플레이 동작
> **완료 기준**: 10명 동시 접속, 클라이언트 예측, 매치메이킹 → 게임 시작 → 결과 저장 E2E

| 태스크 | 산출물 | 의존성 |
|--------|--------|--------|
| 5-A | UDP/KCP 트랜스포트 + FlatBuffers | 독립 |
| 5-B | 클라이언트 예측 + 서버 보정 | 5-A |
| 5-C | IOCP 게임 서버 (20~60 TPS) | 5-A |
| 5-D | AOI 그리드 + 지연 보상 | 5-C |
| 5-E | Go Auth + Matchmaking + Leaderboard | 독립 |
| 5-F | Go Payment + Shop + Kafka 통합 | 5-E |
| 5-G | C++ Client SDK (WinHTTP) | 5-E |
| 5-H | 분산 서버 (Gate/Login/Center/Match) | 5-C, 5-E |

---

## Phase 6 — Steam 출시 (M31-M32)

> **목표**: Steam 스토어 출시 + F2P 수익화
> **완료 기준**: 베타 100~500명, Steam 업적/리더보드/친구초대, 배틀패스 결제

| 태스크 | 산출물 | 의존성 |
|--------|--------|--------|
| 6-A | Steamworks SDK 통합 | 5-B |
| 6-B | 게임 런처 + 자동 업데이트 | 6-A |
| 6-C | 베타 테스트 (100~500명) | 6-B |
| 6-D | F2P 모델 (배틀패스/스킨) | 5-F |
| 6-E | 마케팅 + 커뮤니티 | 6-C |
