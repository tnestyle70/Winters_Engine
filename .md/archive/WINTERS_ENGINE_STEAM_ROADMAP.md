# Winters Engine — 듀얼 프로젝트 마스터 로드맵
## 범용 엔진 DLL → LoL 30일 모작 → 엘든링 모작 | 솔로 개발

> **전략 변경**: 기존 32개월 단일 게임 출시 → 듀얼 프로젝트 (LoL 먼저, 엘든링 이후)
> **핵심**: 하나의 WintersEngine.dll을 두 게임 EXE가 공유
> **LoL 상세 플랜**: `.md/.plan/LOL_30DAY_MASTER_PLAN.md`

---

## 목차

1. 전체 개요 및 전략 (v2 — 듀얼 프로젝트)
2. Phase 0 — 준비 & 환경 구성 (M0~M1) [완료]
3. Phase 1 — Core Foundation + RHI (M2~M9) [완료 — 기초]
4. **LoL 30일 모작** — LOL Phase 0~8 (D0~D29)
5. **엘든링 모작** — Elden Phase E-1~E-5 (LoL 완료 후)
6. 예산 전체 명세
7. 리스크 관리 & 비상 계획
8. 핵심 기술 결정 사항 요약

---

## 1. 전체 개요 및 전략

### 목표
Winters Engine(DirectX 11 기반, RHI 추상화, ECS 구조)을 범용 게임 엔진으로 구축하고,
두 개의 게임 프로젝트를 순차적으로 진행한다:
1. **LoL 30일 모작**: 풀스택 MOBA (클라이언트 + 서버 + 백엔드 + 안티치트)
2. **엘든링 모작**: 액션RPG (LoL에서 검증된 엔진 위에 구축)

### 핵심 전략: "LoL로 엔진 검증 → 엘든링으로 확장"
- LoL 모작이 엔진의 핵심 시스템(ECS, 네트워크, 렌더링, 서버)을 30일 안에 관통 검증
- 엘든링은 LoL에서 누락된 시스템(3인칭 카메라, 레벨 스트리밍, 보스 AI)을 추가
- 두 프로젝트가 동일한 WintersEngine.dll을 공유 → 엔진 코드 중복 없음

### 개발 원칙
- **RHI 불변성**: `ID3D11Device*`는 RHI 레이어 아래에서만 존재. 위로 절대 노출 금지
- **ECS 순수성**: `CGameObject` 상속 계층 재건축 금지. Entity는 `uint64_t`, Component는 순수 데이터
- **데이터 드리븐 스킬**: Lua 5.4로 정의. 엔진 재빌드 없이 밸런스 패치 가능
- **결제 신뢰성**: 서버사이드 영수증 검증 필수. 결제 데이터는 PostgreSQL 불변 트랜잭션 로그
- **엔진 범용성**: Game Layer에 장르 특화 로직 격리. 엔진 레이어는 장르를 모른다

---

## 2. Phase 0 — 준비 & 환경 구성 (M0~M1, 1개월)

### 목표
개발 환경 완비 + DX11 기초 API 체화

### 상세 작업 목록

**환경 세팅**
- Visual Studio 2022 Community + Windows 11 SDK (최신) 설치
- CMake 3.28+ 설치, CMakePresets.json으로 Debug/Release/Profile 프리셋 구성
- Git 저장소 초기화 (`WintersEngine/`), Git LFS 설정 (.fbx, .png, .wav 확장자)
- GitHub Actions 기본 CI 파이프라인 구성 (빌드 성공 여부 자동 확인)

**디버깅 도구 설치 및 기본 숙지**
- RenderDoc: DX11 프레임 캡처, G-Buffer 확인 방법 익히기
- PIX for Windows: GPU 타이밍, 파이프라인 통계 읽는 법 익히기
- Visual Studio Profiler: CPU 샘플링 기본 사용법

**DX11 기초 직접 구현 (100% 직접 타이핑)**
- `D3D11CreateDevice` + `IDXGISwapChain` 생성
- `ID3D11RenderTargetView` 생성 + `ClearRenderTargetView` + `Present`
- 버텍스 버퍼, 인덱스 버퍼로 삼각형 출력
- 상수 버퍼로 MVP 행렬 전달 + 기초 HLSL 버텍스/픽셀 셰이더 작성
- 텍스처 로딩 + `ID3D11SamplerState`

**학습 완료 기준**
- 윤성우 TCP/IP 책 완독 + IOCP 에코서버 (Linux + Windows 각각) 완성
- 마인크래프트 던전스 수업 프로젝트 완성 및 코드 리뷰 문서화

### 마일스톤
DX11로 텍스처가 붙은 회전하는 큐브 화면 출력. IOCP 에코서버 정상 동작.

### 예산
거의 없음 (도구 전부 무료). GitHub Pro 월 $4 시작.

---

