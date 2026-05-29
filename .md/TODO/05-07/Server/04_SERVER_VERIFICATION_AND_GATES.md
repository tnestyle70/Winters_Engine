# Stage Verification + Gates 박제 (Server Fiber Integration)

작성일: 2026-05-07
상위: [`00_INDEX_SERVER_FIBER_INTEGRATION.md`](00_INDEX_SERVER_FIBER_INTEGRATION.md)
대상: Stage 1/2/3 의 검증 매트릭스, 8 게이트 통과 보고, 19 함정 회피 매트릭스, 검증 스크립트, 리스크 매트릭스, 최종 Acceptance.

---

## 1. 8 게이트 통과 매트릭스 (PLAN_AUTHORING_PITFALLS §2)

| 게이트 | 본 박제 묶음 위치 | 통과 보고 |
|---|---|---|
| **A — 사실 수집** | 00_INDEX §2 (모든 코드 사실), 01 §1, 02 §1, 03 §1 | ✅ 모든 인용에 줄 번호 + 직접 인용 블록. PIMPL 클래스 (CSnapshotBuilder 빈 클래스 — PIMPL X / CJobSystem private 멤버는 헤더 인용) 검증. |
| **B — TODO 0** | 01 §1, 02 §1, 03 §1 | ✅ Preflight 표의 모든 행 확정. "필요"/"추정"/"TBD" 0. |
| **C — 호출 경로 grep** | 본 §2 의 검증 스크립트 + 03 §7 | ✅ Phase_BroadcastSnapshot 호출자 = Tick L324 1 곳. Phase_BroadcastSnapshot 의 호출 대상 = Build/WrapEnvelope/Send 3 — 모두 helper 안. Phase_ServerProjectiles/Events/MinionAI 변경 0 명시. 미존재 API 0. |
| **D — ECS 책임 경계** | 00_INDEX §3, 03 §5.3.1 | ✅ Server 의 Scene 의존 0. ECS World 만 의존. 멀티 GameRoom (Sim-10 v2) 진입 시 모든 GameRoom 이 같은 CJobSystem 공유 — Owner 매트릭스 일치. |
| **E — 향후 사례 자료형** | 00_INDEX §6 P-7 회피 + 03 §8 P-7 | ✅ CJobCounter 32-bit 4G ≫ Server max session 8. PerSessionSnapshotInput::lastAckedSeq 32-bit (Sim-10 v2 M2 reliability). |
| **F — Scheduler / 동시성** | 00_INDEX §6 P-9 + 03 §5.3.1 | ✅ 본 박제는 새 phase 0. 같은 phase 안 read+write 0 검증. fractional phase 0. Producer→Consumer 같은 phase 0. |
| **G — Owner Scope** | 00_INDEX §3 매트릭스 | ✅ CServerEntry 프로세스당 1 (RHI Device 패턴). CJobCounter 함수 로컬. PerSessionSnapshotOutput 함수 로컬. m_pSnapBuilder GameRoom 멤버. CGameRoom Tick thread 는 GameRoom 별 1. |
| **H — 인용 의미 + 행동 보존** | 00_INDEX §2.9 + 02 §6.3 + 03 §6.4 | ✅ CLAUDE.md §1.A Track 3 의 "1차 목표 = Fiber shell only" 와 본 박제 (Stage 1 shell + Stage 3 conservative read-only 병렬) 일치. byte-identical 명시. mask 확장 0. P-15 헤더 외부 의존 직접 include. |

---

## 2. 검증 스크립트 (PowerShell)

### 2.1 호출 경로 grep (P-3, P-13 회피 검증)

```powershell
# Submit / WaitForCounter 호출 경로 — Stage 별로 0 또는 정확히 1곳
Write-Host "=== Submit / WaitForCounter 호출 경로 (Server) ==="
rg -n "Submit\(|WaitForCounter\(" `
    "C:\Users\user\Desktop\Winters\Server\Public" `
    "C:\Users\user\Desktop\Winters\Server\Private"

# 기대값:
# Stage 1 적용 후: 0 hit
# Stage 2 적용 후: 0 hit (Stage 2 약속)
# Stage 3 적용 후: GameRoom.cpp 안 BuildSnapshotsParallel 안 1 곳씩
```

