# Winters Engine — 개발 로드맵

> **엔진**: 범용 DX11 C++20 게임 엔진 (하나의 DLL로 여러 게임 구동)
> **현재 상태**: 엔진 기초 완료 (DX11, Win32, 카메라, 큐브, ECS) → LoL 30일 모작 시작
> **첫 번째 타겟**: LoL 30일 모작 (풀스택 MOBA) — 상세: `.md/.plan/LOL_30DAY_MASTER_PLAN.md`
> **두 번째 타겟**: 엘든링 모작 (액션RPG) — LoL 완료 후 진행
> **아키텍처 상세**: `.md/WINTERS_ENGINE_ARCHITECTURE_FINAL.md`

---

## 코드 작성 규칙

### 1. 구현 전 인터페이스 설계 우선 (Design-First)

> **모든 Step은 코드를 작성하기 전에 "어떻게 사용할 것인가"를 먼저 보여준다.**

```cpp
// ── 나쁜 예: 구현부터 작성 ──────────────────────────────
// CDX11Device.cpp를 먼저 채우고, 나중에 어떻게 쓸지 고민

// ── 좋은 예: 사용 코드부터 설계 ────────────────────────
// main.cpp에서 이렇게 쓰이면 좋겠다:
auto mesh = CVIBuffer::Create(device, vertices, indices);
mesh.Draw(context);
// → 이 코드가 동작하게 하려면 CVIBuffer가 무엇을 가져야 하는가? 를 역산
```

---

### 2. Effective C++ 적용 규칙

| 항목 | 규칙 | 코드 패턴 |
|------|------|-----------|
| Item 04 | 객체는 사용 전 반드시 초기화 | 멤버 변수 선언 시 `= {}` 또는 `= nullptr` 기본값 |
| Item 13 | 자원은 객체로 관리 (RAII) | `ComPtr<T>`, `unique_ptr`, `shared_ptr` 사용 |
| Item 14 | 자원 관리 클래스의 복사 동작 명시 | 복사 금지 시 `= delete`, 이동만 허용 시 move만 구현 |
| Item 18 | 인터페이스는 올바르게 쓰기 쉽고, 잘못 쓰기 어렵게 | `bool` 파라미터 대신 `enum class`, `[[nodiscard]]` 적극 사용 |
| Item 25 | 예외 안전 swap 지원 | 이동 생성자/대입은 `noexcept` 명시 |
| Item 41 | 암시적 인터페이스와 컴파일 타임 다형성 | `template` + `concept`으로 덕타이핑 |

---

### 3. Modern C++20 적용 규칙

```cpp
// ── [[nodiscard]]: 반환값을 무시하면 컴파일 경고 ────────
[[nodiscard]] bool Initialize(const DeviceDesc& desc);

// ── noexcept: 이동 연산은 반드시 ──────────────────────
CVIBuffer(CVIBuffer&& other) noexcept;
CVIBuffer& operator=(CVIBuffer&& other) noexcept;

// ── constexpr: 컴파일 타임 상수 ────────────────────────
static constexpr uint32 MAX_VERTICES = 65536;

// ── std::span: 배열의 비소유 뷰 (복사 없이 전달) ────────
void Upload(std::span<const Vertex> vertices);

// ── Strong Type: bool 파라미터 금지 ────────────────────
// 나쁜 예: Initialize(true, false, true)  → 무슨 뜻인지 모름
// 좋은 예: enum class
enum class VSyncMode  : uint8 { On, Off };
enum class FullScreen : uint8 { Windowed, Fullscreen };

// ── Rule of Zero: 소멸자/복사/이동이 필요 없으면 선언 안 함
// ComPtr이 알아서 Release() 하므로 소멸자 불필요
class CShader {
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVS;
    // 소멸자, 복사, 이동 = 컴파일러가 생성한 것으로 충분
};
```

---

### 4. 엔진 최적화 핵심 규칙