## 3. Phase 1 — Core Foundation + RHI (M2~M9, 8개월)

### 목표
Winters Engine의 뼈대 전체 구축. 이 위에 모든 시스템이 올라간다.

### 월별 세부 계획

**M2~M3: Platform Layer + Memory**

Platform Layer:
```
WintersWindow  : HWND 생성, WndProc, 리사이즈 처리
WintersThread  : std::thread 래핑, 스레드 이름 설정
WintersFile    : ReadFileAsync, WriteFileAsync (IOCP 기반)
WintersClock   : QueryPerformanceCounter 래핑, DeltaTime 계산
```

Memory Allocator:
```
PoolAllocator  : 고정 크기 블록 (Component, Packet 등용)
               - FreeList 방식, 스레드 안전 버전 별도 구현
FrameAllocator : 프레임 끝 일괄 해제, 선형 할당
               - 더블 버퍼링 (GPU 프레임 지연 대응)
StackAllocator : LIFO, 레벨 로딩/언로딩용
```

검증 방법: 1,000,000회 할당/해제 벤치마크, `new/delete` 대비 성능 측정

**M4~M5: Job System + Math**

Job System (Fiber 기반):
```
Fiber          : Windows CreateFiber / SwitchToFiber 래핑
JobDeclaration : 함수 포인터 + 파라미터 + 의존성 카운터
JobQueue       : Lock-free MPMC 큐
WorkerThread   : 코어당 1개, 항상 다음 실행 가능 Job 탐색
CounterHandle  : WaitForCounter로 의존성 해소 대기
```

구현 참고: Naughty Dog GDC 2015 "Parallelizing the Naughty Dog Engine" (공개 자료)

SIMD Math Library:
```
Vec2, Vec3, Vec4  : __m128 기반 SSE2 연산
Mat4x4            : 4×4 행렬, SSE 역행렬
Quaternion        : 구면 선형 보간(SLERP), 오일러 변환
AABB, Sphere      : 충돌 테스트 기초
DirectXMath       : 위 구조체 내부에서 래핑하여 사용
```

**M6~M7: RHI + DX11 Backend**

RHI 인터페이스 정의:
```cpp
// 엔진 코드 어디에서도 ID3D11* 타입이 보이면 안 됨
class IRHITexture    { virtual ~IRHITexture() = default; };
class IRHIBuffer     { virtual ~IRHIBuffer() = default; };
class IRHIPipeline   { virtual ~IRHIPipeline() = default; };
class IRHICommandList{ virtual ~IRHICommandList() = default; };
class IRHIDevice {
public:
    virtual IRHITexture*    CreateTexture(const RHITextureDesc&)    = 0;
    virtual IRHIBuffer*     CreateBuffer(const RHIBufferDesc&)      = 0;
    virtual IRHIPipeline*   CreatePipeline(const RHIPipelineDesc&)  = 0;
    virtual IRHICommandList*GetCommandList()                        = 0;
    virtual void            Submit(IRHICommandList*)                = 0;
    virtual void            Present()                               = 0;
};
```

DX11 구현체:
```
D3D11Texture  : IRHITexture  구현 (ID3D11Texture2D 내부 보유)
D3D11Buffer   : IRHIBuffer   구현 (ID3D11Buffer 내부 보유)
D3D11Pipeline : IRHIPipeline 구현 (VS/PS/InputLayout/RS/OM 상태 묶음)
D3D11CmdList  : IRHICommandList 구현 (ID3D11DeviceContext 래핑)
```

**M8~M9: ECS + EventBus + 기본 렌더 루프**

ECS 구현 (Archetype 기반):
```
Entity         : uint64_t (상위 32bit = 세대, 하위 32bit = 인덱스)
ComponentStore : Archetype 단위 SoA 배열
               - 같은 Component 조합 Entity들이 연속 메모리에 저장됨
SystemScheduler: 시스템 의존성 그래프 → JobSystem에 제출
```

EventBus:
```
Event<T>       : 타입 안전 이벤트 래퍼
EventBus       : Subscribe<T>(handler) / Publish<T>(event)
               - 프레임 단위 지연 발행 옵션 (렌더 스레드 안전)
```

기본 렌더 루프:
- 메시 + 텍스처 + 기초 방향 조명으로 캐릭터 출력
- 게임 로직 스레드 / 렌더 스레드 분리
- CommandList 더블 버퍼링 (프레임 N에서 기록, 프레임 N+1에서 제출)

### 마일스톤
Winters Engine으로 마인크래프트 던전스 수준의 게임(캐릭터 이동 + 충돌 + 카메라) 재현 가능.

### 예산 (~80만원)
- JetBrains Rider 시작 (선택): 월 $24.90 × 8개월 = ~27만원
- GitHub Pro: 월 $4 × 8개월 = ~4만원
- AWS EC2 t3.micro (개발 서버): 월 $10 × 8개월 = ~11만원
- 도서/학습 자료: ~20만원
- 기타: ~18만원