### 2.2 Phase_ServerProjectiles / Phase_BroadcastEvents / Phase_ServerMinionAI diff 0 검증 (Stage 3)

```powershell
# Stage 0 baseline (head~1 main) vs Stage 3 적용 후의 diff
git -C "C:\Users\user\Desktop\Winters" diff main..HEAD -- `
    Server/Private/Game/GameRoom.cpp |
    rg "^[\+\-].*(Phase_ServerProjectiles|Phase_BroadcastEvents|Phase_ServerMinionAI)"

# 기대값: 매칭 0 (함수 시그니처 / 본문 변경 0)
```

### 2.3 IOCP worker 회귀 0 검증

```powershell
# IOCPCore.cpp 가 본 박제 묶음에서 변경 0 인지
git -C "C:\Users\user\Desktop\Winters" diff main..HEAD -- `
    Server/Private/Network/IOCPCore.cpp `
    Server/Public/Network/IOCPCore.h

# 기대값: empty diff (본 박제는 IOCP 영역 0 변경)
```

### 2.4 ECS Read-only 보장 검증 (Stage 3 의 Job 안에서 m_world write 0)

```powershell
# BuildSnapshotsParallel 람다 안에서 호출되는 함수 식별
# m_pSnapBuilder->BuildForSession 만 호출 — 그 외 m_world write API 호출 0
rg -n "AddComponent|RemoveComponent|DestroyEntity|CreateEntity" `
    "C:\Users\user\Desktop\Winters\Server\Private\Game\SnapshotBuilder.cpp"

# 기대값: 매칭 0 (BuildForSession → Build 위임 — read-only)
```

### 2.5 `Get_WorkerSlot()` 함정 검증 (CLAUDE.md §1 Track 3)

```powershell
# Server 코드가 CJobSystem::Get_WorkerSlot / Get_WorkerIdx 호출하는지
rg -n "Get_WorkerSlot|Get_WorkerIdx" `
    "C:\Users\user\Desktop\Winters\Server\Public" `
    "C:\Users\user\Desktop\Winters\Server\Private"

# 기대값: 0 hit (본 박제는 Get_WorkerSlot/Idx 호출 0 — 함정 회피)
```

### 2.6 신규 .cpp 의 Server.vcxproj 등록 검증

```powershell
# Stage 1 의 ServerEntry.cpp 가 vcxproj 에 등록되었는지
rg -n "ServerEntry.cpp" "C:\Users\user\Desktop\Winters\Server\Include\Server.vcxproj"

# 기대값: 1 hit ("..\Private\Game\ServerEntry.cpp")
```

### 2.7 행동 보존 (byte-identical) 검증

본 박제 외 별도 단위 테스트 권장 — 본 검증서 §5 의 후속 작업으로 제안.

수동 검증:
```powershell
# Stage 1 baseline + 1 client 5 tick snapshot
.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=5 > stage1_snap.log
# (별도 client 도구로 snapshot bytes hex dump)

# Stage 3 적용 + 1 client 5 tick
.\Server\Bin\Debug\WintersServer.exe --smoke-seconds=5 > stage3_snap.log

# diff stage1_snap.log stage3_snap.log → snapshot bytes 동일 부분만 비교
```

---

## 3. 리스크 매트릭스 + 대응

