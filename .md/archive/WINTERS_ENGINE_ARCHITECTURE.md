# Winters Engine — 아키텍처 방향성 수립 (v1 — ARCHIVED)

> **이 문서는 `WINTERS_ENGINE_ARCHITECTURE_FINAL.md`로 대체되었습니다.**
> 최신 아키텍처: `.md/WINTERS_ENGINE_ARCHITECTURE_FINAL.md`
>
> 아래는 초기 의사결정 기록으로 보존합니다.

> 목표: 리그오브레전드 / 배틀그라운드 스타일 실시간 대전 게임을 서비스 레벨로 개발·배포하고 수익을 내는 독자 게임 엔진

---

## 0. 의사결정 핵심: DirectX 9 / 11 / 12 선택

### 결론: **DirectX 11 → RHI 추상화 → 향후 DX12 확장**

| | DX9 | DX11 | DX12 |
|---|---|---|---|
| 출시 | 2002 | 2009 | 2015 |
| Compute Shader | ❌ | ✅ | ✅ |
| 멀티스레드 렌더링 | ❌ | ✅(제한적) | ✅(완전) |
| GPU Driven | ❌ | ✅(Compute) | ✅(최적) |
| 학습 비용 | 낮음 | 중간 | 매우 높음 |
| 드라이버 안정성 | 최고 | 높음 | 중간 |
| 개발 속도 | 느림(구식) | 빠름 | 느림(직접 관리) |
| 실제 사용 게임 | 구형 게임 | **LoL, Valorant, Dota2** | 사이버펑크, 포르자 |

**DX11을 선택하는 이유:**
- 경쟁 게임 장르에서 그래픽 > 게임플레이가 아니다. LoL, Valorant 모두 DX11
- DX12는 강력하지만 동기화·메모리·파이프라인 장벽을 전부 직접 관리해야 함
- Compute Shader로 GPU Driven Rendering 구현 가능 (DX11로도 충분)
- **RHI 추상화 레이어**를 처음부터 두면 DX12 백엔드를 나중에 추가할 수 있음
- Riot이 DX9 → DX11 → DX12 단계를 밟은 것과 동일한 전략

**RHI(Rendering Hardware Interface)란:**
그래픽 API 호출을 직접 하지 않고, RHI 인터페이스를 통해 호출한다.
`RHI::CreateTexture()` 내부에서 DX11이면 `D3D11Device::CreateTexture2D()`,
DX12면 `D3D12Device::CreateCommittedResource()`를 호출하는 구조.
Unreal Engine이 이 방식으로 DX11/12/Vulkan/Metal을 동시 지원한다.

---

## 1. 전체 엔진 레이어 구조

```
┌─────────────────────────────────────────────────────┐
│                     GAME LAYER                       │
│  Champions / Maps / Items / UI / Game Modes          │
├─────────────────────────────────────────────────────┤
│               GAMEPLAY FRAMEWORK                     │
│  Ability System │ Combat │ Camera │ AI (BT/Nav)      │
├─────────────────────────────────────────────────────┤
│                  WORLD / ECS                         │
│  Entity │ Component Store (SoA) │ System Scheduler   │
├──────────────────┬──────────────────────────────────┤
│  RENDER ENGINE   │         NETWORK (CLIENT)          │
│  Render Graph    │  UDP Transport │ Prediction        │
│  Deferred Pipe   │  Reconciliation │ Interpolation   │
│  G-Buffer        │  Anti-cheat Hooks                 │
├──────────────────┴──────────────────────────────────┤
│              RENDERING HARDWARE INTERFACE            │
│           DX11 Backend │ (DX12 Backend - 향후)       │
├─────────────────────────────────────────────────────┤
│                   CORE FOUNDATION                    │
│  Memory Allocators │ Job System │ Math │ Event Bus   │
├─────────────────────────────────────────────────────┤
│                   PLATFORM LAYER                     │
│       OS 추상화 │ File I/O │ Thread │ Timer          │
└─────────────────────────────────────────────────────┘
```

---

## 2. 레이어별 설계 방향

### Layer 0 — Platform (플랫폼 추상화)
**왜 필요한가:** Windows 전용 코드가 엔진 전반에 박히면 콘솔/Mac 이식이 불가능
- OS 추상화: `WintersWindow`, `WintersThread`, `WintersFile`
- 현재는 Windows만 지원해도, 인터페이스를 추상화하면 된다
- `WCHAR`, `HWND` 같은 Win32 타입이 Core 레이어 위로 올라오면 안 됨