---

## 4. Phase 2 — 렌더 엔진 완성 (M10~M15, 6개월)

### 목표
눈으로 보기에 LoL/Valorant 수준 시각 품질 달성.

### 월별 세부 계획

**M10~M11: Render Graph + Deferred Pipeline**

Render Graph:
```
RenderPass     : 입출력 리소스 선언, 실행 함수 등록
RenderGraph    : Pass 의존성 분석 → 실행 순서 결정 → 불필요 Pass 컬링
ResourceHandle : 가상 리소스 핸들 → 실제 RHITexture에 매핑
               - 자동 Resource Barrier 삽입 (Read→Write 전환 시)
```

Deferred Rendering:
```
G-Buffer Pass:
  RT0: Albedo(RGB) + Roughness(A)         — DXGI_FORMAT_R8G8B8A8_UNORM
  RT1: World Normal(RGB) + Metallic(A)    — DXGI_FORMAT_R16G16B16A16_FLOAT
  RT2: Motion Vector(RG) + AO(B) + Emissive(A) — DXGI_FORMAT_R16G16B16A16_FLOAT
  Depth: DXGI_FORMAT_D32_FLOAT

Lighting Pass (Compute Shader):
  - G-Buffer 읽기 → 각 픽셀 PBR 조명 계산
  - Point Light는 Clustered 방식으로 처리
```

**M12~M13: Clustered Shading + Shadow + Animation**

Clustered Deferred Shading:
```
화면을 X×Y×Z 클러스터로 분할 (예: 16×8×24)
Compute Pass: 각 클러스터에 영향을 주는 광원 목록 사전 계산
Lighting Pass: 픽셀이 속한 클러스터의 광원 목록만 순회
결과: 광원 200개도 프레임 비용 일정 유지
```

Cascaded Shadow Map (CSM):
```
Cascade 수: 4개 (Near 0~10m, Mid 10~50m, Far 50~200m, VeryFar 200m~)
각 Cascade: 별도 뎁스맵 렌더 + PCF 소프트 섀도우
자동 Cascade 분할 (PSSM 알고리즘)
```

Skeletal Animation + Blend Tree:
```
SkeletonAsset  : 뼈대 계층 구조, 바인드 포즈 행렬
AnimationClip  : 키프레임 채널 (위치/회전/스케일)
BlendTree      : 1D/2D 블렌드, Additive 레이어
AnimationSystem: Job으로 병렬 스키닝 (skinning = 행렬 팔레트 × 버텍스)
```

**M14~M15: PostFX + Asset Pipeline**

PostProcessing:
```
TAA (Temporal Anti-Aliasing):
  - 매 프레임 서브픽셀 Jitter (Halton 시퀀스)
  - 이전 프레임 재투영 (Motion Vector 필수)
  - 히스토리 클램핑으로 고스팅 제거
  - 주의: Motion Vector Pass를 G-Buffer와 동시에 출력해야 함

SSAO (Screen Space Ambient Occlusion):
  - 반구 샘플링 (64샘플), 블러 패스
  - G-Buffer Normal + Depth 입력

Bloom:
  - Dual Kawase 블러 방식 (16× 다운샘플 → 업샘플)
  - Threshold → 블러 → Additive 합성

Color Grading:
  - LUT(Look-Up Table) 3D 텍스처 기반
  - 아티스트가 Adobe SpeedGrade에서 출력한 LUT 사용
```

Asset Pipeline:
```
FBX/glTF 임포터 → WintersEngine 내부 바이너리 포맷(.wmesh, .wanim)
텍스처: DXT1/BC7 압축 자동 변환
에셋 핫 리로딩: 파일 변경 감지 → 런타임 리로드
런타임 에셋 스트리밍: 거리 기반 LOD 비동기 로드/언로드
```

### 마일스톤
Winters Engine에서 PBR 재질 캐릭터가 CSM 그림자와 함께 부드럽게 애니메이션되는 장면 출력. PIX 프로파일링으로 각 Pass GPU 시간 확인.

### 예산 (~150만원)
- JetBrains Rider: 월 $24.90 × 6 = ~20만원
- GitHub Pro: 월 $4 × 6 = ~3만원
- AWS 개발 서버: 월 $15 × 6 = ~14만원
- 3D 모델 에셋 (검증용): ~50만원
- 텍스처/PBR 재질 에셋: ~30만원
- 음악/SFX 라이선스 (검증용): ~20만원
- 기타: ~13만원

---

## 5. Phase 3 — 검증 게임: 엘든링 모작 (M16~M21, 6개월)

### 목표
Winters Engine 위에서 싱글 플레이어 액션 RPG 프로토타입을 만들어 엔진 약점을 전부 발굴한다.

### 검증 대상 엔진 기능