| 리스크 | 영향 | 대응 |
|---|---|---|
| Engine 측 Phase 5-A Chase-Lev Main-push race fix 미진입 | Stage 3 의 Tick thread (Main thread 와 동격) 에서 Submit 호출 시 Worker Deque race | Stage 3 진입 전 Engine race fix 확인 (CLAUDE.md §1.C). 옵션 A: Main → Global Queue, Worker → 자기 Deque. **본 박제는 race fix 후 Stage 3 진입** 을 진입 조건으로 명시. |
| Engine `CJobSystem::ExecuteItem` 의 Counter Decrement 누락 | Job 안 throw 시 WaitForCounter 영원 대기 → 데드락 | Stage 3 Job 람다 안 try/catch + bValid=false 표시 (03 §5.3.2). C++ 예외가 람다 밖으로 빠져나가지 않음 보장. |
| ECS World N thread 동시 read 안전성 미확정 | Stage 3 의 BuildForSession 동시 호출 시 GetComponent/HasComponent race | Stage 2 §9 의 ECS Read Concurrency 5 항목 검증 통과 후 Stage 3 진입. 미통과 시 conservative fallback (Stage 2 fallback path 직렬). |
| Tick thread 가 m_stateMutex 잡고 yield 시 데드락 | WaitForCounter 가 yield → 다른 fiber 가 m_stateMutex 잡지 못함 → 무한 대기 | Tick 안 m_stateMutex 는 Tick 시작에서 lock_guard 로 잡힘 ([GameRoom.cpp:306](../../../Server/Private/Game/GameRoom.cpp:306)). **Network worker thread (IOCP) 가 m_stateMutex 잡지 않음** 검증: `rg -n "m_stateMutex" Server/` 결과 = Tick 안 1 곳 + 그 외 0 — 확인 필수. 데드락 0 보장. |
| CSession_Manager::Find 동시 호출 안전성 미확정 | Stage 3 의 SendCollectedSnapshots 직렬 호출 — 단 Stage 2 의 CollectBroadcastInputs 가 이미 호출 | 본 박제는 Find 직렬 호출만. 동시 호출 0. |
| pSession->Send 동시 호출 안전성 미확정 | Send queue race | 본 박제는 Send 직렬 호출만 (Stage 3 §1 약속). |
| `m_rng.GetState()` 가 Tick 안에서 변경되는 경우 | rngState 미일치 → snapshot bytes 미일치 | 본 박제 Stage 3 §6.4 의 byte-identical 검증에서 cache 시점 이후 rng write 0 검증. **Phase_BroadcastSnapshot 진입 전 m_rng write 호출이 있는 phase** = Phase_DrainCommands/ExecuteCommands/SimulationSystems/BroadcastEvents 모두 가능 — 단 모두 BroadcastSnapshot **전** 에 끝남. BroadcastSnapshot 안에서는 rng write 0 보장. |
| Stage 1 의 ConvertThreadToFiber 실패 환경 | bFiberMode=false fallback 시 일반 thread 동작 — Stage 3 의 WaitForCounter help-stealing busy-wait 만 동작 (yield 없음) | 행동 보장 (busy-wait 으로 대기 — 추가 latency 만). |
| Sim-10 v2 멀티 GameRoom 진입 시 | 모든 GameRoom 이 같은 CJobSystem 공유 — JobCounter 는 GameRoom 별 함수 로컬 stack 이므로 자연 격리 | 본 박제 Owner Scope 매트릭스 (00_INDEX §3) 와 일치. 멀티 진입 시 추가 박제 0. |

---

## 4. 19 함정 통합 회피 매트릭스 (Stage 1+2+3)