### Layer 1 — Core Foundation (핵심 기반)

**① 커스텀 메모리 할당자**
- `new/delete`를 엔진 전반에서 직접 쓰면 메모리 단편화, 프레임 단위 할당 추적 불가
- **Pool Allocator**: 고정 크기 오브젝트 (Component, Packet 등)
- **Frame Allocator**: 프레임 끝에 일괄 해제 (임시 계산 데이터)
- **Stack Allocator**: LIFO 패턴의 로딩/언로딩

**② Job System (Fiber 기반)**
- 현재 구조의 `Update → LateUpdate` 순서를 **의존성 그래프**로 교체
- Fiber: 스레드보다 가벼운 실행 단위. 중간에 다른 Job으로 전환 가능
- 너티독이 GDC에서 공개한 "Parallelizing the Naughty Dog Engine" 구조
- 목표: 12코어 CPU라면 12개 코어가 동시에 게임 로직을 처리

```
Frame Start
│
├── [Input Job] ──────────────────────────────► [Player Input Job]
│                                                      │
├── [AI Decision Job] ─────────────────────────► [AI Move Job]
│                                                      │
└── [Physics Step Job] ──────────────────────► [Collision Job] ──► [Animation Job]
                                                                          │
                                                                    [Render Submit Job]
```
의존성이 없는 Job은 동시에 실행 → 멀티코어 완전 활용

**③ SIMD Math Library**
- `Vector3`, `Matrix4x4` 연산이 매 프레임 수백만 번 실행됨
- SSE/AVX 인트린직으로 SIMD 처리 (4개 float을 동시 연산)
- `DirectXMath` 라이브러리를 래핑하거나 직접 구현

**④ Event Bus**
- "A가 B를 직접 참조한다" 대신 "A가 이벤트를 발행, B가 구독"
- 시스템 간 결합도를 낮추는 핵심 메커니즘
- `PlayerDied`, `ItemPickedUp`, `MatchEnded` 같은 이벤트 중심 설계

### Layer 2 — RHI (렌더링 하드웨어 인터페이스)

**핵심 개념:** 그래픽 API에 직접 의존하지 않는다
- `RHITexture`, `RHIBuffer`, `RHIPipeline`, `RHICommandList`
- DX11 구현체: `D3D11Texture implements RHITexture`
- 엔진 코드 어디서도 `ID3D11Device*`가 보이면 안 됨 (RHI 레이어 아래에만 존재)

**Command List 패턴:**
- 게임 로직 스레드는 렌더 명령을 `CommandList`에 기록만 한다
- Render Thread가 별도로 CommandList를 GPU에 제출
- CPU(게임 로직) ↔ GPU(렌더링)이 서로 기다리지 않고 병렬 실행

### Layer 3 — Render Engine (렌더 엔진)

**① Render Graph**
- 렌더 패스(Pass)들의 의존성을 그래프로 선언
- 예: `ShadowMap Pass → GBuffer Pass → Lighting Pass → PostProcess Pass → UI Pass`
- 자동 리소스 배리어(Resource Barrier) 관리, 불필요한 Pass 자동 제거
- Frostbite(배틀필드), Unreal(RDG), Lumen이 모두 이 방식

**② Deferred Rendering Pipeline**
- 경쟁 게임에 수십~수백 개 광원이 동적으로 생성됨 (스킬 이펙트, 폭발 등)
- **Forward**: 각 오브젝트마다 모든 광원 계산 → 오브젝트 × 광원 = O(n²)
- **Deferred**: G-Buffer에 기하 정보를 한 번에 모으고, 화면 픽셀 단위로 조명 계산 → O(pixels)

```
G-Buffer 구성:
  RT0: Albedo (RGB) + Roughness (A)
  RT1: World Normal (RGB) + Metallic (A)
  RT2: Motion Vector (RG) + AO (B) + Emissive Mask (A)
  Depth Buffer
```

**③ Clustered Deferred Shading**
- 화면을 3D 클러스터로 나누고, 각 클러스터에 영향을 주는 광원 목록을 미리 계산
- 수백 개 광원도 비용 없이 처리 가능
- LoL Wild Rift, Valorant가 이 방식 사용

