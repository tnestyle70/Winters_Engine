# Phase 04a v2 — D-3 Sub-plan: Verification + Debugging Guide

**작성일**: 2026-04-30 (Codex 1차 검토 보정 2026-04-30)
**상위 문서**: `04a_MVP_2CLIENT_TCP_DEMO_v2.md` §6
**범위**: D-3A (1 client smoke) + D-3B (2 client Move sync) + D-3C (jitter 측정) + 디버깅 가이드
**합격**: Layer 1 MVP 완성 — 1 server + 2 client localhost 5분 무중단 + Move sync 시각 검증 + jitter < 5ms

**한 줄**: **검증 시나리오 3종 + 디버깅 매트릭스 + 회귀 grep 4종 + 시나리오별 로그 패턴 + 트러블슈팅 결정 트리.**

---

## ★ Prerequisite (★ Codex P1-1 신규)

D-3 검증은 D-0 + D-1 + D-2 build gate **모두 통과 후** 실행. 현재 루트의 placeholder 들을
교체하지 않으면 시나리오 1 step 부터 막힘:
- `Server/Private/main.cpp:24` → 아직 placeholder ("Phase 1 - Placeholder"). D-1I 의 `WintersServer v0.2` 로 교체 필수.
- `Client/Public/Network/ClientNetwork.cpp` / `.h` → 부재. D-2A 박제 필수.
- `Client/Public/Network/CommandSerializer.cpp` → 부재. D-2B 박제 필수 (현재는 stub h 만).
- `Shared/Schemas/Hello.fbs` / `Hello_generated.h` → 부재. D-1J 박제 + run_codegen.bat 갱신 필수.

**Preflight 체크 (D-3 시작 직전)**:
```cmd
:: Server v0.2 확인
WintersServer.exe
:: 기대 출력: "[IOCPCore] listening on port 9000 (workers=4)"
:: 만약 "Phase 1 - Placeholder" 가 나오면 D-1I 미적용 — 재빌드 필요.

:: Client 빌드 산출물 확인
dir Client\Bin\Debug\WintersGame.exe
dir Client\Bin\Intermediate\Debug\ClientNetwork.obj  :: D-2A 산출물
```

---

## ★ Codex 1차 검토 보정 요약 (2026-04-30)

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P1-1 | D-0~D-2 build gate prereq 부재 | 본 §Prerequisite | preflight 체크 추가 |
| P1-2 | Client 실행 working dir 충돌 (Resource 상대 경로) | §1.1 | SolutionDir 에서 `Client\Bin\Debug\WintersGame.exe` 실행 |
| P1-3 | jitter 측정 코드 `if` 가 주석에 먹힘 | §3.1 | `if` 블록 명시 (한 줄 주석 분리) |
| P1-4 | Client 가 GUI 앱 (wWinMain) 이라 std::cout 안 보임 | §1.1, §5.2 | `OutputDebugStringA` / DebugView / 파일 로그로 명시 |
| P1-5 | HP debug 검증 — 명령 미구현 + 필드명 `current` | §2.1, §2.2 | debug command 구현 위치 명시 + `fCurrent` |
| P2-6 | grep 게이트 PowerShell 호환 + transport grep 누락 + allowlist | §4 | `Select-String` 버전 + Server/Private/Game 포함 + allowlist |
| P2-7 | `CreateECSChampionFromDef` API 명 오류 | §1.1, §5.1 | 실제 명 `CreateECSChampion` |
| P3-8 | branch prefix `feature/04a-v2-*` vs codex 환경 | §6.2 | 환경별 prefix 가이드 (feature/ 또는 codex/) |

---

## 1. D-3A — 1 client localhost smoke (1h)

### 1.1 시나리오

#### Step 1: Server 실행

★ Server 는 console app 이라 임의 디렉토리에서 실행 가능. 단 리소스 의존이 없는지
확인 (현재는 없음).