| 함정 | Stage 1 | Stage 2 | Stage 3 | 통합 회피 |
|---|---|---|---|---|
| P-1 데이터 미검증 | ✅ Preflight 표 | ✅ Preflight 표 | ✅ Preflight 표 | 모든 Stage 의 §1 Preflight 행이 줄 번호 + 직접 인용 |
| P-2 PIMPL 추측 | N/A (단순 wrapper) | ✅ CSnapshotBuilder 빈 클래스 검증 | ✅ CJobSystem private 멤버 헤더 인용 | PIMPL 추측 0 |
| P-3 호출 경로 단일 가정 | ✅ TickThread 1 경로 | ✅ Phase_BroadcastSnapshot 1 경로 | ✅ §7 의 Phase_ServerProjectiles/Events/MinionAI 변경 0 명시 | 본 §2.1, §2.2 grep 검증 |
| P-4 ECS / Scene 결합 | ✅ ServerEntry Scene/ECS 의존 0 | ✅ SnapshotBuilder ECS만 의존 (read) | ✅ Job 람다 ECS read-only | Scene 결합 0 |
| P-5 유령 의존 | ✅ 모든 인용 줄 번호 | ✅ 동일 | ✅ 동일 | 통과 |
| P-6 TODO 박제 진입 | ✅ Preflight TODO 0 | ✅ Preflight TODO 0 | ✅ Preflight TODO 0 | 통과 |
| P-7 자료형 미래 사례 | ✅ Stage 1 자료형 추가 0 | ✅ PerSessionSnapshotInput 4G | ✅ CJobCounter 4G | 통과 |
| P-8 인용 의미 반전 | ✅ Stage 1 = shell only (Track 3 일치) | ✅ helper 분리 = byte-identical | ✅ Stage 3 = conservative 병렬 (Track 3 일치) | 인용 의미 일치 |
| P-9 Scheduler 동시성 | ✅ phase 추가 0 | ✅ phase 추가 0 | ✅ 같은 phase 안 read+write 0 | 통과 |
| P-10 Owner Scope | ✅ CServerEntry 프로세스당 1 | ✅ helper 함수 로컬 | ✅ Job/Counter 함수 로컬 | 매트릭스 일치 |
| P-11 도메인 상수 Engine | ✅ 신규 상수 0 | ✅ 신규 상수 0 | ✅ 신규 상수 0 | 통과 |
| P-12 음수 좌표 | N/A | N/A | N/A | 좌표 계산 0 |
| P-13 미존재 API | ✅ Initialize/SetExecutionMode/Shutdown 헤더 인용 | ✅ Build/BuildForSession 헤더 인용 | ✅ Submit/WaitForCounter/Increment 헤더 인용 | 모든 API 헤더 인용 |
| P-14 행동 변경 | ✅ 행동 변화 0 (fallback) | ✅ byte-identical (helper 위임) | ✅ byte-identical (직렬 vs 병렬) | mask 확장 0 |
| P-15 헤더 외부 의존 미include | ✅ ServerEntry.h 가 직접 include | ✅ GameRoom.h 가 SnapshotBuilder.h 직접 include | ✅ GameRoom.cpp 가 JobSystem.h/JobCounter.h 직접 include | 통과 |
| P-16 산술 검증 | ✅ atomic flag 멱등성 | ✅ 산술 0 | ✅ counter 4G ≫ 8 | 통과 |
| P-17 Typedef 일괄 변경 | ✅ typedef 변경 0 | ✅ typedef 변경 0 | ✅ typedef 변경 0 | 통과 |
| P-18 RHI/Engine 인프라 미인지 | ✅ CJobSystem 재사용 | ✅ CSnapshotBuilder 재사용 | ✅ CJobCounter 재사용 | 신설 0 |
| P-19 Render/Sim 결합 | N/A (Server Render 0) | N/A | N/A | 통과 |

---

## 5. 최종 Acceptance Matrix (전체)

본 박제 묶음 (Stage 1+2+3) 적용 완료 시 다음을 모두 통과해야 한다.

### 5.1 빌드

```text
[ ] devenv.exe 종료 후 Engine project 단독 빌드 성공 (PostBuild EngineSDK/inc 동기화)
[ ] Server project 빌드 (Debug + Release) 성공
[ ] /utf-8 + /fp:precise + Mswsock.lib + ws2_32.lib 모두 유지
[ ] 신규 .cpp (ServerEntry.cpp) 가 Server.vcxproj 에 등록됨 (§2.6 grep 통과)
```

### 5.2 회귀 (실행)