**④ Temporal Anti-Aliasing (TAA)**
- MSAA보다 비용이 낮으면서 품질이 높음
- 이전 프레임과 현재 프레임을 블렌딩하여 계단 현상 제거

### Layer 4 — ECS (Entity Component System)

**현재 구조와의 차이:**
```
현재 (OOP):   CMonster : CGameObject { CTransform* / CCollider* / ... }
목표 (ECS):   Entity = uint64_t  /  Component = 순수 데이터 구조체  /  System = 로직
```

**데이터 레이아웃 (SoA - Structure of Arrays):**
```
PositionStore:  [pos0, pos1, pos2, pos3, ...]  ← 연속 메모리
VelocityStore:  [vel0, vel1, vel2, vel3, ...]  ← 연속 메모리
HealthStore:    [hp0,  hp1,  hp2,  hp3,  ...]  ← 연속 메모리
```
MovementSystem이 실행될 때 Position + Velocity 데이터만 캐시에 올라옴.
500개 몬스터를 처리해도 캐시 미스가 거의 없음.

**Archetype 기반 ECS:**
- 같은 Component 조합을 가진 Entity들을 동일한 Archetype에 저장
- Archetype 내 데이터는 100% 연속 메모리
- Unity DOTS, Bevy, EnTT 모두 이 방식

### Layer 5 — Network (클라이언트 네트워크)

**경쟁 게임 네트워크의 3대 원칙:**
1. **서버 권위(Server Authoritative)**: 게임 상태는 서버에서만 결정. 클라이언트는 예측만
2. **클라이언트 예측(Client-Side Prediction)**: 서버 응답 전에 로컬에서 먼저 실행해 반응성 확보
3. **조정(Reconciliation)**: 서버 결과가 오면 예측과 비교해 보정

**LoL 스타일 vs PUBG 스타일 선택:**

| | Lockstep (LoL 방식) | State Replication (PUBG 방식) |
|---|---|---|
| 원리 | 모든 클라이언트가 동일 입력 → 동일 시뮬레이션 | 서버가 시뮬레이션, 상태를 클라이언트에 배포 |
| 장점 | 대역폭 매우 낮음, 리플레이 무료 | 물리/이동 표현이 정밀함 |
| 단점 | 한 명이 렉 → 전체 지연 | 대역폭 높음, 치팅 방어 복잡 |
| 적합 | MOBA, RTS, 턴제 | FPS, 배틀로얄 |

**Winters Engine 방향: Hybrid**
- 게임 로직(스킬, 전투, 아이템): **Lockstep** (결정론적, 리플레이/관전 무료)
- 이동/물리: **State Replication + Prediction** (반응성 확보)

**Rollback Netcode:**
- 격투 게임에서 완성된 기술 (GGPO)
- 현재 입력으로 미래를 예측 → 서버 결과가 오면 과거로 돌아가 재시뮬레이션
- 지연 없이 반응적인 게임플레이 가능

---

## 3. 백엔드 서비스 아키텍처 (수십억 명 스케일)

### 핵심 원칙: 마이크로서비스 + 이벤트 드리븐

```
클라이언트
    │
    ├─── API Gateway (인증, 라우팅, Rate Limiting)
    │
    ├─── Auth Service          (로그인, 토큰 발급, OAuth)
    ├─── Player Profile Service (프로필, 통계, 랭크)
    ├─── Matchmaking Service    (대기열, 스킬 기반 매칭)
    ├─── Game Server Manager    (서버 스핀업/다운, 세션 할당)
    ├─── Shop Service           (아이템 카탈로그, 가격 정책)
    ├─── Payment Service        (결제 처리, 가상화폐 충전)
    ├─── Inventory Service      (보유 아이템, 장착 정보)
    ├─── Leaderboard Service    (랭킹, Redis Sorted Set)
    ├─── Notification Service   (인게임 알림, 푸시)
    ├─── Analytics Service      (이벤트 스트리밍, BI)
    └─── Anti-Cheat Service     (서버사이드 검증)
```

**각 서비스가 독립적으로 스케일링:**
- 결제 서비스 트래픽이 폭증하면 결제 서비스만 인스턴스를 늘린다
- 게임 서버가 모자라면 게임 서버 풀만 확장한다
- 하나의 서비스가 장애가 나도 전체가 멈추지 않는다

### 데이터베이스 전략