| 게임 기능 | 검증 엔진 기능 |
|---|---|
| 히트박스 정밀 전투 | FSM, 프레임 단위 충돌, Physics |
| 복잡한 캐릭터 모션 | Blend Tree, IK, AnimEvent |
| 광활한 맵 | Level Streaming, LOD, Occlusion |
| 락온 시스템 | 카메라 시스템, Target Locking |
| 인벤토리 UI | UI 렌더링, 데이터 바인딩 |

### 월별 세부 계획

**M16~M17: 캐릭터 컨트롤러 + FSM**

```
CharacterController:
  - 캡슐 충돌체 + 물리 이동 (Jolt Physics 통합)
  - 계단 오르기, 경사면 미끄러짐
  - 점프 + 공중 제어

FSM (Finite State Machine):
  - State: Idle / Walk / Run / Roll / Attack / Hit / Dead
  - 각 State: Enter / Update / Exit + 애니메이션 매핑
  - AttackState: 공격 프레임 단위 히트박스 활성화/비활성화
  - 히트박스 Mesh와 분리된 별도 CollisionShape 배열
```

**M18~M19: 전투 시스템 + 락온 + 카메라**

```
CombatSystem:
  - 데미지 계산: (공격력 × 자세 배율) - (방어력 × 방어율)
  - 패링: 특정 프레임 윈도우에서 Hit 감지 시 패링 이벤트
  - 스태미나 소모/회복 (구르기/공격/패링)

LockOnSystem:
  - 타겟 가능 Entity 탐색 (카메라 원뿔 + 거리)
  - 락온 시 카메라는 플레이어와 타겟 사이를 바라봄
  - 좌우 스틱으로 락온 타겟 전환

CameraSystem:
  - 3인칭 자유 카메라 (스프링암 + 충돌 감지)
  - 락온 카메라 (보간, 화면 중앙 타겟 유지)
```

**M20~M21: 레벨 스트리밍 + LOD + 마무리**

```
Level Streaming:
  - 맵을 청크 단위로 분리
  - 카메라 위치 기반 비동기 로드/언로드 (Job으로 백그라운드 처리)
  - 청크 간 전환 시 버벅임 없이 스무스 스트리밍

LOD (Level of Detail):
  - 거리별 메시 전환 (LOD0~LOD3)
  - 임포스터 (먼 거리 빌보드 대체)

인벤토리 UI:
  - 아이템 슬롯, 장착, 스탯 표시
  - UI 렌더링 (별도 패스, G-Buffer 이후)
```

### 마일스톤
보스 1마리, 맵 1개 구역, 완성된 전투 루프(공격/구르기/패링/스태미나)로 10분 플레이 가능한 데모.

**엔진 약점 목록 문서화** → Phase 4~5 설계에 반드시 반영

### 에셋 전략
- 캐릭터: Meshy AI 생성 + 수작업 리깅
- 맵: Quixel Megascans (UE5 커뮤니티 에셋) + 커스텀 레벨 에디터
- 텍스처: Substance Player 무료 재질

### 예산 (~200만원)
- JetBrains Rider: 월 $24.90 × 6 = ~20만원
- GitHub Pro: 월 $4 × 6 = ~3만원
- AWS 서버: 월 $20 × 6 = ~18만원
- 3D 모델/캐릭터 에셋: ~80만원
- 음악/SFX: ~40만원
- Meshy 구독 (AI 모델링): 월 $30 × 6 = ~27만원
- 기타: ~12만원

---

## 6. Phase 4 — Class & Servant 게임플레이 (M22~M26, 5개월)

### 목표
3D 공간 레이어 시스템 위에서 Class & Servant의 핵심 MOBA 루프 구현. 로컬에서 AI와 대전 가능한 수준.

### 최우선 설계 결정: 3D 공간 레이어

이 결정은 Phase 4 첫 주에 확정해야 한다. 이후 전체 충돌/이동/AI/네트워크/렌더링에 영향을 준다.

```
레이어 분리 설계:
  LayerMask: UNDERGROUND = 0x01, GROUND = 0x02, AERIAL = 0x04

  PhysicsLayer:
    - 각 Entity는 자신의 레이어 마스크를 가짐
    - 충돌 검사: (A.layerMask & B.layerMask) != 0 일 때만 충돌
    - 지하↔지상: 터널 입구/출구 포인트로만 이동 가능
    - 지상↔공중: 비행/점프로 전환

  렌더링:
    - 지하 레이어: 별도 씬 렌더 (투명 지면으로 지상에서 살짝 보임)
    - 공중 레이어: 그림자는 지상에 투영

  네트워크 동기화 (Phase 5에서 구현):
    - 레이어 상태를 패킷에 포함 (2bit)
```

### 월별 세부 계획

**M22~M23: MOBA 기반 구조 + 캐릭터 3종**