```text
[ ] WintersServer.exe --smoke-seconds=30 동작
[ ] [ServerEntry] Initialize OK workers=N mode=FiberShell 로그 1회
[ ] [Tick] Fiber shell entered (room=1) 로그 1회
[ ] [IOCPCore] listening on port 9000 (workers=4) 로그 1회
[ ] [Tick #N] snap broadcast to M sids (parallel) 로그 (Stage 3 적용 후)
[ ] [Tick] Fiber shell exited (room=1) 로그 1회
[ ] [ServerEntry] Shutdown complete 로그 1회
[ ] exit code 0 — 30s smoke 동안 crash 0
[ ] Tick maxJitter < 1000us 유지 (Stage 0 baseline + 200us 이내)
```

### 5.3 행동 보존

```text
[ ] Stage 1 baseline vs Stage 3 적용 후 1 client smoke 의 snapshot bytes byte-identical
[ ] 8 session 모킹 (또는 단위 테스트) — 직렬 vs 병렬 결과 byte-identical
[ ] Phase_ServerProjectiles / Phase_BroadcastEvents / Phase_ServerMinionAI 의 git diff 0
[ ] CIOCPCore::WorkerLoop 변경 0 (§2.3 검증 통과)
[ ] BanPick TCP 회귀 0 (1 client 연결 + LobbyState 수신 동작)
```

### 5.4 호출 경로 검증

```text
[ ] §2.1 grep Submit/WaitForCounter 결과: GameRoom.cpp 의 BuildSnapshotsParallel 안 1 곳씩 — 그 외 0
[ ] §2.5 grep Get_WorkerSlot/Get_WorkerIdx 결과: 0 hit (Server 측)
[ ] §2.4 grep ECS write API in SnapshotBuilder.cpp: 0 hit
```

### 5.5 데드락/안전

```text
[ ] WaitForCounter 데드락 0 (30s smoke + 8 session 모킹 통과)
[ ] m_stateMutex 가 Network worker thread 에서 잡히지 않음 검증: rg -n "m_stateMutex" Server/ 결과 = Tick (GameRoom.cpp:306) 1 곳
[ ] ConvertThreadToFiber 실패 시 fallback 동작 보장
```

### 5.6 컨벤션 (CLAUDE.md §6)

```text
[ ] 신규 클래스 C 접두사 (CServerEntry)
[ ] 신규 파일명 C/I 접두사 X (ServerEntry.h, ServerEntry.cpp)
[ ] enum class 이미 e 접두사 (eJobExecutionMode — 기존)
[ ] 신규 멤버 변수 m_ 접두사 (s_ for static member — 자체 컨벤션)
[ ] 신규 함수에서 raw float/int 사용 X (u32_t/f32_t/bool_t 사용)
[ ] WINTERS_ENGINE dllexport 클래스 + unique_ptr 멤버 0 (CServerEntry static 멤버만)
[ ] include 모두 폴더 경로 포함 ("Game/SnapshotBuilder.h", "Core/JobSystem.h")
```

---

## 6. PR 체크리스트 (커밋 전)

```text
=== 빌드 ===
[ ] devenv.exe 종료 확인
[ ] Engine 빌드 → EngineSDK/inc 동기화 OK
[ ] Server 빌드 (Debug)
[ ] Server 빌드 (Release)

=== 검증 스크립트 ===
[ ] §2.1 호출 경로 grep 통과
[ ] §2.2 Phase_* diff 0 통과
[ ] §2.3 IOCPCore diff 0 통과
[ ] §2.4 ECS write API 0 통과
[ ] §2.5 Get_WorkerSlot 0 통과
[ ] §2.6 vcxproj 등록 통과

=== 동작 ===
[ ] 30s smoke 통과
[ ] 1 client BanPick 회귀 0
[ ] 8 session 모킹 byte-identical 통과 (또는 단위 테스트)

=== 컨벤션 ===
[ ] §5.6 컨벤션 항목 모두 통과
[ ] PostBuild 가 WintersEngine.dll Bin/Debug 에 정상 copy

=== 게이트 ===
[ ] §1 8 게이트 통과 보고 작성
[ ] §4 19 함정 회피 매트릭스 통과 보고 작성
```