```
PostgreSQL (관계형):
  - 사용자 계정, 결제 내역, 트랜잭션
  - ACID 보장이 필요한 모든 것 (돈 = PostgreSQL)

Redis (캐시 + 실시간):
  - 세션 토큰, 로그인 상태
  - 랭킹 (Sorted Set으로 실시간 업데이트)
  - 매칭 대기열
  - 인게임 상태 캐시

MongoDB (문서형):
  - 플레이어 통계, 게임 히스토리
  - 스킬/아이템 데이터 (복잡한 중첩 구조)
  - 설정 데이터

Kafka (이벤트 스트리밍):
  - 서비스 간 이벤트 전달 (게임종료 → 랭크갱신 → 알림 → 분석)
  - 이벤트 소싱으로 서비스 결합도 제거
```

### 게임 서버 관리

```
Game Server Manager
    │
    ├─── Server Pool (예열된 게임 서버 인스턴스)
    │       ├── Warm servers (즉시 할당 가능)
    │       └── Cold servers (30초 내 시작 가능)
    │
    ├─── 매칭 완료 → 서버 할당 → 클라이언트에 IP/Port 전달
    │
    └─── 게임 종료 → 결과 전송 → 서버 풀 반환
```

Kubernetes: 서버 컨테이너 오케스트레이션
Agones (Google): 게임 서버 전용 K8s 확장 (PUBG, Ubisoft 사용)

### 결제 시스템

**가상화폐 레이어 (RP 방식):**
```
실제 돈 → [결제 게이트웨이] → 가상화폐 (Coin)
가상화폐 → [Shop Service] → 게임 아이템/스킨
```

가상화폐 레이어의 이점:
- 결제 규제/환율에서 아이템 가격이 독립적
- 사용자가 "얼마짜리"인지 직관적으로 인지하기 어려움 (구매 심리)
- 잔여 가상화폐가 환불 청구를 복잡하게 만드는 법적 장벽

**결제 처리 원칙:**
- PG사 추상화 레이어 (Stripe, Toss, PayPal을 동일 인터페이스로)
- 모든 거래는 **불변 트랜잭션 로그** (한 번 쓰면 수정 불가)
- 영수증 검증은 반드시 서버사이드 (클라이언트 영수증은 위조 가능)
- 환불 처리, 이중 결제 방지, 사기 감지는 별도 서비스

---

## 4. 인프라 아키텍처 (글로벌 스케일)

```
[Seoul Region]──┐
[Tokyo Region]──┤──► Global Load Balancer (GeoDNS)──► API Gateway
[US Region]─────┤
[EU Region]─────┘

각 Region:
  ├── API Servers (Auto Scaling Group)
  ├── Game Servers (Kubernetes + Agones)
  ├── Database Cluster (Primary + Read Replicas)
  ├── Redis Cluster
  └── Kafka Cluster

CDN (Cloudflare):
  ├── 게임 클라이언트 패치 파일
  ├── 이미지/에셋 파일
  └── 정적 리소스
```

**수십억 명이 동시 접속해도 무너지지 않는 원칙:**
1. **수평 확장(Horizontal Scaling)**: 서버를 더 강하게가 아닌 더 많이
2. **무상태(Stateless) API**: 어느 서버에 요청이 가도 동일한 결과
3. **캐시 우선**: DB에 직접 가기 전에 Redis 캐시 먼저
4. **Circuit Breaker**: 하나의 서비스 장애가 연쇄 장애로 번지지 않도록
5. **이벤트 드리븐**: 서비스 간 직접 호출 최소화, Kafka를 통한 비동기 통신

---

## 5. 프로젝트 구조 (새 프로젝트)