```cpp
// ── 캐시 친화적 데이터 배치: SoA 선호 ──────────────────
// 나쁜 예 (AoS - 캐시 미스 많음):
struct Monster { Vec3 pos; Vec3 rot; float hp; Texture* tex; ... };
Monster monsters[500];

// 좋은 예 (SoA - 이동 시스템이 pos[]만 읽어 캐시 효율 극대화):
struct MonsterPositions  { Vec3  data[500]; };
struct MonsterHealths    { float data[500]; };

// ── Hot Path에서 virtual 금지 ────────────────────────────
// Update()가 매 프레임 수천 번 호출되는 함수라면 virtual 비용이 누적됨
// 해결: CRTP (컴파일 타임 다형성) 또는 Component + System 분리

// ── SIMD: DirectXMath 활용 ───────────────────────────────
// XMVECTOR, XMMATRIX는 SSE2 자동 적용 → float 4개를 동시 연산
#include <DirectXMath.h>
using namespace DirectX;
XMMATRIX world = XMMatrixRotationY(angle) * XMMatrixTranslation(x, y, z);
```

---

## Step 1 — 렌더링 파이프라인 구축 (삼각형 띄우기)

**목표**: 화면에 색상이 있는 삼각형 하나를 렌더링한다.
**의미**: DX11 파이프라인 전체(셰이더 컴파일 → 정점 버퍼 → Draw)가 동작함을 검증.

### 왜 삼각형부터인가

DX9는 `DrawPrimitive()` 한 줄로 그릴 수 있었다. DX11에는 고정 파이프라인이 없다.
삼각형 하나를 그리려면 **Vertex Shader + Pixel Shader + Input Layout + Vertex Buffer + Constant Buffer** 전체가 연결되어야 한다.
이 과정을 통과해야 이후 모든 렌더링 작업이 가능해진다.

### 사용 코드 설계 (구현 전)

```cpp
// Client/Code/CGameApp.cpp — 이렇게 쓰이면 좋겠다

bool CGameApp::OnInit()
{
    auto& device = CEngineApp::Get().GetDevice();

    // 셰이더 로드
    m_pShader = CShader::CreateFromFile(device, L"Shaders/Basic.hlsl");

    // 정점 데이터 (위치 + 색상)
    std::array<Vertex_PC, 3> verts = {{
        { { 0.f,  0.5f, 0.f}, {1.f, 0.f, 0.f, 1.f} },   // 상단 빨강
        { { 0.5f,-0.5f, 0.f}, {0.f, 1.f, 0.f, 1.f} },   // 우하 초록
        { {-0.5f,-0.5f, 0.f}, {0.f, 0.f, 1.f, 1.f} },   // 좌하 파랑
    }};
    m_pVBuffer = CVIBuffer::Create(device, verts);

    return true;
}

void CGameApp::OnRender()
{
    auto& ctx = CEngineApp::Get().GetDevice().GetContext();
    m_pShader->Bind(ctx);
    m_pVBuffer->Draw(ctx);
}
```

### 기술 스택 선택 이유

| 항목 | 선택 | 이유 |
|------|------|------|
| 셰이더 컴파일 | `D3DCompileFromFile()` 런타임 컴파일 | 개발 단계에서 HLSL 수정 즉시 반영 가능. 배포 시 사전 컴파일(`.cso`)으로 전환 |
| 상수 버퍼 | `ID3D11Buffer` + `Map/Unmap` | 매 프레임 행렬 갱신이 필요. Dynamic 버퍼 + Map이 가장 효율적 |
| 정점 형식 | 목적별 구조체 (`Vertex_PC`, `Vertex_PTN`) | 범용 `Vertex`보다 메모리 절약, 셰이더 InputLayout이 명확해짐 |

### 새로 추가되는 파일 구조

```
Engine/
├── Header/
│   ├── Renderer/
│   │   ├── CShader.h        // VS + PS 페어, 컴파일 + 바인딩
│   │   ├── CVIBuffer.h      // Vertex/Index Buffer 추상화
│   │   └── CConstantBuffer.h // 상수 버퍼 템플릿 래퍼
│   └── Math/
│       └── WintersMath.h    // DirectXMath 래핑 타입 (Vec2, Vec3, Vec4, Mat4)
└── Code/
    └── Renderer/
        ├── CShader.cpp
        ├── CVIBuffer.cpp
        └── CConstantBuffer.cpp

Client/
└── Shaders/
    ├── Basic.hlsl           // 가장 기본 VS + PS
    └── Common.hlsli         // 공용 상수 버퍼 구조체 정의
```