```
MOBA 게임 루프:
  GameMode_ClassAndServant:
    - 미니언 스폰 (웨이브 시스템, 30초 간격)
    - 타워 (공격 범위 내 적 자동 공격)
    - 넥서스 (HP, 파괴 시 게임 종료)
    - 골드 시스템 (킬/어시스트/미니언)

MVP 클래스 3종:
  1. 근거리 전사 (GROUND 레이어)
     - Q: 돌진 + 광역 타격
     - W: 방패 막기 (데미지 흡수)
     - E: 회오리 베기 (주변 적 전체)
     - R: 버서커 모드 (공격 속도 상승)

  2. 원거리 마법사 (AERIAL 레이어 진입 가능)
     - Q: 관통 마법탄
     - W: 지면 폭발 (GROUND 레이어 적 피격)
     - E: 비행 (일시적 AERIAL 전환)
     - R: 메테오 소환 (전 레이어 피격)

  3. Servant 조종형 (UNDERGROUND 활용)
     - Q: Servant 파견 (지하 경로로 정찰)
     - W: Servant 자원 수집 명령
     - E: Servant 지원 요청 (근거리 지원 Servant 소환)
     - R: Servant 전체 특공 (다수 Servant 일제 돌격)
```

**M24: Ability System (Data-Driven)**

```
Lua 5.4 바인딩:
  WintersLua.h: sol2 라이브러리로 C++↔Lua 인터페이스

스킬 데이터 정의 (Lua):
  Ability_Q_Warrior = {
    name = "돌진",
    cooldown = 8.0,
    cost = { stamina = 30 },
    range = 400,
    damage = { base = 120, ratio = 1.2 },
    layer_mask = LAYER_GROUND,
    on_cast = function(caster, target_pos)
      -- 돌진 이동 + 충돌 시 데미지
    end
  }

C++ 실행:
  AbilitySystem.Cast(entity, "Ability_Q_Warrior", targetPos)
  → Lua 스크립트 실행 → 이펙트/데미지/이동 C++ 콜백
```

**M25~M26: Servant 서브시스템 + HUD + AI**

```
Servant RTS 서브시스템:
  ServantManager:
    - 자원 수집 Servant: 맵 자원 지점 탐색 → 수집 → 귀환
    - 정찰 Servant: 지하 경로 순찰, 적 감지 시 이벤트 발행
    - 지원 Servant: 주인 위치 추적, 범위 내 적 공격

  NavMesh: 레이어별 별도 NavMesh 생성
    (지하 경로는 지상 NavMesh와 분리, 연결점만 공유)

BehaviorTree (AI):
  Selector → Sequence → Task 구조
  - Servant용 간단한 BT (수집/정찰/지원)
  - 미니언용 기초 BT (전진/공격/도망)

HUD:
  - 체력/마나/스태미나 바
  - 스킬 아이콘 + 쿨다운 오버레이
  - 미니맵 (3D 레이어 구분 아이콘)
  - Servant 상태 패널
  - 골드/킬/타워 상태

카메라:
  - 탑다운 + 줌 (마우스 휠)
  - 화면 끝으로 카메라 이동 (LoL 방식)
  - 클릭 이동 (NavMesh 경로 탐색)
```

### 마일스톤
3개 클래스, 1개 맵, AI 상대로 10분 완성 게임 루프. Servant가 실제로 자원 수집/정찰하는 장면 플레이 가능.

### 예산 (~300만원)
- Rider + GitHub Pro: ~26만원
- AWS (스테이징 서버 추가): 월 $50 × 5 = ~37만원
- 챔피언 3종 3D 모델 (Meshy + 수작업): ~80만원
- 맵 에셋: ~60만원
- UI/아이콘 에셋: ~40만원
- SFX (스킬/히트음): ~30만원
- BGM 라이선스: ~27만원

---

## 7. Phase 5 — 네트워크 + 백엔드 서비스 (M27~M30, 4개월)

### 목표
인터넷으로 실제 두 사람이 매칭 → 대전 → 랭크 갱신되는 전체 루프 완성.

### 네트워크 아키텍처 결정

```
Hybrid Netcode:
  게임 로직 (스킬/전투/아이템): Lockstep
    - 결정론적: 같은 입력 → 같은 결과 (부동소수점 주의)
    - 대역폭 매우 낮음 (입력만 전송)
    - 리플레이/관전 무료

  이동/물리: State Replication + Prediction
    - 클라이언트: 서버 응답 전 로컬에서 먼저 이동 적용
    - 서버: 권위 있는 상태 broadcast
    - 클라이언트: 서버 결과와 차이 발생 시 Rollback 보정

패킷 직렬화: FlatBuffers (제로 카피, C++ 코드 자동 생성)
전송 레이어: UDP + KCP (신뢰성, 재전송, 혼잡 제어)
서버 tick rate: 60Hz (게임 로직), 30Hz (이동 브로드캐스트)
```

