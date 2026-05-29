# Winters Engine — Architecture Final (v3.0)

> 자체 엔진의 목표: 목표한 게임에 맞게 100%의 성능을 끌어내는 것.
> **범용 엔진 DLL** — 하나의 엔진으로 여러 게임 프로젝트 구동
> 첫 번째 타겟: **LoL 30일 모작** (풀스택 MOBA)
> 두 번째 타겟: **엘든링 모작** (액션RPG)

---

## 1. 엔진 비전

### 1-1. 왜 자체 엔진인가

범용 엔진(Unity, Unreal)은 모든 장르를 지원하기 위해 불필요한 추상화와 오버헤드를 가진다.
Winters Engine은 **멀티플레이어 PvP(vE) 전용**으로 설계하여:

- **네트워크 최우선 아키텍처**: 엔진 코어부터 네트코드를 고려한 설계
- **서버 권위(Server-Authoritative)**: 치트 방지가 엔진 레벨에서 보장
- **대규모 동시 엔티티 처리**: ECS + JobSystem으로 100+ 캐릭터 실시간 시뮬레이션
- **낮은 입력 지연**: 클라이언트 예측 + 서버 보정이 엔진 내장

### 1-2. 타겟 게임 프로젝트

| 순서 | 프로젝트 | 참고작 | 카메라 | 핵심 기술 요구 |
|------|---------|--------|--------|----------------|
| **1st** | **WintersLOL** (30일 모작) | LoL (MOBA) | 탑다운 고정 | 5v5, 스킬 판정, 미니맵, 포그 오브 워, 매치메이킹 |
| **2nd** | **WintersElden** (이후) | 엘든링 (액션RPG) | 3인칭 | 보스 AI, IK 애니메이션, 오픈월드, 레벨 스트리밍 |

**공통 요구사항 (엔진이 제공):**
- ECS + JobSystem으로 대규모 엔티티 처리
- Deferred Rendering + PostFX
- Jolt Physics (충돌, 히트박스)
- FMOD 오디오
- Lua 스크립팅 (스킬/밸런스 데이터 드리븐)

**LoL 전용:**
- 실시간 멀티플레이어 (서버 20 TPS, 클라이언트 60 FPS)
- 서버 권위 게임 로직 + 클라이언트 예측 + 서버 보정
- AOI 기반 네트워크 최적화
- Go 백엔드 (Auth, Shop, Matchmaking)
- 커널 레벨 안티치트

**엘든링 전용 (LoL 이후 추가):**
- 3인칭 카메라 컨트롤러 (Spring Arm, 락온)
- 레벨 스트리밍 (오픈월드)
- 고급 애니메이션 (IK, 레이어드 블렌딩, CCD)
- 보스 AI (Behavior Tree)
- 세이브/로드 시스템

---

## 2. 기술 스택

| 영역 | 기술 | 선택 근거 |
|------|------|-----------|
| **언어** | C++20 | 성능 + 현대 문법 (concepts, ranges, coroutines) |
| **그래픽** | DirectX 11 → RHI 추상화 | DX11: Compute Shader 지원 + 빠른 개발, RHI: 향후 DX12 |
| **물리** | Jolt Physics (MIT) | 멀티스레드, 결정론적, 무료 |
| **오디오** | FMOD Studio | 3D 사운드, 런타임 믹싱, 인디 무료 |
| **스크립팅** | Lua 5.4 | 스킬/밸런스 핫리로드, C++ 바인딩 용이 |
| **직렬화** | FlatBuffers | 제로카피 역직렬화 → 네트워크 패킷 최적 |
| **게임 서버** | C++ (Windows IOCP) | 10,000+ 동시 접속, 싱글 스레드 게임 로직 |
| **백엔드** | Go 마이크로서비스 | Auth, 매치메이킹, 상점, 리더보드 |
| **DB** | PostgreSQL + Redis | 영속 데이터 + 실시간 캐시/랭킹 |
| **메시지 브로커** | Kafka | 서비스 간 이벤트 드리븐 통신 |
| **오케스트레이션** | Kubernetes + Agones | 게임 서버 오토스케일링 |

---

## 3. 엔진 아키텍처 레이어