---

## Step 2 — 카메라 시스템 (CTransform, CCamera, CDynamicCamera)

**목표**: 3D 공간에서 오브젝트를 배치하고, 카메라로 바라보는 기본 씬을 구성한다.

### 사용 코드 설계 (구현 전)

```cpp
// 트랜스폼: 위치/회전/스케일 → 월드 행렬
CTransform transform;
transform.SetPosition({ 0.f, 0.f, 5.f });
transform.SetRotationY(XM_PIDIV4);          // 45도
Mat4 worldMatrix = transform.GetWorldMatrix();

// 카메라: 뷰 + 투영 행렬 생성
CCamera camera;
camera.SetPerspective(XM_PIDIV4, 16.f/9.f, 0.1f, 1000.f);
camera.LookAt({ 0.f, 5.f, -10.f }, { 0.f, 0.f, 0.f });
Mat4 viewProj = camera.GetViewProjection();

// 다이나믹 카메라: 플레이어 추적
CDynamicCamera dynCam;
dynCam.SetTarget(&playerTransform);
dynCam.SetArmLength(15.f);   // 스프링 암 길이
dynCam.SetPitch(-30.f);      // 탑다운 각도 (LoL 스타일)
```

### CTransform 설계 원칙

```cpp
// ── Dirty Flag 패턴: 변경된 경우에만 행렬 재계산 ──────
// 매 프레임 SetPosition 없이도 GetWorldMatrix() 호출 시
// 내부적으로 캐싱된 값 반환 → 불필요한 XMMATRIX 곱셈 제거
class CTransform
{
public:
    void SetPosition(Vec3 pos) { m_Position = pos; m_bDirty = true; }
    const Mat4& GetWorldMatrix();   // Dirty이면 재계산, 아니면 캐시 반환

private:
    Vec3  m_Position = {};
    Vec3  m_Rotation = {};          // 오일러 각 (라디안)
    Vec3  m_Scale    = {1.f,1.f,1.f};
    Mat4  m_WorldMatrix = {};       // 캐싱된 월드 행렬
    bool  m_bDirty = true;          // 갱신 필요 여부
};
```

### 기술 스택 선택 이유

| 항목 | 선택 | 이유 |
|------|------|------|
| 수학 라이브러리 | `DirectXMath` (XMVECTOR/XMMATRIX) | SSE2 SIMD 내장. 별도 라이브러리 없이 DX SDK에 포함 |
| 회전 표현 | 오일러(내부) + 쿼터니언(보간) | 편집은 오일러가 직관적, 애니메이션 보간은 쿼터니언 필수 (Gimbal Lock 방지) |
| 카메라 투영 | Perspective (원근) 기본, Ortho는 UI/에디터 전용 | 게임 씬은 원근, 에디터 top-view는 직교 |

---

## Step 3 — CubeTex / CVIBuffer / 텍스처 로딩 (DDS, PNG)

**목표**: 텍스처가 입혀진 큐브를 3D 공간에 배치한다.

### 사용 코드 설계 (구현 전)

```cpp
// 큐브 메시 + 텍스처 조합
auto pCube = std::make_unique<CCubeTex>();
pCube->Initialize(device, L"Textures/stone.dds");
pCube->GetTransform().SetPosition({ 2.f, 0.f, 0.f });

// OnUpdate에서
pCube->Update(deltaTime);

// OnRender에서
pCube->Render(context, camera.GetViewProjection());
```

### 텍스처 로딩 — DDS vs PNG

```
DDS (DirectDraw Surface):
  - GPU에 그대로 업로드 가능 (BC1~BC7 압축 포맷)
  - CPU 압축 해제 불필요 → 로딩 빠름
  - Mipmap 내장
  - 게임 최종 빌드에서는 DDS 사용 (에셋 파이프라인 필수)

PNG:
  - 원본 편집 포맷 (포토샵/블렌더 출력)
  - GPU 업로드 전 RGBA로 변환 필요 (CPU 비용 발생)
  - 개발 단계 빠른 테스트용으로 사용
```