### 월별 세부 계획

**M27: 클라이언트 네트워크 레이어**

```
UDP Transport:
  KcpSession     : KCP 라이브러리 래핑
  PacketDispatcher: 패킷 타입 → 핸들러 등록/디스패치
  NetworkManager : 연결 관리, 재연결 처리

FlatBuffers 패킷:
  // PacketDef.fbs
  table C2S_Move {
    seq     : uint;
    pos_x   : float;
    pos_y   : float;
    pos_z   : float;
    layer   : ubyte;
    dir_yaw : float;
  }
  table S2C_State {
    seq       : uint;
    entities  : [EntityState];
  }

클라이언트 예측:
  InputBuffer: 최근 N프레임 입력 기록
  LocalSimulation: 서버 응답 전 로컬 적용
  Reconciliation: 서버 snapshot 도착 → 차이 계산 → 보정
```

**M28: 게임 서버 (Go)**

```
Go 게임 서버 구조:
  GameSession : 1 게임 = 1 고루틴
  Ticker      : 60Hz tick, 입력 수집 → 시뮬레이션 → 브로드캐스트
  UDPServer   : KCP 세션 관리 (1 플레이어 = 1 KCP 세션)

  AntiCheat:
    - 이동속도 검증: 1프레임 이동량이 물리 한계 초과 시 킥
    - 순간이동 감지: 위치 델타 > 임계값 시 경고
    - 스킬 쿨다운 서버사이드 재검증

Agones 통합:
  GameServer CR → 게임 서버 Pod 생성
  Allocated → 게임 시작
  Shutdown  → 게임 종료 → Pod 삭제 후 Pool 반환
```

**M29: 백엔드 마이크로서비스 (Go)**

```
Auth Service (Go + PostgreSQL):
  POST /auth/register  : 계정 생성 (bcrypt 패스워드 해싱)
  POST /auth/login     : JWT 발급 (Access 1h + Refresh 7d)
  POST /auth/refresh   : 토큰 갱신

Matchmaking Service (Go + Redis):
  입력: 플레이어 MMR + 선호 레이어
  알고리즘: Elo 기반 확장 매칭 (대기 시간 증가 시 허용 범위 확장)
  출력: 게임 세션 ID + 게임 서버 IP:Port

Player Profile / Leaderboard (Go + Redis + PostgreSQL):
  Redis Sorted Set으로 실시간 랭킹 업데이트
  PostgreSQL에 영구 기록

Inventory / Shop Service (Go + PostgreSQL):
  아이템 구매: 가상화폐 차감 + 인벤토리 추가 (트랜잭션 단일 처리)
  모든 거래: 불변 트랜잭션 로그 기록

Notification Service (Go + Kafka):
  게임 종료 이벤트 수신 → 랭크 갱신 → 알림 → 분석
```

**M30: 인프라 + 결제 + 모니터링**

```
Kubernetes (EKS on AWS ap-northeast-2):
  - Agones: 게임 서버 Pod 관리
  - HPA: 매칭 트래픽에 따른 자동 스케일링
  - Helm Chart: 서비스별 배포 관리

결제 (PG 추상화 레이어):
  interface PaymentGateway {
      CreateOrder(amount, currency) OrderID
      VerifyReceipt(receiptData)   bool  // 서버사이드 검증
      Refund(orderId)              bool
  }
  TossPaymentsGateway implements PaymentGateway  // 국내
  StripeGateway implements PaymentGateway        // 글로벌

  가상화폐 레이어:
    실제 돈 → [PG] → Coin (가상화폐) → 아이템/스킨

모니터링:
  Prometheus: 메트릭 수집 (게임 서버 CPU/메모리, 패킷 지연, 동접)
  Grafana: 대시보드 시각화
  ELK Stack: 로그 중앙화
  PagerDuty: 장애 알림 (p99 레이턴시 > 100ms 시 알람)
```

### 마일스톤
두 사람이 인터넷으로 매칭 → 3레이어 MOBA 대전 → 게임 종료 → 랭크 갱신 전체 루프 작동.

### 예산 (~500만원)
- Rider + GitHub Pro: ~24만원
- AWS 클라우드 (베타 규모):
  - EC2 c5.xlarge × 2 (게임 서버): 월 $150 × 4 = ~88만원
  - EKS 클러스터: 월 $80 × 4 = ~47만원
  - RDS PostgreSQL db.t3.medium: 월 $50 × 4 = ~29만원
  - ElastiCache Redis cache.t3.micro: 월 $30 × 4 = ~18만원
  - MSK (Kafka): 월 $60 × 4 = ~35만원
  - CloudFront CDN + S3 (에셋): 월 $30 × 4 = ~18만원
  - 소계: ~235만원
- Toss Payments 등록 (무료, 수수료 3.3%): 0
- PagerDuty (알람): 월 $20 × 4 = ~11만원
- 기타: ~230만원 (예비)