```
WintersEngine/
│
├── Engine/                    ← 엔진 코어 (DLL)
│   ├── Platform/              ← OS 추상화
│   ├── Core/                  ← Memory, JobSystem, Math, EventBus
│   ├── RHI/                   ← Rendering Hardware Interface
│   │   ├── RHI_Interface/     ← 추상 인터페이스
│   │   └── DX11/              ← DX11 구현체
│   ├── Renderer/              ← RenderGraph, Pipeline, Materials
│   ├── ECS/                   ← Entity, ComponentStore, SystemScheduler
│   ├── Physics/               ← Jolt Physics 래핑
│   ├── Audio/                 ← FMOD 래핑
│   ├── Network/               ← 클라이언트 네트워크 레이어
│   ├── Input/                 ← Raw Input, Action Mapping
│   └── Asset/                 ← 에셋 로딩, 스트리밍, 패킹
│
├── GameServer/                ← 게임 서버 (독립 프로세스)
│   ├── SimCore/               ← 결정론적 게임 시뮬레이션
│   ├── Network/               ← UDP 서버, 세션 관리
│   └── AntiCheat/             ← 서버사이드 검증
│
├── Services/                  ← 백엔드 마이크로서비스 (Go)
│   ├── Auth/
│   ├── Matchmaking/
│   ├── PlayerProfile/
│   ├── Shop/
│   ├── Payment/
│   ├── Inventory/
│   ├── Leaderboard/
│   └── Analytics/
│
├── Infrastructure/
│   ├── Kubernetes/            ← K8s 배포 명세
│   ├── Terraform/             ← 클라우드 인프라 코드
│   └── CI-CD/                 ← GitHub Actions 파이프라인
│
├── Tools/
│   ├── WorldEditor/           ← 맵 에디터
│   ├── AssetProcessor/        ← 에셋 빌드 파이프라인
│   └── ProfilerViewer/        ← 엔진 프로파일러 뷰어
│
└── Game/                      ← 실제 게임 콘텐츠
    ├── Champions/             ← 챔피언 데이터/코드
    ├── Maps/
    ├── Items/
    └── UI/
```

---

## 6. 기술 스택

| 영역 | 기술 | 이유 |
|------|------|------|
| 엔진 언어 | C++20 | 성능, 기존 지식 활용 |
| 그래픽 API | DX11 → RHI → DX12 | 단계적 전환 |
| 물리 | Jolt Physics | MIT 라이선스, 멀티스레드 설계 |
| 오디오 | FMOD Studio | 업계 표준 |
| 스크립팅 | Lua 5.4 | 챔피언/스킬 데이터 정의 |
| 백엔드 언어 | Go | 고성능 마이크로서비스 |
| 관계형 DB | PostgreSQL | 결제/계정 ACID 보장 |
| 캐시/실시간 | Redis | 세션, 랭킹, 대기열 |
| 문서 DB | MongoDB | 플레이어 통계, 게임 히스토리 |
| 메시지 브로커 | Apache Kafka | 서비스 간 이벤트 스트리밍 |
| 컨테이너 | Docker + Kubernetes | 수평 확장 |
| 게임서버 오케스트레이션 | Agones | 게임 서버 전용 K8s |
| CDN | Cloudflare | 글로벌 에셋 배포 |
| 결제 | Toss Payments (국내) + Stripe (글로벌) | PG 추상화 레이어 위에 |
| 모니터링 | Prometheus + Grafana | 실시간 서버 지표 |
| 로깅 | ELK Stack | 중앙화 로그 분석 |

---

## 7. 개발 단계 로드맵

### Phase 1 — 엔진 기반 (3~4개월)
- Platform 레이어, Core 시스템 (Memory, JobSystem, Math, EventBus)
- RHI 인터페이스 + DX11 구현체
- 기본 ECS (Entity, ComponentStore, System)
- 간단한 렌더 루프 (DX11로 도형 출력)
- **이 단계의 목표:** 현재 DX9 엔진의 기능을 새 구조로 재현

### Phase 2 — 렌더 엔진 (3~4개월)
- Render Graph 구현
- Deferred Rendering Pipeline (G-Buffer, Lighting Pass)
- Skeletal Animation + Blend Tree
- Shadow Map (Cascaded)
- TAA, SSAO, Bloom 포스트 프로세싱
- **이 단계의 목표:** 눈으로 보기에 LoL 수준의 시각 품질

### Phase 3 — 게임플레이 시스템 (3~4개월)
- 캐릭터 이동 (내비게이션 메시 + 물리)
- Ability System (Data-Driven: Lua 정의)
- 전투 시스템 (히트박스, 데미지 계산)
- 카메라 시스템 (탑뷰/3인칭)
- **이 단계의 목표:** 인게임 프로토타입 플레이 가능

### Phase 4 — 네트워크 (3~4개월)
- UDP 전송 레이어 + 신뢰성 레이어
- 서버 권위 시뮬레이션
- 클라이언트 예측 + 보정
- 매칭 서버 기초
- **이 단계의 목표:** 두 명이 실제로 대전 가능