---

## 7. 후속 작업 (out of scope, 명시만)

본 박제 묶음 통과 후 추가 트랙:

| 후속 작업 | 진입 조건 | 위치 (예상) |
|---|---|---|
| Engine `CJobSystem` Phase 5-A Chase-Lev Main-push race 정식 수정 | CLAUDE.md §1.C 미결 — Engine 측 별도 PR | `.md/plan/engine/...` 별도 박제 |
| Engine Fiber 본체 구현 (`CFiberJobSystem` etc.) | FIBER_JOB_SYSTEM.md v2 (codex 추가 검토 반영) 진입 후 | `.md/plan/engine/FIBER_JOB_SYSTEM.md` |
| Server `Phase_ServerMinionAI` 2-pass (Decision-Apply) 분리 | Engine race fix + Get_WorkerSlot 정정 진입 후 | `.md/TODO/05-NN/Server/05_*.md` 별도 박제 |
| Server `Phase_ServerProjectiles` collision query read-only 분리 | Stage 3 통과 후 (큰 이득 미확실 — 후순위) | 별도 박제 |
| Sim-10 v2 멀티 GameRoom 진입 | Sim-04a v2 D-0~D-3 통과 후 | `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md` 진입 |
| AnimUpdate 병렬화 (Client 측) | Phase 5-B Engine Fiber 본체 진입 후 | CLAUDE.md §1.C 미결 — 별도 박제 |

---

## 8. 본 박제 묶음 종합 영향 보고

### 8.1 변경 라인 수 합계

```
Server/Public/Game/ServerEntry.h         신규  +60L
Server/Private/Game/ServerEntry.cpp      신규  +75L
Server/Public/Game/SnapshotBuilder.h     수정  +35L (Input + Output POD + BuildForSession)
Server/Private/Game/SnapshotBuilder.cpp  수정  +18L (BuildForSession 위임)
Server/Public/Game/GameRoom.h            수정  +25L (helper 4개 선언)
Server/Private/Game/GameRoom.cpp         수정  +130L (Tick fiber shell + helper 정의 + Phase_BroadcastSnapshot 분해)
Server/Private/main.cpp                  수정  +14L (Initialize/Shutdown 호출)
Server/Include/Server.vcxproj            수정  +1L  (ServerEntry.cpp 등록)
─────────────────────────────────────────────────
합계:                                          +358L (신규 +135L, 수정 +223L)
삭제 라인 수: ~36L (Phase_BroadcastSnapshot 본문이 helper 호출로 압축)
순 증가: ~322L
```

### 8.2 핵심 변경점

```text
Stage 1: CServerEntry 글로벌 + ConvertThreadToFiber shell + SetExecutionMode(FiberShell)
         → 행동 변화 0
Stage 2: CSnapshotBuilder::BuildForSession 위임 + GameRoom::CollectBroadcastInputs/BroadcastSnapshotForInput helper
         → 행동 변화 0 (byte-identical)
Stage 3: GameRoom::BuildSnapshotsParallel/SendCollectedSnapshots — Phase_BroadcastSnapshot N session 병렬
         → 행동 변화 0 (byte-identical, conservative — Send 직렬 유지)
```

### 8.3 영향 받지 않는 모듈

```text
✅ CIOCPCore (Network/IOCPCore.h/cpp) — 변경 0
✅ CSession / CSession_Manager — 변경 0
✅ CPacketDispatcher / CFrameParser — 변경 0
✅ CGameRoom 의 Phase_DrainCommands / ExecuteCommands / SimulationSystems
   ServerMinionWave / ServerMinionAI / ServerTurretAI / ServerProjectiles
   BroadcastEvents — 변경 0
✅ Engine/Public/Core/JobSystem.h — 변경 0
✅ Engine/Public/Core/JobCounter.h — 변경 0
✅ Engine/Public/Core/Fiber/* — 변경 0
✅ ECS / Shared/GameSim / Schemas — 변경 0
✅ Backend HTTP / 인증 / 매칭 — 변경 0
✅ BanPick TCP / GameStart / LobbyState — 변경 0
```