```
┌─────────────────────────────────────────────┐
│            GAME LAYER (Client)              │
│  CGameApp, Scene, GameObject, UI/HUD        │
├─────────────────────────────────────────────┤
│           GAMEPLAY SYSTEMS                  │
│  Ability(Lua), FSM, BehaviorTree, NavMesh   │
├─────────────────────────────────────────────┤
│           ENGINE SYSTEMS                    │
│  ECS World, JobSystem, EventBus, Asset Mgr  │
├─────────────────────────────────────────────┤
│           NETWORK LAYER                     │
│  UDP/KCP Transport, ClientPrediction,       │
│  ServerReconciliation, AOI, Snapshot        │
├─────────────────────────────────────────────┤
│           RENDERER                          │
│  RenderGraph, Deferred Pipeline, Animation  │
│  Clustered Lighting, Shadow, PostFX         │
├─────────────────────────────────────────────┤
│           RHI (DX11 Backend)                │
│  Device, Shader, Buffer, Pipeline,          │
│  ConstantBuffer, Geometry                   │
├─────────────────────────────────────────────┤
│           PLATFORM                          │
│  Win32 Window, Input, Timer, FileIO         │
├─────────────────────────────────────────────┤
│           CORE                              │
│  Memory Allocators, Math(SIMD), Containers  │
└─────────────────────────────────────────────┘
```

### 의존성 방향 (필터 번호 = 레이어)
```
00. Manager(RHI) ← 01. Core ← 02. Structure ← 03. Renderer ← 04. Editor
                                                    ↑
05. ECS ← 06. Resource ← 07. Physics ← 08. Audio ← 09. Network ← 10. JobSystem
```
낮은 번호는 높은 번호에 의존하지 않는다.

---

## 4. 핵심 시스템 상세

### 4-1. ECS (Entity-Component-System)

**왜 ECS인가:** 멀티플레이어 PvP에서 100+ 엔티티(챔피언, 미니언, 투사체, 이펙트)를
매 틱 처리해야 한다. 상속 기반 OOP는 캐시 미스와 가상 함수 오버헤드로 성능 병목.

```
설계:
  Entity   = uint32 ID (생성/삭제 O(1))
  Component = 순수 데이터 구조체 (SoA 배열)
  System   = 로직 함수 (Component 조합 쿼리 → 일괄 처리)
  World    = Entity + Component 컨테이너

PvP 특화:
  - NetworkComponent: entityId, ownerId, lastServerTick
  - PredictionComponent: pendingInputs[], confirmedState
  - AOIComponent: gridCell, visibilityMask
  - AbilityComponent: skillSlots[4], cooldowns[], castState
```

### 4-2. 네트워크 아키텍처 (PvP 핵심)

**하이브리드 넷코드:**

| 영역 | 방식 | 이유 |
|------|------|------|
| **게임 로직** (스킬 판정, 데미지) | Lockstep | 결정론적 → 리플레이 무료, 치트 방지 |
| **이동/물리** | State Replication | 반응성 우선, 클라이언트 예측 |

```
클라이언트 예측 흐름:
  1. 입력 발생 → 로컬 즉시 적용 (예측)
  2. 입력을 서버에 전송 (UDP/KCP)
  3. 서버 검증 → 권위적 상태 브로드캐스트
  4. 클라이언트 수신 → 예측과 비교
     - 일치: 아무것도 안 함
     - 불일치: 서버 상태로 스냅백 → 이후 입력 재적용
```

**AOI (Area of Interest):**
```
  맵을 50m × 50m 그리드로 분할
  각 엔티티는 자신의 셀 + 주변 3×3 셀만 업데이트 수신
  100명 배틀로얄 → 실제 전송 대상 9~15명으로 감소 (90% 패킷 절감)
```

**지연 보상 (Lag Compensation):**
```
  서버가 과거 N틱의 월드 스냅샷 보관
  피격 판정 시 공격자의 RTT/2만큼 과거 스냅샷으로 롤백
  해당 시점의 적 위치로 히트박스 판정 → 결과를 현재 틱에 적용
```

### 4-3. 렌더링 파이프라인