### Phase 5 — 백엔드 서비스 (4~6개월)
- Auth + Player Profile
- Matchmaking + 게임 서버 관리
- Shop + 가상화폐 + Payment
- Kubernetes 배포 환경
- **이 단계의 목표:** 실제 서비스 배포 준비

### Phase 6 — 게임 콘텐츠 + 서비스 출시
- 챔피언, 맵, 아이템 콘텐츠
- 클라이언트 론처 + 자동 패치
- Beta 출시 → 지표 기반 개선
- 수익화 (배틀패스, 스킨 판매)

---

## 7-1. 프로젝트 폴더 구조 + 배포 전략 (실질적 계획)

### Bin / Include / Header / Code 구조 — 그대로 가도 된다

이 구조는 **미들웨어 배포 방식**의 표준이다.
FMOD, PhysX가 딱 이 방식으로 배포된다.

```
WintersEngine/
├── Engine/
│   ├── Code/       ← 엔진 내부 구현 (.cpp) — 절대 외부 공개 안 함
│   ├── Header/     ← 엔진 내부 헤더 (.h)  — 절대 외부 공개 안 함
│   ├── Include/    ← 클라이언트에 공개하는 헤더만 (.h) — 공개 API
│   └── Bin/        ← 컴파일 결과물 (WintersEngine.dll / .lib / .pdb)
│
├── Client/
│   ├── Code/       ← 게임 로직 구현 (.cpp)
│   ├── Header/     ← 게임 로직 헤더 (.h)
│   ├── Include/    ← Engine/Include를 참조 (별도 복사 or 경로 연결)
│   └── Bin/        ← 최종 실행파일 (Game.exe + WintersEngine.dll)
│
├── Server/
│   ├── Code/
│   ├── Header/
│   └── Bin/        ← GameServer.exe
│
└── Shared/
    └── PacketDef.h ← Client + Server 공용 패킷 정의
```

**배포 시 넘기는 것:**
```
배포 패키지:
  ├── Engine/Bin/WintersEngine.dll    ← 실행에 필요
  ├── Engine/Bin/WintersEngine.lib    ← 링킹에 필요
  ├── Engine/Include/*.h              ← 사용법만 공개
  └── 사용 예제 프로젝트

절대 공개 안 함:
  ├── Engine/Code/**                  ← 엔진 핵심 구현
  └── Engine/Header/**                ← 내부 구현 헤더
```

**Include vs Header 분리 기준:**
- `Include/`: 클라이언트 코드가 `#include` 해야 하는 것만
  - `WintersEngine.h` (통합 인클루드)
  - `IRenderer.h`, `IJobSystem.h` 같은 공개 인터페이스
- `Header/`: 엔진 내부에서만 쓰는 것
  - `D3D11DeviceImpl.h`, `JobFiber.h` 등 구현 세부사항

### 시각적 의존 흐름
```
Client/Code/*.cpp
    │
    ├── #include "Client/Header/*.h"
    └── #include "Engine/Include/*.h"  ← 엔진 공개 헤더만
                          │
                          ▼
              Engine/Bin/WintersEngine.dll  ← 링크만 함, 소스는 모름
```

---

## 7-2. 새 폴더 시작 vs 기존 코드 수정 — 결론

### **새 폴더(Winters)에서 싹 다 시작한다**

이유:
1. **DX9 → DX11**: 디바이스 생성, 리소스 관리, 렌더 루프가 근본적으로 다름
   기존 코드를 고치면 DX9 코드와 DX11 코드가 뒤섞여 더 복잡해짐
2. **ECS로 전환**: `CGameObject` 계층 구조를 ECS로 바꾸려면
   사실상 엔진 전체를 다시 쓰는 것과 같음. 중간 상태가 오히려 혼란
3. **RHI 레이어**: 처음부터 추상화 레이어를 설계해야 함
   기존 코드 위에 얹으면 기술 부채만 쌓임
4. **학습 목적**: 처음부터 새로 짜면서 각 결정이 왜 그렇게 되는지를 체화해야 함

**기존 코드에서 가져오는 것:**
- Packet 구조 설계 철학 (PacketDef.h 패턴)
- Sound System 래핑 방식 (CSoundMgr 구조)
- 수학 유틸리티 함수들 (CPipeline)
- Input 처리 방식 (CDInputMgr)
- 파일 로딩 패턴