---

## 9. CLAUDE.md 정합성 보고

| CLAUDE.md 항목 | 본 박제 정합성 |
|---|---|
| §1.A Track 3 (Fiber JobSystem 박제) — 호출부 유지 | ✅ Submit/WaitForCounter/CJobCounter API 변경 0 |
| §1.A Track 3 — 1차 목표 = Fiber shell only | ✅ Stage 1 = shell only. Stage 3 도 conservative (BuildForSession 만 병렬, Send 직렬) |
| §1.A Track 3 — Get_WorkerSlot() 함정 | ✅ Server 측 Get_WorkerSlot/Idx 호출 0 (§2.5 grep) |
| §1.C 미결 — Phase 5-A Chase-Lev Main-push race | Stage 3 진입 조건으로 명시 (§3 리스크 첫 행) |
| §5.7 P-1~P-19 함정 | §4 매트릭스 통과 |
| §6.1 네이밍 (C 접두사 클래스, 파일 X) | ✅ CServerEntry, ServerEntry.h/cpp |
| §6.2 타입 alias (u32_t/f32_t/bool_t) | ✅ 본 박제 신규 코드 모두 alias 사용 |
| §6.3 클래스 설계 (생성자 private, 정적 팩토리) | ✅ CServerEntry 의 ctor private + static Initialize 패턴. CSnapshotBuilder 기존 패턴 보존. |
| §6.5 GameInstance 경계 | Server 는 GameInstance 사용 X — 별도 CServerEntry. Client 의 Tier-1/2 패턴과 독립 |
| §6.6 Include / ComPtr | ✅ 모든 외부 의존 직접 include. ComPtr 사용 0 (Server 는 DX 미사용) |
| §8 계획서 규칙 (전문 박제, 줄 번호, 수정 전/후) | ✅ 본 박제 묶음 5 파일 모두 h/cpp 전문 + 줄 번호 + 인용 |

---

## 10. 본 박제 묶음 종료 보고

```text
박제 완료일: 2026-05-07
박제자: claude-opus-4-7[1m]

박제 파일 (5개):
✅ 00_INDEX_SERVER_FIBER_INTEGRATION.md   (본 묶음 인덱스)
✅ 01_SERVER_FIBER_SKELETON.md            (Stage 1, h/cpp 전문)
✅ 02_SERVER_TICK_FIBER_AWARE.md          (Stage 2, h/cpp 전문)
✅ 03_SERVER_PARALLEL_PHASES.md           (Stage 3, h/cpp 전문)
✅ 04_SERVER_VERIFICATION_AND_GATES.md    (본 검증서)

PLAN_AUTHORING_PITFALLS 통과 (8 게이트, 19 함정):
✅ 모든 게이트 통과 (§1)
✅ 모든 함정 회피 (§4)

진입 절차:
  1. Engine 측 Phase 5-A Chase-Lev Main-push race fix 확인 (CLAUDE.md §1.C)
  2. Stage 1 적용 → §5.2 30s smoke 통과
  3. Stage 2 적용 → §5.3 byte-identical 검증 통과
  4. (선택) Engine 측 Fiber 본체 구현 진입 (FIBER_JOB_SYSTEM.md v2)
  5. Stage 3 적용 → §5.3 8 session byte-identical 검증 + §5.5 데드락 0 통과
  6. 본 §6 PR 체크리스트 통과 → 머지

후속 트랙:
  - Engine Fiber 본체 (FIBER_JOB_SYSTEM.md v2)
  - Server Phase_ServerMinionAI 2-pass 분리
  - Sim-10 v2 멀티 GameRoom 진입
```

---

**END OF VERIFICATION**