### 기술 스택 선택 이유

| 항목 | 선택 | 이유 |
|------|------|------|
| 텍스처 로딩 | `DirectXTex` (MS 공식 오픈소스) | DDS/PNG/HDR/TGA/BMP 전부 지원. WIC 백엔드로 PNG 디코딩 |
| GPU 압축 | BC1 (불투명), BC3 (알파), BC7 (고품질) | VRAM 사용량 4~6배 절감. GPU가 직접 압축 해제 |
| Mipmap 생성 | `DirectXTex::GenerateMipMaps()` | 원거리 텍스처 앨리어싱 방지, 성능 향상 |

### 새로 추가되는 파일 구조

```
Engine/
├── Header/
│   └── Renderer/
│       ├── CTexture.h       // ID3D11ShaderResourceView 래퍼
│       └── CCubeTex.h       // 텍스처 큐브 (CVIBuffer + CTexture 조합)
└── Code/
    └── Renderer/
        ├── CTexture.cpp
        └── CCubeTex.cpp

Client/
├── Textures/                // 게임용 텍스처 에셋
│   ├── stone.dds
│   └── grass.dds
└── Shaders/
    └── Textured.hlsl        // 텍스처 샘플링 셰이더
```

---

## Step 4 — 에디터 구축 (ImGui + 블록 피킹 + 맵 생성)

**목표**: 에디터 모드에서 ImGui로 블록을 선택하고 3D 공간에 배치해 맵을 만든다.
**방향**: 팀원들이 몬스터/플레이어를 쌓기 전에 먼저 완성되어야 할 기반.

### 사용 코드 설계 (구현 전)

```cpp
// 에디터 UI 예시
void CEditorApp::OnRender()
{
    // ImGui 에디터 패널
    ImGui::Begin("Block Palette");
    for (auto& blockType : BlockRegistry::GetAll())
    {
        if (ImGui::ImageButton(blockType.thumbnail, {48, 48}))
            m_SelectedBlock = blockType.id;
    }
    ImGui::End();

    ImGui::Begin("World Properties");
    ImGui::DragFloat3("Sun Direction", &m_SunDir.x, 0.01f);
    ImGui::ColorEdit3("Ambient Color", &m_AmbientColor.x);
    ImGui::End();

    // 3D 뷰포트에 그리드 오버레이
    m_WorldEditor.RenderGrid(context, camera);
    m_WorldEditor.RenderSelectedBlock(context, m_PickResult);
}

// 마우스 피킹: 스크린 좌표 → 월드 레이 → 블록 충돌 판정
void CEditorApp::OnUpdate(float32 dt)
{
    Ray ray = m_Camera.ScreenToRay(Input::GetMousePos());
    m_PickResult = m_WorldEditor.Pick(ray);

    if (Input::IsMousePressed(MouseButton::Left) && m_PickResult.hit)
        m_WorldEditor.PlaceBlock(m_PickResult.position, m_SelectedBlock);
}
```

### 피킹 구현 방식

```
레이-AABB 교차 판정 (Ray vs Axis-Aligned Bounding Box):

1. 스크린 좌표 → NDC → 역투영(Inverse Projection) → 뷰 공간 레이
2. 뷰 공간 레이 → 역뷰(Inverse View) → 월드 공간 레이
3. 월드 공간 레이와 각 블록 AABB 교차 판정
4. 가장 가까운 블록 반환

최적화: QuadTree / Octree로 교차 후보 먼저 좁힘
         전체 블록 순회 O(n) → O(log n)
```

### 기술 스택 선택 이유

| 항목 | 선택 | 이유 |
|------|------|------|
| 에디터 UI | `Dear ImGui` + DX11 백엔드 | 헤더 온리에 가까운 즉시 모드 GUI. 엔진 내부에 통합이 매우 쉬움 |
| 에디터/게임 분리 | `enum class AppMode { Game, Editor }` | 같은 빌드에서 F1로 토글 가능. 별도 exe 불필요 |
| 맵 저장 형식 | 바이너리 + JSON 메타 | 바이너리: 로딩 빠름 / JSON: 사람이 읽을 수 있어 버전 관리(Git)에 유리 |
| 그리드 스냅 | 고정 단위 스냅 (0.5f 배수) | 블록 기반 맵에서 정밀 배치보다 그리드 정렬이 제작 속도에 유리 |