---

## 8. Phase 6 — Steam 출시 (M31~M32, 2개월)

### 목표
Steam 정식 출시 + 첫 1,000 동접 + 지속 수익 구조 확립.

### Steam 출시 체크리스트

**M31: 출시 준비**

```
Steam 설정:
  [ ] Steam Direct 등록 ($100 USD)
  [ ] 스팀 페이지: 게임 설명, 스크린샷 10장, 트레일러 1개
  [ ] Steam SDK 통합: 도전과제, 리더보드, 친구 초대
  [ ] 나이 등급 심의 (게관위, ESRB)
  [ ] PEGI 등급 (유럽 출시 시)
  [ ] Steam 지역 가격 설정 (한국 ₩ 포함)

클라이언트 런처:
  WintersLauncher.exe:
    - 패치 서버에서 버전 확인 → 업데이트 파일 다운로드 → 적용
    - Steam 인증 연동 (Steamworks SDK)
    - 게임 서버 IP 목록 갱신

얼리 액세스 베타 (100~500명):
  - 디스코드 서버 개설 + 베타 테스터 모집
  - 버그 리포트 양식 제공
  - 매일 빌드 + 패치노트 배포
```

**M32: 정식 출시 + 운영**

```
수익화 구조:
  기본 요금제: 무료 플레이 (F2P)
  배틀패스: $9.99/시즌 (90일)
    - 무료 트랙 (기본 보상)
    - 프리미엄 트랙 (스킨, 이펙트, 칭호)
  개별 스킨: Coin 2,000~8,000 (약 $2~$8)
  Coin 충전: 1,000코인=$1 / 5,500코인=$5 / 12,000코인=$10

마케팅 채널:
  - YouTube 데브로그 (개발 과정 시리즈, 출시 전 공개)
  - Reddit r/indiegaming, r/gamedev, r/MOBA 커뮤니티
  - Twitter/X 개발 일지 (알고리즘 노출)
  - 인디게임 Discord 서버 교차 홍보
  - 인플루언서 1~2명 (중소형, 실제 플레이 콘텐츠)

운영 원칙:
  - Steam 리뷰 72시간 내 전수 응답
  - 디스코드 버그 리포트 48시간 내 확인
  - 2주 패치 사이클 (긴급 버그픽스는 즉시)
  - 시즌제: 3개월마다 신규 챔피언 1~2종 + 맵 업데이트
```

### 마일스톤
Steam 정식 출시 + 첫 1주일 동접 1,000 돌파 + 배틀패스 첫 판매.

### 예산 (~200만원)
- Steam Direct: $100 = ~13만원
- AWS (출시 스케일아웃): 월 $400 × 2 = ~106만원
- 마케팅 (SNS 광고): ~50만원
- 인플루언서 협업: ~31만원

---

## 9. 예산 전체 명세

### 순수 개발비 요약

| Phase | 기간 | 예산 |
|---|---|---|
| Phase 0 — 준비 | 1개월 | ~10만원 |
| Phase 1 — 엔진 코어 | 8개월 | ~80만원 |
| Phase 2 — 렌더 엔진 | 6개월 | ~150만원 |
| Phase 3 — 검증 게임 | 6개월 | ~200만원 |
| Phase 4 — 게임플레이 | 5개월 | ~300만원 |
| Phase 5 — 네트워크/백엔드 | 4개월 | ~500만원 |
| Phase 6 — 출시 | 2개월 | ~200만원 |
| **예비비 (10%)** | - | **~144만원** |
| **합계 (순수 개발비)** | **32개월** | **~1,584만원** |

### 항목별 상세 예산

**소프트웨어 & 도구**

| 항목 | 단가 | 기간 | 금액 |
|---|---|---|---|
| FMOD Studio (인디) | 무료 | - | 무료 |
| Visual Studio Community | 무료 | - | 무료 |
| RenderDoc / PIX / NSight | 무료 | - | 무료 |
| Jolt Physics (MIT) | 무료 | - | 무료 |
| JetBrains Rider (선택) | $24.90/월 | 32개월 | ~107만원 |
| GitHub Pro | $4/월 | 32개월 | ~17만원 |
| Meshy AI (3D 모델링) | $30/월 | 12개월 | ~46만원 |
| PagerDuty | $20/월 | 4개월 | ~11만원 |
| **소계** | | | **~181만원** |

**클라우드 인프라 (AWS ap-northeast-2)**

| 기간 | 구성 | 월 비용 | 금액 |
|---|---|---|---|
| M1~M21 (21개월) | EC2 t3.micro × 2, 개발 서버 | $20 | ~56만원 |
| M22~M28 (7개월) | EC2 c5.xlarge × 2, RDS, Redis, EKS | $370 | ~349만원 |
| M29~M32 (4개월) | 출시 스케일 (c5.xlarge × 4 + 전체) | $600 | ~323만원 |
| **소계** | | | **~728만원** |