**Deferred Rendering (지연 렌더링):**
```
Pass 1: G-Buffer 생성
  → RT0: Albedo.rgb + Metallic.a
  → RT1: Normal.xyz (옥타헤드럴 인코딩)
  → RT2: Roughness + AO + EmissiveMask
  → Depth: 24bit depth + 8bit stencil

Pass 2: 라이팅 (Compute Shader)
  → Clustered Deferred: 200+ 동적 라이트 처리
  → Cascaded Shadow Maps: 4단계 캐스케이드

Pass 3: PostFX
  → TAA (Temporal Anti-Aliasing)
  → SSAO (Screen-Space Ambient Occlusion)
  → Bloom + Color Grading
```

**왜 Deferred인가:** MOBA/배틀로얄에서 다수의 스킬 이펙트(불, 번개, 폭발)가
동시에 발생 → 라이트 수에 비례하지 않는 O(1) 라이팅 비용 필수.

### 4-4. JobSystem (Fiber 기반)

```
목표: 12코어 CPU 100% 활용

구조:
  Worker Thread × (코어 수 - 2)
  각 Worker: Fiber Pool에서 작업 스틸
  의존성 그래프: Counter 기반 (Naughty Dog GDC 2015)

PvP 병렬화 예시 (매 프레임):
  [Physics Update] ──┐
  [Animation Update]─┤──→ [Render Submit] ──→ [GPU Kick]
  [AI BehaviorTree]──┘
  [Network Recv] ──→ [Server Reconciliation] ──→ [Prediction Replay]
```

### 4-5. 스킬/어빌리티 시스템 (데이터 드리븐)

```lua
-- Lua 스킬 정의 예시 (밸런스 패치 = Lua 파일만 수정)
DefineSkill("Fireball", {
  cooldown = 8.0,
  manaCost = 60,
  castTime = 0.5,
  range    = 800,
  damage   = { base = 150, apRatio = 0.7 },
  projectile = {
    speed = 1200,
    radius = 40,
    onHit = function(caster, target)
      ApplyDamage(caster, target, CalcMagicDamage(caster, target))
      ApplyBuff(target, "Ignite", 3.0)  -- 3초 도트
    end
  }
})
```

**서버 검증:** 클라이언트는 스킬 입력만 전송. 서버가 Lua 스킬 로직을 실행하여
쿨다운, 마나, 사거리, 히트 판정을 모두 검증.

---

## 5. 서버 인프라

### 5-1. 게임 서버 (C++ IOCP)

```
구조:
  Network Thread (IOCP) → 패킷 수신/파싱
       ↓ (Lock-Free Queue)
  Logic Thread (단일) → 게임 틱 처리 (20~60 TPS)
       ↓ (Lock-Free Queue)
  DB Thread Pool → PostgreSQL/Redis 비동기 쿼리

서버 권위 검증:
  - 이동 속도 제한 (speedhack 방지)
  - 스킬 사거리 제한 (range hack 방지)
  - 쿨다운 서버 타이머 (쿨감 핵 방지)
  - 시야 밖 정보 미전송 (맵핵 방지)
```

### 5-2. 백엔드 마이크로서비스 (Go)

```
┌──────────┐  ┌────────────┐  ┌─────────────┐
│ Auth     │  │ Matchmaking│  │ Leaderboard │
│ (JWT)    │  │ (MMR/Elo)  │  │ (Redis ZSet)│
└────┬─────┘  └─────┬──────┘  └──────┬──────┘
     │              │                │
     └──────────────┼────────────────┘
                    │
              ┌─────▼─────┐
              │   Kafka    │ (이벤트 브로커)
              └─────┬─────┘
                    │
     ┌──────────────┼────────────────┐
     │              │                │
┌────▼─────┐  ┌────▼──────┐  ┌─────▼──────┐
│ Payment  │  │ Shop/Inv  │  │ Profile    │
│ (PG+코인)│  │ (카탈로그) │  │ (전적/통계)│
└──────────┘  └───────────┘  └────────────┘
```

### 5-3. 분산 게임 서버 토폴로지

```
[Gate Server] ← 클라이언트 최초 접속, 토큰 인증
     ↓
[Login Server] ← 계정 검증, 세션 발급
     ↓
[Center Server] ← 서버 목록, 로비, 채팅
     ↓
[Match Server] ← 매치메이킹 결과 수신, 게임 서버 할당
     ↓
[Game Server] ← 실제 게임 시뮬레이션 (Agones 오토스케일)
```

---

## 6. 개발 페이즈