### 새로 추가되는 파일 구조

```
Engine/
├── Header/
│   └── Editor/
│       ├── CImGuiRenderer.h     // ImGui DX11 초기화 + 렌더
│       └── CRayPicker.h         // 스크린→월드 레이 변환 + 피킹

Client/
├── Header/
│   ├── CEditorApp.h             // IWintersApp 에디터 구현체
│   └── CWorldEditor.h           // 블록 배치/삭제/저장 로직
└── Code/
    ├── CEditorApp.cpp
    └── CWorldEditor.cpp
```

---

## Step 5 — 플레이어 & 몬스터 구현

**목표**: 플레이어가 이동하고, 몬스터가 AI로 추적 및 공격한다.

### 사용 코드 설계 (구현 전)

```cpp
// 플레이어: 컴포넌트 조합으로 구성
class CPlayer : public CGameObject
{
public:
    bool OnInit() override
    {
        AddComponent<CTransform>();
        AddComponent<CCapsuleCollider>({0.5f, 1.8f});
        AddComponent<CCharacterController>();
        AddComponent<CSkeletalMesh>(L"Models/Player.fbx");
        AddComponent<CAnimBlendTree>(L"Anims/Player.anim");
        return true;
    }
};

// 몬스터: FSM (유한 상태 머신) 기반 AI
class CMonster : public CGameObject
{
    enum class State { Idle, Chase, Attack, Dead };

    void OnUpdate(float32 dt) override
    {
        switch (m_State)
        {
        case State::Idle:
            if (CanSeePlayer())  TransitionTo(State::Chase);
            break;
        case State::Chase:
            MoveToward(m_pTarget, dt);
            if (InAttackRange()) TransitionTo(State::Attack);
            break;
        case State::Attack:
            PerformAttack();
            break;
        }
    }
};
```

### FSM vs Behavior Tree 선택

```
FSM (유한 상태 머신):
  - 상태 수가 적은 단순 AI에 적합 (슬라임, 기본 근접 몬스터)
  - 구현 빠름, 디버깅 쉬움
  - 상태가 10개 이상이면 전이 조건 관리가 복잡해짐

Behavior Tree:
  - 복잡한 AI에 적합 (보스, 플레이어 유사 행동)
  - 모듈 재사용 가능 (Patrol 노드 = 모든 몬스터가 공유)
  - 에디터에서 시각적으로 편집 가능

Winters Engine 방향:
  - 일반 몬스터: FSM
  - 보스 / 복잡한 AI: Behavior Tree
  - 두 방식 모두 CGameObject 위에서 Component로 부착 가능하게 설계
```

### 기술 스택 선택 이유

| 항목 | 선택 | 이유 |
|------|------|------|
| 물리 / 충돌 | `Jolt Physics` (MIT 라이선스) | 멀티스레드 설계, Bullet보다 성능 우수, Roblox / Horizon 사용 |
| 애니메이션 | Skeletal Mesh + Blend Tree | 걷기/달리기 속도에 따른 자연스러운 블렌딩. IK는 Phase 이후 |
| 경로 탐색 | NavMesh + A* | 던전스 프로젝트의 A* 코드를 NavMesh 기반으로 업그레이드 |

---

## Step 6 — 씬 전환 & 네트워크

**목표**: 멀티플레이어 대전이 가능한 씬 전환 흐름과 UDP 네트워크를 구축한다.

### 씬 전환 설계 (구현 전)

```cpp
// 씬 매니저: 전환 요청 → 페이드 아웃 → 로딩 → 페이드 인
CSceneManager::Get().ChangeScene<CGameScene>(
    SceneTransition::FadeBlack, 0.5f  // 0.5초 페이드
);

// 씬 인터페이스
class CGameScene : public IScene
{
public:
    void OnEnter() override;     // 씬 진입 시 리소스 로드
    void OnExit()  override;     // 씬 이탈 시 리소스 해제
    void OnUpdate(float32 dt) override;
    void OnRender() override;
};
```