**기존 마인크래프트 프로젝트는?**
- 수업 과제로 완성까지 마무리한다
- 완성 후 Winters Engine으로 동일한 게임을 **다시** 만들어보는 게 이상적인 학습
- "같은 기능을 두 번 짜봐야 두 번째가 왜 좋은지 이해한다"

---

## 7-3. 실질적 학습 + 개발 로드맵

```
[현재] 마인크래프트 던전스 수업 완료
         ↓
         ▼ 현재 엔진 구조를 100% 이해하고 완성

[Step 1] Winters Engine 기반 구축 (Phase 1~2, 6~8개월)
         DX11 RHI + Core Systems + ECS + 기본 렌더 파이프라인
         목표: Winters로 마인크래프트 던전스를 다시 만들 수 있는 수준

         ↓ 구조를 몸으로 익힘

[Step 2] 엘든링 모작 (Phase 2~3, 4~6개월)
         Winters Engine 위에서 개발
         - 히트박스 정밀도 (프롬소프트 스타일 FSM)
         - 복잡한 캐릭터 애니메이션 (Blend Tree)
         - 오픈 월드 스트리밍 (레벨 스트리밍)
         - 락온 시스템, 구르기, 파리 시스템
         목표: 엔진이 "더 좋은 게임을 위해 어떻게 바뀌어야 하는가"를 체감

         ↓ 엔진 약점 파악 완료

[Step 3] 실시간 대전 게임 (Phase 3~5, 12~18개월)
         Winters Engine + 백엔드 서비스
         LoL / PUBG 스타일 게임 개발
         네트워크, 매칭, 결제, 서비스 배포까지
         목표: 실제 서비스, 수익화
```

**엘든링 모작이 중요한 이유:**
- LoL 같은 네트워크 게임보다 싱글 게임이 엔진 구조를 검증하기 쉽다
- 복잡한 전투 시스템 → FSM / Animation System / Physics가 검증됨
- 광활한 맵 → Asset Streaming / LOD / Occlusion이 검증됨
- 모작이기 때문에 디자인 결정에 에너지를 안 써도 됨, 기술에만 집중 가능

---

## 8. 현재 엔진(수업용)과 Winters Engine의 관계

현재 DX9 엔진에서 배운 것들이 Winters Engine의 직접 기반이다:

| 현재 | Winters Engine 대응 |
|------|------|
| CMainApp 루프 | Job System의 Frame Root Job |
| Update/LateUpdate | System 의존성 그래프 |
| CRenderer 등록 방식 | RenderGraph Command 제출 |
| CManagement/CScene | World + Scene 개념 유지 (ECS로 내부 재구성) |
| CComponent (Clone 패턴) | ECS Component Store (Archetype 기반) |
| CNetworkMgr (UDP 소켓) | Network Transport Layer의 직접 전신 |
| CMonsterMgr (Manager → System) | ECS System의 직접 전신 |
| PacketDef | Binary Protocol의 직접 전신 |

현재 구조를 "버리는" 게 아니다.
현재 구조에서 배운 **왜**가 Winters Engine의 **어떻게**를 만든다.

---

## 9. 본질적 설계 원칙

**엔진의 역할은 "가능성의 공간"을 정의하는 것이다**

좋은 엔진은 게임 개발자가 게임플레이에 집중할 수 있게 한다.
렌더링이 어떻게 되는지, 네트워크가 어떻게 동기화되는지,
메모리가 어디서 할당되는지 — 이런 것들이 엔진이 처리한다.
게임 개발자는 "챔피언이 이 스킬을 쓰면 어떤 일이 일어나야 하는가"에만 집중한다.

**규모는 설계에서 결정된다, 코드에서 결정되지 않는다**

수십억 명이 동시에 플레이해도 무너지지 않는 시스템은
나중에 성능 최적화를 해서 만들어지지 않는다.
처음부터 수평 확장 가능한 구조, 상태를 공유하지 않는 서비스,
이벤트 드리븐 비동기 통신 — 이것이 처음부터 설계에 있어야 한다.

**신뢰성은 결제와 데이터에서 시작된다**

플레이어가 돈을 냈는데 아이템이 없거나,
구매 내역이 사라지는 순간 서비스는 끝난다.
결제와 인벤토리 데이터는 PostgreSQL + 불변 트랜잭션 로그로 절대 손실이 없어야 한다.
"99.999% 가용성"이 목표가 아니라 전제 조건이다.