```cmd
cd C:\Users\user\Desktop\Winters\Server\Bin\Debug
WintersServer.exe
```

**기대 출력**:
```
[IOCPCore] listening on port 9000 (workers=4)
[Server] WintersServer v0.2 running on port 9000.
[Server] Press 'q' + Enter to quit.
```

#### Step 2: Client 실행

★ **Codex 보정 (P1-2)**: Client 는 `Client/Bin/Resource/...` 같은 **상대 경로** 로
리소스 로드. `Client.vcxproj:39` 의 `LocalDebuggerWorkingDirectory` 도 `$(SolutionDir)`
(= `Winters\` 루트). 따라서 `Client\Bin\Debug` 에서 직접 실행하면 리소스 경로 깨짐.

**올바른 실행 — 루트에서 실행**:
```cmd
cd C:\Users\user\Desktop\Winters
Client\Bin\Debug\WintersGame.exe
```

#### Step 3: InGame 진입

- 메인메뉴 → BanPick 또는 직접 InGame
- Scene_InGame OnEnter 진입 시 로그:

★ **Codex 보정 (P1-4)**: Client 는 `wWinMain` GUI 앱 (Client/Private/main.cpp:13).
`std::cout` 출력은 콘솔에 표시되지 않음. 다음 중 하나로 로그 관측:
- **DebugView** (Sysinternals) — `OutputDebugStringA` 출력 실시간 표시 ★ 권장
- **Visual Studio 출력 창** — 디버거 attach 시 자동 표시
- **파일 로그** — `Client/Bin/Debug/Client.log` 등으로 redirect (코드 수정 필요)

**기대 OutputDebugStringA 출력 (DebugView 또는 VS 출력 창)**:
```
[Scene_InGame] Server connected (127.0.0.1:9000)
[Scene] callbacks registered (snapshot/cmd/network)
```

**Server 로그**:
```
[Session_Manager] OnAccept sid=1
[GameRoom] OnSessionJoin sid=1, entity=1, netId=1
[GameRoom] Hello sent to sid=1
```

#### Step 4: 본인 챔피언 표시
- Client 화면에 이즈리얼 (디폴트) 표시
- ★ Codex 보정 (P2-7): Hello → SnapshotApplier::OnHello → SetOnNewEntityCallback →
  **`CreateECSChampion(eChampion, eTeam)`** (실제 시그니처. `CreateECSChampionFromDef` 는
  존재하지 않음 — Scene_InGame.h:293 참조)

#### Step 5: 우클릭 이동
- 우클릭 → CommandSerializer::SendMove
- Server log:
```
[GameRoom] OnCommandBatch sid=1 cmds=1
[Tick #N] Drain 1 commands, exec 1
```
- 다음 snapshot tick 후 본인 캐릭터 1-2 frame 지연 후 이동 시작

#### Step 6: Disconnect
- Client `q` → Disconnect → Server log:
```
[Session_Manager] OnDisconnect sid=1
[GameRoom] OnSessionLeave sid=1
```

### 1.2 합격 체크리스트
- [ ] WintersServer 실행 시 listen 로그
- [ ] Client connect 시 server 가 sid 발급
- [ ] Hello 도착 시각 < 0.5s
- [ ] 본인 챔피언 화면 표시
- [ ] 우클릭 이동 시 1-2 frame 지연 (RTT < 50ms localhost)
- [ ] Disconnect 시 leak 0 (Server 콘솔에 outstanding pending 메시지 없음)

---

## 2. D-3B — 2 client localhost Move sync smoke (2h)

### 2.1 시나리오

#### Step 1: Server 실행 (위와 동일)

#### Step 2: Client A 실행
- WintersGame.exe (instance 1) → InGame 진입 → Hello 받음 (sid=1, netId=1, team=Blue)
- 본인 챔피언 화면 표시

#### Step 3: Client B 실행
- WintersGame.exe (instance 2) → InGame 진입 → Hello 받음 (sid=2, netId=2, team=Red)
- 본인 챔피언 + Client A 챔피언 둘 다 표시 (snapshot OnNewEntity 콜백)
- Client A 화면에도 Client B 챔피언 새로 등장

#### Step 4: Client A 우클릭 이동
- Client A 화면: 본인 캐릭터 이동 (snapshot Move 적용)
- Client B 화면: **Client A 캐릭터 이동 시각화 (목표!)**
- 1-2 frame 지연 OK (Layer 1 = no prediction)

#### Step 5: 양쪽 동시 입력
- Client A 이동 + Client B 이동 → 양쪽 화면 모두 정확 반영
- 결정성 검증 — 같은 server 에서 두 client 가 본 결과 동일

#### Step 6: HP 디버그 변경 (★ Codex P1-5 — 검증 가능 조건 명시)

**옵션 A — Server 콘솔 명령 (권장, 합격 조건이면 D-1I 에서 구현)**:
- `Server/Private/main.cpp` 의 stdin 루프에 `hp <netId> <value>` 파싱 추가:
```cpp
// main.cpp 의 while (std::getline(std::cin, line)) 안:
if (line.starts_with("hp "))
{
    std::istringstream iss(line.substr(3));
    u32_t netId; f32_t value;
    if (iss >> netId >> value)
    {
        EntityID e = room->GetEntityIdMap().FromNet(netId);
        if (e != NULL_ENTITY && room->GetWorld().HasComponent<HealthComponent>(e))
            room->GetWorld().GetComponent<HealthComponent>(e).fCurrent = value;
    }
}
```
(GetEntityIdMap / GetWorld accessor 는 GameRoom 에 추가 필요 — debug 전용)

**옵션 B — 코드 1회 변경 (smoke 검증만)**:
- ★ Codex P1-5: 실제 필드는 `fCurrent`. 예시:
```cpp
world.GetComponent<HealthComponent>(e).fCurrent = 100.f;
```
GameRoom Tick 중간에 한 번 호출 후 양쪽 client UI HP 갱신 확인.

**옵션 C — HP sync 합격 조건에서 제외**:
- D-3 시점에 debug command 미구현이면 시나리오 6 skip 가능.
- 단 합격 체크리스트 (§2.2) 의 "HP 변경 → 양쪽 UI 갱신" 항목도 함께 빼야 일관됨.

### 2.2 합격 체크리스트
- [ ] Client B 화면에 Client A 챔피언 보임
- [ ] Client A 화면에 Client B 챔피언 보임
- [ ] Client A 이동 → Client B 화면 반영
- [ ] Client B 이동 → Client A 화면 반영
- [ ] HP 변경 → 양쪽 UI 갱신 (★ Codex P1-5: 옵션 A/B 중 택일 구현 시만 — 옵션 C 시 항목 제외)
- [ ] 5분 무중단
- [ ] Connect/Disconnect 반복 5회 → leak 0

---

## 3. D-3C — Tick jitter 측정 (1h)

### 3.1 측정 코드 추가

`Server/Private/Game/GameRoom.cpp` 의 `TickThread` 에:

```cpp
void CGameRoom::TickThread()
{
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    const auto period = std::chrono::microseconds(33333);

    int64_t maxJitterMicros = 0;
    int64_t totalTicks = 0;
    auto lastReport = clock::now();

    while (m_bRunning)
    {
        auto tickStart = clock::now();
        Tick();

        // ★ jitter 측정 — actual vs expected
        int64_t jitter = std::chrono::duration_cast<std::chrono::microseconds>(
            tickStart - next).count();
        if (jitter < 0) jitter = -jitter;
        if (jitter > maxJitterMicros) maxJitterMicros = jitter;
        ++totalTicks;

        // ★ Codex 보정 (P1-3): 30초마다 리포트 — if 가 한 줄 주석에 먹히면
        //   매 tick 로그 폭주 + maxJitterMicros 매 tick reset → 측정 무의미.
        //   if 를 별도 라인으로 분리 + 코멘트는 위에 1줄로 분리.
        if (clock::now() - lastReport > std::chrono::seconds(30))
        {
            std::cout << "[Tick] count=" << totalTicks
                      << " maxJitter=" << maxJitterMicros << " us\n";
            maxJitterMicros = 0;
            totalTicks = 0;
            lastReport = clock::now();
        }

        next += period;
        std::this_thread::sleep_until(next);
    }
}
```

### 3.2 합격
- 5분 구동 후 `maxJitter < 5000 us` (5ms)
- 부하 적은 환경 (다른 무거운 process 없을 때) 에서 < 1000 us 도 가능

---

## 4. 회귀 grep 게이트 (★ 결정성 1차)

★ **Codex 보정 (P2-6)**: 본 환경은 PowerShell — `grep -rn` 은 git-bash / WSL 만 동작.
PowerShell 에선 `Select-String` 사용. 또 transport grep 은 `Server/Private/Game` 도 포함해야
GameRoom 내부의 우발적 transport 호출 잡힘. 결과는 **allowlist 외 0 hit** 가 합격.

### 4.1 회귀 grep 4종 — git-bash 버전

```bash
# 1. /fp:fast 회귀
grep -rn "FloatingPointModel.*Fast" \
    Engine/Include/ Server/Include/ Client/Include/
# 결과: 0 hit

# 2. unordered iteration in sim (allowlist 외)
grep -rn "unordered_map\|unordered_set" \
    Shared/GameSim/ Server/Private/Game/ Server/Private/Security/
# Allowlist (sim 외 lookup-only — hit OK):
#   - Shared/GameSim/EntityIdMap.h          (NetEntityId ↔ EntityID lookup)
#   - Shared/GameSim/Registries/*Registry.h (id → def lookup)
#   - Server/Public/Game/GameRoom.h         (m_sessionToEntity — sim 외 lookup)

# 3. wall-clock in sim
grep -rn "chrono::\|GetTickCount\|time(0\|time(nullptr\|QueryPerformance" \
    Shared/GameSim/ Server/Private/Game/ Server/Private/Security/
# Allowlist:
#   - GameRoom::TickThread 의 steady_clock          (★ sim logic 외부 — OK)
#   - GameRoom::OnCommandBatch 의 system_clock recvMs (★ tick 진입 전 stamp — OK)

# 4. Transport boundary (★ A') — Server/Private/Game 포함 (Codex P2-6)
grep -rn "WSARecv\|WSASend\|recvfrom\|sendto\|AcceptEx" \
    Server/Public/Game/ Server/Private/Game/ Server/Public/Security/ Shared/
# 결과: 0 hit (transport-neutral 강제)
```

### 4.2 회귀 grep 4종 — PowerShell 버전 (★ Codex P2-6)

```powershell
# 1. /fp:fast 회귀
Select-String -Path "Engine\Include\*","Server\Include\*","Client\Include\*" `
    -Pattern "FloatingPointModel.*Fast" -List

# 2. unordered iteration in sim
Select-String -Path "Shared\GameSim\*","Server\Private\Game\*","Server\Private\Security\*" `
    -Pattern "unordered_map|unordered_set" -List `
| Where-Object {
    $_.Path -notmatch "EntityIdMap\.h$" -and
    $_.Path -notmatch "Registries\\.*Registry\.h$" -and
    $_.Path -notmatch "GameRoom\.h$"
}

# 3. wall-clock in sim
Select-String -Path "Shared\GameSim\*","Server\Private\Game\*","Server\Private\Security\*" `
    -Pattern "chrono::|GetTickCount|time\(0|time\(nullptr|QueryPerformance" -List `
| Where-Object {
    -not ($_.Path -match "GameRoom\.cpp$" -and $_.Line -match "TickThread|OnCommandBatch")
}

# 4. Transport boundary
Select-String -Path "Server\Public\Game\*","Server\Private\Game\*","Server\Public\Security\*","Shared\*" `
    -Pattern "WSARecv|WSASend|recvfrom|sendto|AcceptEx" -List
```

### 4.3 합격 — 4종 모두 allowlist 외 0 hit (git-bash / PowerShell 동일)

---

## 5. 디버깅 매트릭스

### 5.1 증상별 결정 트리

| 증상 | 1차 의심 | 디버깅 |
|---|---|---|
| **Server 가 listen 안 됨** | port 점유 | `netstat -ano \| findstr :9000` → kill |
| | WSAStartup 실패 | main.cpp 로그 |
| **Client connect 실패** | server 미실행 | 양쪽 콘솔 동시 확인 |
| | 방화벽 | localhost 는 보통 OK, LAN 시 firewall 허용 |
| **Hello 안 옴** | OnSessionJoin 미호출 | IOCPCore Accept completion 로그 |
| | Send 큐 race | Session::Send mutex 확인 |
| | FrameCallback 미등록 | Scene_InGame OnEnter 로그 |
| **본인 챔피언 안 보임** | OnHello → callback 호출 안 됨 | SetOnNewEntityCallback 등록 시점 |
| | `CreateECSChampion` 실패 (★ Codex P2-7 — `FromDef` 가 아님) | championId 매핑 확인 (eChampion::EZREAL=12) |
| **다른 client 챔피언 안 보임** | snapshot OnNewEntity 미호출 | snapshot payload 안 entity 수 확인 |
| | NetEntityId allowlist | EntityIdMap.Bind 호출 확인 |
| **이동 반영 안 됨** | CommandBatch 송신 X | InputSystem 우클릭 → SendMove 로그 |
| | server EnqueueCommand 미수신 | DispatchCommandBatch 로그 |
| | MoveSystem stat.moveSpeed=0 | StatComponent 초기화 확인 |
| **Tick jitter 큼** | sleep_until 정확도 | timeBeginPeriod(1) 추가 (Windows) |
| | 다른 process 부하 | Task Manager 확인 |
| | snapshot send 부하 | snapshot size 확인 (목표 < 800B) |
| **PDB lock 빌드 실패** | devenv.exe 켜져 있음 | VS 종료 또는 `/p:MultiProcessorCompilation=false` |
| **LNK2019** | Shared cpp 미편입 | D-0A 체크리스트 |
| | Engine project ref 미추가 | D-1A 체크리스트 |

### 5.2 핵심 로그 포인트

**Server 측 추가 권장 로그**:
- `IOCPCore::WorkerLoop` Accept completion: `[IOCP] Accept sid=N`
- `Session_Manager::OnAccept`: `[SM] OnAccept sid=N`
- `GameRoom::OnCommandBatch`: `[Room] cmd batch sid=N count=K`
- `GameRoom::Phase_DrainCommands`: `[Tick #T] drain=K exec=K`
- `GameRoom::Phase_BroadcastSnapshot`: `[Tick #T] snap broadcast to N sids`

**Client 측 추가 권장 로그** (★ Codex P1-4 — GUI 앱이라 `OutputDebugStringA` 사용):
- `ClientNetwork::Connect`: `OutputDebugStringA("[Net] connect ok\n")`
- `ClientNetwork::RecvThread` close: `OutputDebugStringA("[Net] recv closed\n")`
- `Scene_InGame::OnEnter` callback 등록: `OutputDebugStringA("[Scene] callbacks registered\n")`
- `SnapshotApplier::OnHello`: `OutputDebugStringA(buf)` (sprintf_s 로 포맷)
- `SnapshotApplier::OnSnapshot` first time per netId: `OutputDebugStringA("[Snap] new netId=N spawn\n")`

**관측 도구**:
- DebugView (Sysinternals) — Client GUI 앱의 OutputDebugStringA 실시간 표시
- Visual Studio 출력 창 — 디버거 attach 시 자동 표시

### 5.3 RenderDoc / Wireshark 활용

- **Wireshark**: localhost loopback 트래픽 확인 (npcap 필요). port 9000 필터 → CommandBatch / Snapshot envelope 직접 관찰
- **RenderDoc**: Layer 1 시각 동기화 검증 시 메쉬 / Transform 갱신 추적

---

## 6. 위험 시나리오 + 롤백

### 6.1 롤백 단계

| 깨짐 | 롤백 |
|---|---|
| D-0 Shared cpp 편입 후 Server 빌드 깨짐 | Server.vcxproj 의 추가 ItemGroup 통째로 제거 |
| D-0B 5종 system 박제 후 LNK2019 | 5종 cpp 만 빈 함수 (no-op) 로 stub 화 |
| D-1B IOCPCore 본격 후 listen 실패 | 기존 sketch 로 복원 + WSAStartup 로그 |
| D-1G GameRoom 본격 후 tick crash | Phase_SimulationSystems 의 system 호출 1개씩 주석 처리하며 격리 |
| D-2A ClientNetwork 후 Scene 진입 시 hang | Connect 호출을 별도 button 으로 분리 |
| D-2E InputSystem 마이그 후 본인 캐릭터 안 움직임 | CommandSerializer::SendMove 직후 server log 확인 → server 도달 여부 검증 |

### 6.2 안전 branch 전략

★ **Codex 보정 (P3-8)**: 환경별 prefix 가이드. Codex 환경 / Anthropic Claude Code 환경
에선 `codex/`, `claude/` prefix 가 자동 강제되는 경우가 있음. 일반 git 환경 / 사람이 직접
작업 시 `feature/` 사용 OK.

```bash
# 사람이 직접 / 일반 git
git checkout -b feature/04a-v2-d0
# Codex 환경
git checkout -b codex/04a-v2-d0
# Claude Code 환경
git checkout -b claude/04a-v2-d0

# D-0 작업
git commit -m "D-0: Shared cpp link + ServerSimSubset"

# 후속 단계는 동일 prefix 유지
git checkout -b feature/04a-v2-d1 feature/04a-v2-d0     # 또는 codex/04a-v2-d1 codex/04a-v2-d0
# ... (D-1, D-2, D-3 동일 패턴)
```

---

## 7. 합격 시 다음 단계

### 7.1 Layer 1 합격 = 04a v2 MVP 완성
✅ 2 client localhost Move/HP sync 검증 완료
✅ 결정성 가드 4종 통과
✅ Transport boundary 0 hit

### 7.2 다음 사이클 옵션

| 옵션 | 산출물 | 시간 |
|---|---|---|
| **A. Layer 2 (Cast event echo)** — 04a v2 §7 D-4 | `Event.fbs` 확장 + EventApplier + PendingHitSystem 서버 포팅 + DamageQueue 본격 | +20h |
| **B. Sim-10 v2 M1 직진 (UDP 마이그)** | UdpCore/UdpSession/UdpReliabilityChannel | 50h |
| **C. 두 가지 병행** | Layer 2 (시각 검증) + UDP 별도 branch | 70h |

권장: **A → B 순차**. Layer 2 는 시각 demo 완성도 향상 (스킬 FX). UDP 는 production transport. 둘 다 가치.

---

## 8. 한 줄

**D-3 = MVP 합격 검증 (1 client / 2 client / jitter) + 회귀 grep 4종 + 디버깅 매트릭스 + 롤백 branch 전략. Layer 1 합격 = 1 server + 2 client localhost Move/HP sync + Hello 자동 + jitter < 5ms + transport boundary 0 hit. 다음은 Layer 2 (Cast FX) 또는 Sim-10 v2 M1 (UDP).**