### 네트워크 설계 원칙

```
서버 권위 (Server Authoritative):
  클라이언트는 입력만 전송, 게임 결과는 서버가 결정
  → 치팅 방지의 기본

클라이언트 예측 (Client-Side Prediction):
  서버 응답 전에 로컬에서 먼저 이동 실행 → 반응성 확보
  서버 결과가 오면 비교 → 차이가 있으면 보정 (Reconciliation)

관심 영역 관리 (Interest Management):
  각 클라이언트는 자신 주변의 오브젝트 상태만 수신
  전체 브로드캐스트 O(n²) → 공간 분할 O(log n)
```

### 기술 스택 선택 이유

| 항목 | 선택 | 이유 |
|------|------|------|
| 전송 계층 | UDP + 자체 신뢰성 레이어 | TCP는 재전송 지연이 게임에서 치명적. UDP로 속도 확보 + 중요 패킷만 ACK |
| 직렬화 | FlatBuffers (구글) | 파싱 없이 바이트 직접 접근. PacketDef.h 패턴보다 안전하고 빠름 |
| 서버 틱레이트 | 20 TPS (로직), 60 TPS (이동 보간) | LoL 30 TPS, Valorant 128 TPS. 시작은 20으로 검증 후 상향 |
| 매칭 | Redis 대기열 + Go 매칭 서비스 | 게임 서버와 분리. 독립 스케일링 가능 |

---

## 전체 의존성 그래프 — LoL 30일 모작 Phase 매핑

```
[엔진 기초 ✅] 삼각형 → 카메라 → 큐브 → ECS
    │
    ▼
[Phase 0] 에셋 파이프라인
    │  CMeshLoader, CAnimLoader, CTextureLoader (.wmesh/.wanim/.wmat)
    ▼
[Phase 1] 코어 강화
    │  Fiber JobSystem, Allocator, EventBus
    ▼
[Phase 2] Deferred Pipeline
    │  RenderGraph, G-Buffer, Clustered Lighting, CSM, PostFX
    ▼
[Phase 3] GPU-Driven & Profiling
    │  GPU Cull CS, IndirectDraw, CProfiler, DisplaySettings
    ▼
[Phase 4] 네트워크 & 게임 서버
    │  UDP/KCP, FlatBuffers, IOCP, GameRoom, AOI, 클라이언트 예측
    ▼
[Phase 5] Go 백엔드
    │  Auth, Shop, Matchmaking, Profile, Kafka 이벤트
    ▼
[Phase 6] 안티치트
    │  커널 드라이버, 유저모드 서비스, 서버사이드 검증
    ▼
[Phase 7] 에디터 & 콘텐츠
    │  ImGui, 소환사의 협곡, Lua 챔피언
    ▼
[Phase 8] 통합 테스트
```

**LoL 이후 → 엘든링 확장:**
```
[엘든링] 3인칭 카메라, 레벨 스트리밍, 보스 AI(BT), IK 애니메이션, 세이브/로드
```

---

## 기술 부채 관리

| 현재 | 향후 교체 대상 | 교체 시점 |
|------|---------------|-----------|
| `CGameObject` OOP 계층 | ECS (이미 워크트리에 구현됨) | main 머지 시 |
| `OnRender()` 직접 호출 | RenderGraph (Pass 등록 방식) | Phase 2 Deferred 도입 시 |
| DX11 직접 사용 | RHI 인터페이스 추상화 | DX12 백엔드 추가 시 (엘든링 이후) |
| `PacketDef.h` 수동 직렬화 | FlatBuffers | Phase 4 네트워크 시 |
| Worker Thread Pool | Fiber JobSystem | Phase 1 코어 강화 시 |

> **핵심 원칙**: 지금 당장 필요하지 않은 추상화는 만들지 않는다.
> 필요해지는 순간이 올 때, 그 이유를 몸으로 알게 된 후 교체한다.

---

*Last Updated: 2026-04-12 | Winters Engine — LoL 30일 모작 시작*