### Phase 1: LoL 30일 모작 (상세: `.md/.plan/LOL_30DAY_MASTER_PLAN.md`)

| Phase | 일차 | 내용 | 핵심 산출물 |
|-------|------|------|------------|
| **0** | D0~D2 | 에셋 파이프라인 | .wmesh/.wanim/.wmat, Blender 변환기, DirectXTex |
| **1** | D3~D5 | 코어 강화 | Fiber JobSystem, Allocator, EventBus |
| **2** | D6~D10 | Deferred Pipeline | RenderGraph, G-Buffer, Clustered Lighting, CSM, PostFX |
| **3** | D11~D13 | GPU-Driven & Profiling | GPU Cull CS, IndirectDraw, Profiler |
| **4** | D14~D19 | 네트워크 & 서버 | UDP/KCP, IOCP, GameRoom, AOI, 클라이언트 예측 |
| **5** | D20~D23 | Go 백엔드 | Auth, Shop, Matchmaking, Profile, Kafka |
| **6** | D24~D26 | 안티치트 | 커널 드라이버, 유저모드 서비스, 서버 검증 |
| **7** | D27~D28 | 에디터 & 콘텐츠 | ImGui, 소환사의 협곡, Lua 챔피언 |
| **8** | D29 | 통합 테스트 | E2E 테스트, 성능 최적화, 메모리 검사 |

### Phase 2: 엘든링 모작 (LoL 완료 후, 기간 TBD)

| 단계 | 내용 | 핵심 산출물 |
|------|------|------------|
| **E-1** | 3인칭 카메라 & 캐릭터 컨트롤러 | Spring Arm, 락온, 회피/구르기 |
| **E-2** | 전투 시스템 | 스태미나, 패링, 강/약 공격, 히트 판정 |
| **E-3** | 보스 AI | Behavior Tree, 페이즈 전환, CCD IK |
| **E-4** | 오픈월드 | 레벨 스트리밍, LOD, 지형 렌더링 |
| **E-5** | 세이브/로드 & 폴리싱 | 직렬화, 화톳불 시스템, Co-op |

---

## 7. 코드 컨벤션

| 규칙 | 예시 |
|------|------|
| 클래스명 C 접두사 | `CTimer`, `CCamera`, `CDX11Device` |
| 멤버 변수 m_ | `m_position`, `m_pDevice` |
| static s_, global g_ | `s_instance`, `g_engineConfig` |
| 상수 ALL_CAPS | `MAX_PLAYERS`, `TICK_RATE` |
| 헤더 가드 | `#pragma once` |
| COM 객체 | `ComPtr<ID3D11Device>` (raw 금지) |
| D3D API | `HRESULT` 반드시 체크 |
| 리소스 관리 | RAII, unique_ptr, shared_ptr |
| 최적화 | Dirty Flag, SoA, SIMD |
| cbuffer | 16바이트 정렬 (alignas(16)) |

---

## 8. vcxproj.filters 구조

### Engine
```
Include              공개 API 헤더 (DLL 경계)
00. Manager          DX11/RHI (GraphicDev, Pipeline, Shader, Buffer, ConstantBuffer, Geometry)
01. Core             Timer, Transform, Platform(Window/Input), Paths
02. Structure        Framework(게임루프), Entry(DLL 진입점)
03. Renderer         Camera, Triangle, Cube
04. Editor           [예약] ImGui 에디터
05. ECS              [예약] Entity/Component/System/World
06. Resource         [예약] Texture/Mesh/Material 로더
07. Physics          [예약] Jolt Physics 래퍼
08. Audio            [예약] FMOD 래퍼
09. Network          [예약] UDP/KCP 트랜스포트
10. JobSystem        [예약] Fiber 워커 스레드
```

### Client
```
00. MainApp          main.cpp + CGameApp
01. Scene            [예약] Logo, Loading, Stage
02. GameObject       [예약] Player, Monster, Environment
03. Manager          [예약] PathFinding, Inventory, Damage
Shaders              HLSL 파일
```

### Server
```
00. Server           main.cpp (서버 엔트리)
01. Network          [예약] IOCP, Session, PacketHandler
02. Game             [예약] GameRoom, GameLogic, AOI
Shared               클라이언트-서버 공유 패킷 정의
```