**에셋 & 아트**

| 항목 | 금액 |
|---|---|
| 챔피언 3D 모델 3종 (Meshy + 수작업) | ~150만원 |
| 맵/환경 에셋 | ~80만원 |
| UI/아이콘/텍스처 | ~70만원 |
| 음악 라이선스 (BGM 5트랙) | ~80만원 |
| 사운드 FX (스킬/UI/환경) | ~40만원 |
| **소계** | **~420만원** |

**플랫폼 & 마케팅**

| 항목 | 금액 |
|---|---|
| Steam Direct 등록비 | ~13만원 |
| SNS 광고 (출시 전후) | ~100만원 |
| 인플루언서 협업 1~2인 | ~80만원 |
| **소계** | **~193만원** |

### 생활비 고려 (솔로 개발자)

| 항목 | 금액 |
|---|---|
| 월 생활비 (서울 기준, 최소) | 150만원/월 |
| 32개월 생활비 | **~4,800만원** |
| **총 필요 자금 (생활비 포함)** | **~6,400만원** |

### 절감 전략
- Phase 1~2 (14개월): 아르바이트/프리랜서 병행으로 생활비 일부 자체 충당
- Phase 3~4: 엔진 초기 버전을 다른 개발자에게 라이선스 가능성 검토
- Phase 5~6: 클라우드 비용은 수익으로 충당 가능 구간

---

## 10. 리스크 관리 & 비상 계획

### 기술 리스크

| 리스크 | 확률 | 영향 | 대응 |
|---|---|---|---|
| DX11 렌더 엔진 완성 지연 | 높음 | 높음 | 모든 Phase에 20% 버퍼 일정 포함 |
| FlatBuffers 결정론적 시뮬레이션 불일치 | 중간 | 매우 높음 | 부동소수점 고정 모드 (fixedpoint.h) 대비 |
| Agones 게임 서버 스케일링 장애 | 낮음 | 높음 | 수동 폴백 서버 풀 상시 유지 |
| 결제 서비스 PG 장애 | 낮음 | 높음 | Toss/Stripe 이중화, 큐 기반 재처리 |

### 일정 리스크

| 리스크 | 대응 |
|---|---|
| Phase 1 엔진 기반이 8개월 초과 | MVP 범위 축소: ECS 단순화, EventBus 나중으로 |
| 검증 게임(엘든링 모작)이 길어짐 | 3개월로 중단, 충분히 검증된 것만 반영 |
| 52스킬 구현이 Phase 4에서 병목 | MVP 3스킬 × 3클래스로 출시, 나머지 패치 |

### 비상 계획: 범위 축소 버전

완전한 Winters Engine 대신 다음 범위로 축소 가능:
- 클래스 3종 (추후 DLC 추가)
- 레이어 2개 (지상 + 공중, 지하는 1.0 이후)
- Servant 1종 (정찰형만)
- 백엔드 단순화 (Agones 없이 고정 서버 풀)

이 경우 개발 기간은 24개월, 비용은 ~1,100만원으로 단축 가능.

---

## 11. 핵심 기술 결정 사항 요약

| 결정 | 선택 | 이유 |
|---|---|---|
| 그래픽 API | DX11 → RHI → DX12 | LoL/Valorant와 동일 경로, 단계적 전환 |
| 네트워크 방식 | Hybrid (Lockstep + State Rep) | 로직 결정론성 + 이동 반응성 동시 확보 |
| 직렬화 | FlatBuffers | 제로 카피, C++ 자동 생성 |
| 물리 엔진 | Jolt Physics | MIT 라이선스, 멀티스레드 설계 |
| 스크립팅 | Lua 5.4 (sol2 바인딩) | 스킬 밸런스 패치 엔진 재빌드 없이 가능 |
| 백엔드 언어 | Go | 고성능 마이크로서비스, 동시성 강점 |
| 결제 관계형 DB | PostgreSQL | ACID 보장, 돈은 반드시 PostgreSQL |
| 세션/랭킹 캐시 | Redis | Sorted Set 랭킹, 세션 토큰 |
| 서비스 통신 | Apache Kafka | 이벤트 드리븐, 서비스 결합도 제거 |
| 컨테이너 오케스트레이션 | Kubernetes + Agones | 게임 서버 전용 K8s 확장 |
| CDN | Cloudflare | 글로벌 에셋 배포, 한국→글로벌 |
| 결제 PG | Toss(국내) + Stripe(글로벌) | PG 추상화 레이어 위에 구현 |
| 3D 레이어 시스템 | Phase 4 첫 주에 확정 필수 | 이후 전체 시스템에 영향 |

---

*Winters Game Studio — Class & Servant 개발 로드맵 v1.0*
*작성일: 2026년*
