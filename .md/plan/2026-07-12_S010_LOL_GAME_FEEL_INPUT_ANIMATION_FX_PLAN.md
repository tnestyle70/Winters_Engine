Session - S010은 LoL식 조작감을 단순한 이동 속도나 이펙트 크기가 아니라 `의도 포착 -> 같은 프레임의 로컬 반응 -> 서버 권위 판정 -> 동일 시간축의 애니메이션/FX/사운드 -> 제한된 오차 정정`이라는 폐루프로 정의한다. 현재 Winters의 서버 권위 뼈대와 이동 잠금 정책은 유지하되, Direct LAN 이중 writer, 로컬 위치 prediction 부재, 30 Hz snapshot을 매번 55 ms SmoothStep으로 뒤쫓는 구조, 기본 공격 impact가 전체 action 끝에 놓인 문제, 하드 애니메이션 전환, FX의 late-start 미보정, 카메라 이중 평활화와 60 FPS 기본 페이싱을 측정 가능한 Feel Budget으로 순서대로 교정한다. 실제 LoL의 내부 구현을 추정해 복제하는 것이 아니라 동일 조건의 고속 캡처와 반복 시나리오에서 입력 지연·속도 jerk·phase error·정정 오차를 비교해 수치를 확정한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

서버 권위 규칙 바로 아래에 조작감 소유권을 추가한다. 조작감의 본질은 “즉시성” 하나가 아니라 다음 네 조건의 동시 충족이다.

- Responsiveness: 입력이 유실되지 않고 같은 프레임에 cursor indicator, facing, animation anticipation 중 최소 하나로 응답한다.
- Continuity: 위치·회전·카메라·관절 pose의 속도와 가속도가 snapshot마다 다시 0에서 시작하지 않는다.
- Phase alignment: 서버의 Cast/Impact/Recovery tick과 애니메이션, FX, 사운드가 같은 사건을 가리킨다.
- Trust: prediction이 틀려도 correction은 제한된 시간과 거리 안에서 끝나며 gameplay truth를 만들지 않는다.

기존 코드:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual
```

아래에 추가:

```text
Gameplay Feel ownership

AuthoritativeState
- Shared/Server GameSim만 이동 성공, 충돌, 공격 impact, 피해, CC, cooldown을 확정한다.

PredictedPresentation
- 로컬 플레이어의 cursor feedback, facing, locomotion anticipation, predicted render transform만 소유한다.
- 서버 결과가 오면 authoritative base로 rebase하고 아직 처리되지 않은 입력만 다시 적용한다.
- gameplay component, target health, cooldown, nav truth를 확정하지 않는다.

RemotePresentation
- 원격 champion/minion은 server-time sample buffer를 사용한다.
- 마지막 snapshot endpoint를 고정 시간 SmoothStep으로 반복 추적하지 않는다.

PresentationTimeline
- action start, impact, FX, sound, camera impulse는 serverTick/startTick을 공통 기준으로 사용한다.
- 늦게 수신한 cue는 frame 0부터 다시 시작하지 않고 age를 fast-forward한다.

Tuning order
1. dual writer / 잘못된 timing / 입력 유실 제거
2. input-to-command, input-to-first-motion, correction, phase error 계측
3. prediction / interpolation / crossfade 구조 확정
4. camera, FX, sound, targeting radius 수치 튜닝
5. 60/144/300 FPS와 localhost/LAN에서 동일 정책 검증
```

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

Direct IP/LAN 연결도 shared roster 연결과 동일하게 서버 권위 경로를 사용해야 한다. 현재는 연결에 성공해 snapshot을 받더라도 `m_bUsingSharedNetwork == false`이면 로컬 Navigation, Unit AI, Turret, Projectile, BT, MCTS가 같이 켜질 수 있다.

기존 코드:

```cpp
InitializeNetworkSession();
m_bNetworkAuthoritativeGameplay =
    m_bUsingSharedNetwork || m_bReplayPlaybackMode;
```

아래로 교체:

```cpp
InitializeNetworkSession();
const bool_t bConnectedToAuthoritativeServer =
    m_pNetworkView && m_pNetworkView->IsConnected();
m_bNetworkAuthoritativeGameplay =
    m_bReplayPlaybackMode || bConnectedToAuthoritativeServer;
```

`CMinion_Manager::Set_Enabled`, client `CNavigationSystem`, `CLocalUnitAISystem`, NavigationThrottle, Turret/Projectile, BT/MCTS 등록 조건은 이 단일 플래그만 계속 사용한다. 별도의 “Direct LAN 예외” 경로는 만들지 않는다.

### 1-3. C:/Users/user/Desktop/Winters/Engine/Public/Core/CInput.h

현재 boolean current/previous 비교는 한 render frame 안에 down/up이 모두 들어오면 click을 잃는다. 상태와 event를 분리하되 heap allocation 없는 bounded queue를 사용한다.

`class CInput` public 영역의 `MouseRay` 선언 위에 아래를 추가:

```cpp
enum class ePointerButton : u8_t
{
    Left = 0,
    Right = 1,
};

struct PointerPressEvent
{
    ePointerButton eButton = ePointerButton::Left;
    int32 iX = 0;
    int32 iY = 0;
    u64_t uTimestampUs = 0;
};

static constexpr u32_t kPointerPressCapacity = 32u;

u32_t GetPointerPressCount() const { return m_uPointerPressCount; }
bool_t TryGetPointerPressEvent(u32_t iIndex, PointerPressEvent& outEvent) const;
u8_t GetKeyPressCount(uint8 vKey) const { return m_KeyPressCounts[vKey]; }
u8_t GetKeyReleaseCount(uint8 vKey) const { return m_KeyReleaseCounts[vKey]; }
```

기존 코드:

```cpp
void OnKeyDown(uint8 vKey) { m_Keys[vKey] = true; }
void OnKeyUp(uint8 vKey) { m_Keys[vKey] = false; }
void OnLButtonDown() { m_bLButton = true; }
void OnLButtonUp() { m_bLButton = false; }
void OnRButtonDown() { m_bRButton = true; }
void OnRButtonUp() { m_bRButton = false; }
```

아래로 교체:

```cpp
void OnKeyDown(uint8 vKey);
void OnKeyUp(uint8 vKey);
void OnLButtonDown(int32 x, int32 y);
void OnLButtonUp() { m_bLButton = false; }
void OnRButtonDown(int32 x, int32 y);
void OnRButtonUp() { m_bRButton = false; }
```

private 영역에 아래를 추가:

```cpp
void PushPointerPress(ePointerButton eButton, int32 x, int32 y);

u8_t m_KeyPressCounts[256] = {};
u8_t m_KeyReleaseCounts[256] = {};
PointerPressEvent m_PointerPressEvents[kPointerPressCapacity] = {};
u32_t m_uPointerPressCount = 0u;
u32_t m_uDroppedPointerPressCount = 0u;
```

`EndFrame()`의 마지막에 아래를 추가:

```cpp
std::memset(m_KeyPressCounts, 0, sizeof(m_KeyPressCounts));
std::memset(m_KeyReleaseCounts, 0, sizeof(m_KeyReleaseCounts));
m_uPointerPressCount = 0u;
```

### 1-4. C:/Users/user/Desktop/Winters/Engine/Private/Platform/CInput.cpp

기존 edge query는 event count를 먼저 보고 기존 current/previous 비교를 fallback으로 남긴다.

기존 코드:

```cpp
bool CInput::IsKeyPressed(uint8 vKey) const
{
    return m_Keys[vKey] && !m_pPrevKeys[vKey];
}

bool CInput::IsKeyReleased(uint8 vKey) const
{
    return !m_Keys[vKey] && m_pPrevKeys[vKey];
}
```

아래로 교체:

```cpp
bool CInput::IsKeyPressed(uint8 vKey) const
{
    return m_KeyPressCounts[vKey] > 0u ||
        (m_Keys[vKey] && !m_pPrevKeys[vKey]);
}

bool CInput::IsKeyReleased(uint8 vKey) const
{
    return m_KeyReleaseCounts[vKey] > 0u ||
        (!m_Keys[vKey] && m_pPrevKeys[vKey]);
}
```

`IsLButtonPressed`/`IsRButtonPressed`도 해당 button의 `PointerPressEvent` 존재 여부를 먼저 확인한다. 파일 하단에 실제 event 기록 함수를 추가한다.

아래에 추가:

```cpp
void CInput::OnKeyDown(uint8 vKey)
{
    if (!m_Keys[vKey] && m_KeyPressCounts[vKey] < 0xffu)
        ++m_KeyPressCounts[vKey];
    m_Keys[vKey] = true;
}

void CInput::OnKeyUp(uint8 vKey)
{
    if (m_Keys[vKey] && m_KeyReleaseCounts[vKey] < 0xffu)
        ++m_KeyReleaseCounts[vKey];
    m_Keys[vKey] = false;
}

void CInput::OnLButtonDown(int32 x, int32 y)
{
    m_bLButton = true;
    PushPointerPress(ePointerButton::Left, x, y);
}

void CInput::OnRButtonDown(int32 x, int32 y)
{
    m_bRButton = true;
    PushPointerPress(ePointerButton::Right, x, y);
}
```

`PushPointerPress`의 timestamp는 `std::chrono::steady_clock` microseconds를 사용한다. overflow는 overwrite하지 않고 drop counter를 올려 `Input::CollapsedEdgeCount`에 노출한다.

### 1-5. C:/Users/user/Desktop/Winters/Engine/Private/Platform/CWin32Window.cpp

ImGui가 message를 처리했다는 이유로 CInput 상태 갱신까지 건너뛰면 button-up 유실과 stuck input이 생길 수 있다.

기존 코드:

```cpp
if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;
```

아래로 교체:

```cpp
const bool_t bImGuiHandled =
    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
```

기존 코드:

```cpp
input.OnLButtonDown();
```

아래로 교체:

```cpp
input.OnLButtonDown(
    static_cast<int32>(GET_X_LPARAM(lParam)),
    static_cast<int32>(GET_Y_LPARAM(lParam)));
```

기존 코드:

```cpp
input.OnRButtonDown();
```

아래로 교체:

```cpp
input.OnRButtonDown(
    static_cast<int32>(GET_X_LPARAM(lParam)),
    static_cast<int32>(GET_Y_LPARAM(lParam)));
```

WndProc 마지막은 `bImGuiHandled`이면 `0`, 아니면 `DefWindowProcW`를 반환한다. gameplay 소비 여부는 기존 `WantCaptureMouse/Keyboard`가 결정하고, 물리 입력 상태 자체는 항상 정상적으로 닫는다.

### 1-6. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

한 frame에서 hover/attack/skill/move가 서로 다른 camera pose와 cursor ray를 계산하지 않도록 frame intent를 한 번만 만든다.

기존 include 영역에 아래를 추가:

```cpp
#include "Core/CInput.h"
#include "Network/Client/ClientInputBuffer.h"
```

`UpdateTargeting` 선언 위에 아래를 추가:

```cpp
struct FramePointerIntent
{
    CInput::MouseRay Ray{};
    Vec3 vMapSurfacePos{};
    int32 iScreenX = 0;
    int32 iScreenY = 0;
    u64_t uInputTimestampUs = 0u;
    u32_t uRightPressCount = 0u;
    u32_t uLeftPressCount = 0u;
    bool_t bHasMapSurfacePos = false;
};

void CaptureFramePointerIntent();
Vec3 ResolveMapSurfacePosFromRay(const CInput::MouseRay& ray) const;
```

기존 선언:

```cpp
void UpdateTargeting();
void UpdateCombatInput(bool& outSkipGroundMove);
```

아래로 교체:

```cpp
void UpdateTargeting(const FramePointerIntent& intent);
void UpdateCombatInput(const FramePointerIntent& intent, bool& outSkipGroundMove);
```

scene member에 아래를 추가:

```cpp
FramePointerIntent m_FramePointerIntent{};
```

기존 `NetworkMovePrediction` deque는 1-11의 `CClientInputBuffer` 및 단일 local presentation predictor로 교체한 뒤 삭제한다. 두 개의 pending-input 저장소를 유지하지 않는다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

snapshot/presentation transform을 적용한 뒤 camera를 갱신하고, 그 camera pose로 frame intent를 캡처한 후 모든 combat input이 소비하게 한다.

기존 코드:

```cpp
bool bSkipGroundMove = false;
if (!m_bReplayPlaybackMode && !bPlayerDead)
{
    UpdateTargeting();
    UpdateCombatInput(bSkipGroundMove);
}
```

아래로 교체:

```cpp
if (m_pCamera)
    m_pCamera->Update(dt, CInput::Get());

CaptureFramePointerIntent();

bool bSkipGroundMove = false;
if (!m_bReplayPlaybackMode && !bPlayerDead)
{
    UpdateTargeting(m_FramePointerIntent);
    UpdateCombatInput(m_FramePointerIntent, bSkipGroundMove);
}
```

삭제할 코드:

```cpp
if (m_pCamera)
    m_pCamera->Update(dt, CInput::Get());
```

삭제 위치는 현재 `UpdateChampionStateTimers(dt)` 이후, `UpdatePlayerControl` 직전의 두 줄이다. 카메라 갱신은 한 frame에 한 번만 실행한다.

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameInput.cpp

frame 수 기반 기본 공격 retry는 60 FPS에서 약 100 ms, 300 FPS에서 약 20 ms로 달라진다. 현재 intent는 실제 `SendBasicAttack` 뒤 즉시 clear되므로 6-frame throttle은 불필요하다.

삭제할 코드:

```cpp
u32_t s_uNetworkAttackCommandFrame = 0u;
constexpr u32_t kNetworkAttackCommandIntervalFrames = 6u;
```

`ClearNetworkAttackIntent`에서 삭제할 코드:

```cpp
s_uNetworkAttackCommandFrame = 0u;
```

target 설정에서 삭제할 코드:

```cpp
s_uNetworkAttackCommandFrame = kNetworkAttackCommandIntervalFrames;
```

`DriveNetworkAttackIntent`에서 삭제할 코드:

```cpp
if (s_uNetworkAttackCommandFrame < kNetworkAttackCommandIntervalFrames)
{
    ++s_uNetworkAttackCommandFrame;
    return;
}
s_uNetworkAttackCommandFrame = 0u;
```

`UpdateTargeting`, `UpdateCombatInput`, ping/ward/attack cursor 위치는 `ResolveMouseMapSurfacePos()`를 다시 호출하지 않고 전달받은 `FramePointerIntent`의 ray/ground를 사용한다. 공격 대상 흡착 반경 `+0.85`는 구조 수정 후 Feel Lab에서 별도 측정하며 이 단계에서 임의 변경하지 않는다.

### 1-9. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

send 실패를 성공 sequence처럼 반환하지 않도록 내부 API를 bool로 바꾼다.

기존 코드:

```cpp
void SendSingle(CClientNetwork& net, GameCommandWire& wire);
```

아래로 교체:

```cpp
bool_t SendSingle(CClientNetwork& net, GameCommandWire& wire);
```

### 1-10. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

`m_onCommandSent`는 serialize 성공이 아니라 실제 socket send 성공 뒤에만 호출한다.

기존 코드:

```cpp
void CCommandSerializer::SendSingle(CClientNetwork& net, GameCommandWire& wire)
{
    std::vector<GameCommandWire> wires;
    wires.push_back(wire);

    auto payload = BuildCommandBatch(wires);
    if (payload.empty())
        return;

    if (m_onCommandSent)
        m_onCommandSent(wire);
```

아래로 교체:

```cpp
bool_t CCommandSerializer::SendSingle(CClientNetwork& net, GameCommandWire& wire)
{
    std::vector<GameCommandWire> wires;
    wires.push_back(wire);

    auto payload = BuildCommandBatch(wires);
    if (payload.empty())
        return false;
```

기존 코드:

```cpp
net.Send(std::move(packet));
}
```

아래로 교체:

```cpp
const bool_t bSent = net.Send(std::move(packet));
if (bSent && m_onCommandSent)
    m_onCommandSent(wire);

WINTERS_PROFILE_COUNT("Command::SendFailure", bSent ? 0u : 1u);
return bSent;
}
```

`SendMove`와 `SendBasicAttack`의 마지막은 아래로 교체한다.

```cpp
return SendSingle(net, wire) ? wire.sequenceNum : 0u;
```

void 반환 serializer는 `(void)SendSingle(net, wire);`로 명시한다.

### 1-11. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/ClientInputBuffer.h

이미 project에 등록됐지만 사용되지 않는 이 buffer를 유일한 unconfirmed command 저장소로 사용한다. 별도의 scene deque를 추가하지 않는다.

기존 코드:

```cpp
void Push(const GameCommandWire& wire);
void DropAcked(u32_t ackedSeq);
void ForEachAfter(u32_t ackedSeq, const std::function<void(const GameCommandWire&)>& fn) const;
```

아래로 교체:

```cpp
struct PendingCommand
{
    GameCommandWire Wire{};
    u64_t uSentAtUs = 0u;
    bool_t bAccepted = false;
};

void Push(const GameCommandWire& wire, u64_t uSentAtUs);
void ConfirmAccepted(u32_t commandSeq);
void Reject(u32_t commandSeq);
void DropProcessed(u32_t processedSeq);
void ForEachPending(const std::function<void(const PendingCommand&)>& fn) const;
u32_t GetCount() const { return m_count; }
```

내부 array도 `GameCommandWire`가 아니라 `PendingCommand`를 저장한다. `lastAckedCommandSeq`는 transport processed watermark로만 쓰고, prediction 성공 확정은 1-12의 결과 event를 사용한다.

### 1-12. C:/Users/user/Desktop/Winters/Shared/Schemas/Event.fbs

현재 snapshot ack는 executor 성공이 아니라 server drain 완료를 뜻한다. accepted/rejected를 별도 event로 복제한다.

기존 코드:

```fbs
EffectTrigger = 19
```

아래로 교체:

```fbs
EffectTrigger = 19,
CommandResult = 20
```

`EffectTriggerEvent` 아래에 추가:

```fbs
enum CommandResultCode : ushort {
    Accepted = 0,
    RejectedInvalidState = 1,
    RejectedCooldown = 2,
    RejectedTarget = 3,
    RejectedRange = 4,
    RejectedNavigation = 5,
    ReplacedByNewerMove = 6,
    RejectedTransport = 7
}

table CommandResultEvent {
    commandSeq:uint;
    commandKind:ubyte;
    result:CommandResultCode;
    executeTick:ulong;
    resolvedX:float;
    resolvedY:float;
    resolvedZ:float;
}
```

`EventPacket` 마지막에 추가:

```fbs
commandResult:CommandResultEvent;
```

generated C++/Go 파일은 직접 편집하지 않고 schema generator로 재생성한다.

### 1-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

executor가 `void`로 실패를 삼키지 않도록 결과를 typed contract로 반환한다.

`GameCommand` 아래에 추가:

```cpp
enum class eCommandResultCode : u16_t
{
    Accepted = 0,
    RejectedInvalidState,
    RejectedCooldown,
    RejectedTarget,
    RejectedRange,
    RejectedNavigation,
    ReplacedByNewerMove,
    RejectedTransport,
};

struct CommandExecutionResult
{
    eCommandResultCode eCode = eCommandResultCode::RejectedInvalidState;
    Vec3 vResolvedPosition{};

    bool_t IsAccepted() const
    {
        return eCode == eCommandResultCode::Accepted;
    }
};
```

기존 virtual 선언:

```cpp
virtual void ExecuteCommand(CWorld& world, const TickContext& tc,
    const GameCommand& cmd) = 0;
```

아래로 교체:

```cpp
virtual CommandExecutionResult ExecuteCommand(
    CWorld& world,
    const TickContext& tc,
    const GameCommand& cmd) = 0;
```

모든 `return;`을 무조건 Accepted로 바꾸지 않는다. 각 validation branch가 실제 reject reason을 반환하도록 `CommandExecutor.cpp`의 Move/BasicAttack/CastSkill부터 순서대로 변환한다.

### 1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

command 실행 결과를 같은 tick의 replicated event queue에 넣는다.

기존 코드:

```cpp
if (!TryHandlePracticeControl(tc, cmd))
    m_pExecutor->ExecuteCommand(m_world, tc, cmd);
```

아래로 교체:

```cpp
if (!TryHandlePracticeControl(tc, cmd))
{
    const CommandExecutionResult result =
        m_pExecutor->ExecuteCommand(m_world, tc, cmd);
    QueueCommandResultEvent(m_world, tc, cmd, result);
}
```

`QueueCommandResultEvent`는 issuer session에만 전송해야 하므로 기존 global gameplay feedback과 섞지 않는다. 정확한 per-session event routing은 `CReplicationEmitter`의 현재 broadcast API가 session filter를 지원하는지 확인 후 구현한다. 이 한 항목은 `CONFIRM_NEEDED`: global broadcast로 임시 구현하지 않는다.

### 1-15. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

로컬과 원격 actor가 같은 55 ms endpoint chase를 쓰지 않도록 presentation state를 분리한다.

기존 `NetworkSnapshotInterpState`를 아래 구조로 교체:

```cpp
struct NetworkActorSample
{
    Vec3 vPosition{};
    Vec3 vRotation{};
    u64_t uServerTick = 0u;
    u64_t uServerTimeMs = 0u;
};

struct RemotePresentationState
{
    static constexpr u32_t kSampleCapacity = 4u;
    NetworkActorSample Samples[kSampleCapacity]{};
    u32_t uSampleCount = 0u;
    Vec3 vPresentedPosition{};
    Vec3 vPresentedRotation{};
    bool_t bInitialized = false;
};

struct LocalMovePresentationState
{
    Vec3 vAuthoritativePosition{};
    Vec3 vPresentedPosition{};
    Vec3 vCorrectionOffset{};
    Vec3 vPredictedVelocity{};
    u64_t uAuthoritativeServerTick = 0u;
    u32_t uLastConfirmedCommandSeq = 0u;
    bool_t bPredictionActive = false;
};

std::unordered_map<EntityID, RemotePresentationState> m_RemotePresentationStates{};
LocalMovePresentationState m_LocalMovePresentation{};
CClientInputBuffer m_ClientInputBuffer{};

f32_t m_fRemoteInterpolationDelaySec = 2.f / 30.f;
f32_t m_fLocalCorrectionHalfLifeSec = 0.045f;
```

선언을 아래처럼 교체한다.

```cpp
void QueueAuthoritativePresentationSamples(u64_t serverTick, u64_t serverTimeMs);
void ApplyRemoteActorPresentation(f32_t dt);
void RebaseLocalMovePrediction(
    u64_t serverTick,
    u32_t processedCommandSeq,
    const Vec3& authoritativePosition);
void UpdateLocalMovePresentation(f32_t dt);
void ApplyCommandResult(u32_t commandSeq, eCommandResultCode result,
    const Vec3& resolvedPosition, u64_t executeTick);
```

### 1-16. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

현재 `CaptureNetworkActorInterpolationStarts`, `BeginNetworkActorInterpolationForSnapshot`, `ApplyNetworkActorInterpolation`의 55 ms SmoothStep endpoint chase를 삭제하고 다음 규칙으로 교체한다.

```text
local player
1. snapshot Transform은 authoritative base로 저장한다.
2. accepted 결과가 확인된 command까지 input buffer에서 제거한다.
3. 아직 처리되지 않은 최신 Move intent를 client nav로 다시 resolve한다.
4. render dt에서 presentation position만 전진시킨다.
5. authoritative와 predicted 차이는 correction offset으로 만들고 half-life 45 ms로 감쇠한다.
6. 0.25 world unit 이상 또는 explicit teleport는 prediction을 중단하고 snap한다.

remote champion/minion
1. snapshot position/rotation을 serverTick/serverTimeMs sample로 최대 4개 보관한다.
2. renderServerTime = estimatedServerTime - adaptiveDelay를 계산한다.
3. 앞뒤 sample이 있으면 linear 또는 velocity-aware Hermite로 보간한다.
4. sample이 하나뿐이면 최대 100 ms까지만 제한 extrapolation하고 그 뒤 hold한다.
5. local player는 이 경로에 절대 넣지 않는다.
```

`OnAuthoritativeSnapshot`의 기존 `(void)serverTimeMs; (void)localNetId;`를 삭제하고 실제 sample clock 및 local rebase에 사용한다.

아래 profiler counter를 추가:

```cpp
WINTERS_PROFILE_COUNT("Net::SnapshotInterarrivalUs", snapshotInterarrivalUs);
WINTERS_PROFILE_COUNT("Prediction::CorrectionDistanceMm", correctionDistanceMm);
WINTERS_PROFILE_COUNT("Prediction::OldestPendingAgeUs", oldestPendingAgeUs);
WINTERS_PROFILE_COUNT("Interp::TargetErrorMm", remoteTargetErrorMm);
WINTERS_PROFILE_COUNT("Interp::ResetBeforeComplete", resetBeforeCompleteCount);
```

보간 delay의 초기값은 66.7 ms이나 고정 정답으로 박제하지 않는다. 최근 32개 snapshot interarrival p95를 기준으로 2~3 sample 범위에서만 조정한다.

### 1-17. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/ClientNetwork.cpp

TCP gameplay 경로가 유지되는 동안 stale snapshot backlog가 화면 지연으로 변하지 않도록 pump 시 최신 snapshot 하나만 적용하고 Event/Hello 순서는 보존한다.

기존 코드:

```cpp
for (auto& [type, seq, payload] : drained)
    m_callback(type, seq, payload.data(), static_cast<u32_t>(payload.size()));
```

아래로 교체:

```cpp
size_t iNewestSnapshot = drained.size();
for (size_t i = 0; i < drained.size(); ++i)
{
    if (std::get<0>(drained[i]) == ePacketType::Snapshot)
        iNewestSnapshot = i;
}

for (size_t i = 0; i < drained.size(); ++i)
{
    auto& [type, seq, payload] = drained[i];
    if (type == ePacketType::Snapshot && i != iNewestSnapshot)
        continue;
    m_callback(type, seq, payload.data(), static_cast<u32_t>(payload.size()));
}
```

`Net::RxPendingFrames`, `Net::RxPendingBytes`, `Net::RxSnapshotsPerPump`, `Net::RxCoalescedSnapshots`를 기록한다. pending Event가 4096개 또는 8 MiB를 넘으면 조용히 버리지 말고 bounded diagnostic 후 연결을 종료해 desync를 드러낸다. gameplay UDP 전환은 기존 UDP migration 문서의 한 경로로 진행하고 새로운 세 번째 transport를 만들지 않는다.

### 1-18. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

기본 공격 및 스킬의 gameplay timing을 애니메이션 길이 하나로 추정하지 않도록 stage에 impact/recovery 의미를 추가한다.

기존 코드:

```cpp
struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
};
```

아래로 교체:

```cpp
struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
    f32_t impactTimeSec = -1.f;
    f32_t recoveryEndSec = -1.f;
};
```

`-1`은 legacy fallback을 뜻하고 `0`은 즉시 impact라는 명시적인 값이다. gameplay pack의 timing과 client animation frame은 별도 소유한다.

### 1-19. C:/Users/user/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py

기존 코드:

```python
STAGE_FIELDS = {
    "lockDurationSec": 0.6,
}
```

아래로 교체:

```python
STAGE_FIELDS = {
    "lockDurationSec": 0.6,
    "impactTimeSec": -1.0,
    "recoveryEndSec": -1.0,
}
```

semantic validation을 추가한다.

```python
if stage["impactTimeSec"] >= 0.0 and stage["recoveryEndSec"] >= 0.0:
    if stage["impactTimeSec"] > stage["recoveryEndSec"]:
        fail(f"{path}.impactTimeSec must be <= recoveryEndSec")
if stage["recoveryEndSec"] >= 0.0 and stage["recoveryEndSec"] > stage["lockDurationSec"]:
    fail(f"{path}.recoveryEndSec must be <= lockDurationSec")
```

generated emitter도 두 필드를 출력한다. 기본 공격부터 authored 값을 채우고 나머지 스킬은 `-1` fallback으로 단계적으로 이관한다.

### 1-20. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.h

기존 코드:

```cpp
struct ChampionSkillTimingDefaults
{
    f32_t lockDurationSec = 0.6f;
};

struct ChampionBasicAttackTimingDefaults
{
    f32_t fWindupSec = 0.25f;
    f32_t fActionDurationSec = 0.75f;
};
```

아래로 교체:

```cpp
struct ChampionSkillTimingDefaults
{
    f32_t lockDurationSec = 0.6f;
    f32_t impactTimeSec = -1.f;
    f32_t recoveryEndSec = -1.f;
};

struct ChampionBasicAttackTimingDefaults
{
    f32_t fWindupSec = 0.25f;
    f32_t fRecoveryEndSec = 0.75f;
    f32_t fActionDurationSec = 0.75f;
};
```

### 1-21. C:/Users/user/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

`BuildBasicAttackTiming`은 authored `impactTimeSec/recoveryEndSec`를 우선하고 legacy 데이터만 35% fallback을 사용한다. fallback 사용 횟수를 `Data::BasicAttackTimingFallback` counter로 기록해 0으로 줄인다.

아래로 교체할 핵심 계산:

```cpp
timing.fActionDurationSec =
    SanitizePositive(skillTiming.lockDurationSec, 0.75f);
timing.fWindupSec = skillTiming.impactTimeSec >= 0.f
    ? std::clamp(skillTiming.impactTimeSec, 0.f, timing.fActionDurationSec)
    : std::clamp(timing.fActionDurationSec * 0.35f,
        0.12f,
        (std::max)(0.05f, timing.fActionDurationSec - 0.03f));
timing.fRecoveryEndSec = skillTiming.recoveryEndSec >= 0.f
    ? std::clamp(skillTiming.recoveryEndSec,
        timing.fWindupSec,
        timing.fActionDurationSec)
    : timing.fActionDurationSec;
```

이를 위해 `ChampionSkillTimingDefaults`에도 두 authored field를 보존한다.

### 1-22. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기본 공격을 `windup -> impact -> recovery`로 분리한다. 현재는 full action duration이 impact tick이고 end tick도 같아 공격 판정이 애니메이션 끝에 발생한다.

기존 코드:

```cpp
const u64_t actionTicks =
    ResolveScaledBasicAttackActionTicks(attackActionDurationSec, attackSpeedScale);
```

아래로 교체:

```cpp
const u64_t impactTicks = ResolveScaledBasicAttackActionTicks(
    attackTiming.fWindupSec,
    attackSpeedScale);
const u64_t recoveryEndTicks = ResolveScaledBasicAttackActionTicks(
    attackTiming.fRecoveryEndSec,
    attackSpeedScale);
```

기존 코드:

```cpp
action.uImpactTick = tc.tickIndex + actionTicks;
action.uEndTick = action.uImpactTick;
```

아래로 교체:

```cpp
action.uImpactTick = tc.tickIndex + impactTicks;
action.uEndTick = tc.tickIndex +
    (std::max)(impactTicks, recoveryEndTicks);
```

pre-impact 이동 cancel 시 `CombatActionComponent`만 지우지 말고 basic attack slot cooldown을 0으로 복구하고 `PoseState`/`ActionState`를 새 sequence의 Move 또는 Idle로 전환한다. impact 뒤 이동은 recovery animation을 blend-out하면서 즉시 이동을 허용한다. Kalista `QueueMoveUntilImpact` 정책은 유지한다.

추가 counter:

```cpp
WINTERS_PROFILE_COUNT("Attack::WindupTicks", impactTicks);
WINTERS_PROFILE_COUNT("Attack::RecoveryTicks", recoveryEndTicks - impactTicks);
WINTERS_PROFILE_COUNT("Attack::CanceledBeforeImpact", 1u);
WINTERS_PROFILE_COUNT("Attack::CooldownConsumedOnCancel", 0u);
```

### 1-23. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Move/MoveSystem.cpp

accepted click facing은 초기 6 tick만 보호하고 이후 path tangent가 회전을 소유해야 한다.

기존 코드:

```cpp
outFacingLocked = moveTarget.facingLockTicks > 0;
if (outFacingLocked)
    --moveTarget.facingLockTicks;

// Move path steers position only; the accepted input intent owns facing until the move ends.
outUsedFacingIntent = true;
return facingDirection;
```

아래로 교체:

```cpp
outFacingLocked = moveTarget.facingLockTicks > 0;
if (outFacingLocked)
{
    --moveTarget.facingLockTicks;
    outUsedFacingIntent = true;
    return facingDirection;
}

ClearMoveFacingOverride(moveTarget);
return moveDirection;
```

### 1-24. C:/Users/user/Desktop/Winters/Engine/Public/Resource/Animator.h

즉시 clip 교체 대신 현재 evaluated local pose에서 새 clip으로 crossfade한다. 별도의 두 번째 Animator를 만들지 않는다.

기존 선언 아래에 추가:

```cpp
void PlayAnimation(CAnimation* pAnim, bool_t bLoop,
    f64_t dStartTime, f32_t fPlaySpeed, f32_t fBlendDurationSec);
bool_t IsBlending() const { return m_fBlendDurationSec > 0.f; }
```

private member에 추가:

```cpp
vector<XMFLOAT4X4> m_vecBlendSourceLocalTransforms;
f32_t m_fBlendElapsedSec = 0.f;
f32_t m_fBlendDurationSec = 0.f;
```

기존 overload는 새 overload를 blend `0.f`로 호출해 호환성을 유지한다.

### 1-25. C:/Users/user/Desktop/Winters/Engine/Private/Resource/Animator.cpp

`PlayAnimation(..., fBlendDurationSec)` 진입 시 현재 `m_vecLocalTransforms`를 source pose로 복사한 뒤 새 clip을 시작한다. `Update`에서 새 clip pose를 평가한 후 각 local matrix를 `XMMatrixDecompose`, translation/scale lerp, rotation slerp로 혼합한다. matrix 원소를 직접 lerp하지 않는다.

기존 include 영역에 아래를 추가:

```cpp
#include <algorithm>
```

`m_pCurrentAnim->Evaluate(...)` 바로 아래에 추가할 핵심 코드:

```cpp
if (m_fBlendDurationSec > 0.f &&
    m_vecBlendSourceLocalTransforms.size() == m_vecLocalTransforms.size())
{
    m_fBlendElapsedSec += fDeltaTime;
    const f32_t fBlend = std::clamp(
        m_fBlendElapsedSec / m_fBlendDurationSec,
        0.f,
        1.f);

    for (size_t i = 0; i < m_vecLocalTransforms.size(); ++i)
    {
        XMVECTOR srcScale{}, srcRot{}, srcPos{};
        XMVECTOR dstScale{}, dstRot{}, dstPos{};
        const bool_t bSrcOk = XMMatrixDecompose(
            &srcScale, &srcRot, &srcPos,
            XMLoadFloat4x4(&m_vecBlendSourceLocalTransforms[i]));
        const bool_t bDstOk = XMMatrixDecompose(
            &dstScale, &dstRot, &dstPos,
            XMLoadFloat4x4(&m_vecLocalTransforms[i]));
        if (!bSrcOk || !bDstOk)
            continue;

        const XMMATRIX blended =
            XMMatrixScalingFromVector(XMVectorLerp(srcScale, dstScale, fBlend)) *
            XMMatrixRotationQuaternion(XMQuaternionSlerp(srcRot, dstRot, fBlend)) *
            XMMatrixTranslationFromVector(XMVectorLerp(srcPos, dstPos, fBlend));
        XMStoreFloat4x4(&m_vecLocalTransforms[i], blended);
    }

    if (fBlend >= 1.f)
    {
        m_fBlendElapsedSec = 0.f;
        m_fBlendDurationSec = 0.f;
        m_vecBlendSourceLocalTransforms.clear();
    }
}
```

reverse 재생의 frame event는 현재 `HasFramePassed`가 정방향만 처리하므로 `HasFrameCrossed(frame, previousFrame, direction)`으로 교체한다. 역재생이면 `current <= frame && frame < previous`를 사용한다.

### 1-26. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/ModelRenderer.h

기존 signature의 마지막에 default blend 인자를 추가한다.

기존 코드:

```cpp
bool PlayAnimationByNameAdvanced(const std::string& strKeyword,
    bool bLoop,
    bool_t bReverse,
    f32_t fPlaySpeed = 1.f);
```

아래로 교체:

```cpp
bool PlayAnimationByNameAdvanced(const std::string& strKeyword,
    bool bLoop,
    bool_t bReverse,
    f32_t fPlaySpeed = 1.f,
    f32_t fBlendDurationSec = 0.f);
```

### 1-27. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

기존 호출:

```cpp
m_pImpl->pInstanceAnimator->PlayAnimation(pAnim, bLoop, startTime, speed);
```

아래로 교체:

```cpp
m_pImpl->pInstanceAnimator->PlayAnimation(
    pAnim,
    bLoop,
    startTime,
    speed,
    fBlendDurationSec);
```

`animation switch`, `same clip restart`, `blend started/completed` counter를 추가한다. 같은 clip/같은 loop/speed 요청은 explicit restart flag가 없는 한 무시해 foot sliding을 줄인다.

### 1-28. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`ApplyActionStart`는 replicated action data만 갱신하고 직접 animation을 재생하지 않는다. action animation의 단일 소유자는 `UpdateNetworkChampionLocomotion`의 presentation controller다.

기존 코드:

```cpp
if (!bShouldPlay)
    return;
m_lastActionSeq[ev->netId()] = ev->actionSeq();
PlayReplicatedActionVisual(world, entity, ev->actionId(), actionStage);
```

아래로 교체:

```cpp
if (!bShouldPlay)
    return;
m_lastActionSeq[ev->netId()] = ev->actionSeq();
```

controller는 같은 frame 후반에 `ReplicatedActionComponent.sequence`를 보고 one-shot/loop 모두 한 번만 시작한다. 기존 effect/projectile/kill-feed dedupe key는 유지한다.

`OnEvent`에서 `EffectTrigger` 호출은 packet tick을 함께 전달한다.

기존 코드:

```cpp
ApplyEffectTrigger(world, entityMap, packet->effect());
```

아래로 교체:

```cpp
ApplyEffectTrigger(
    world,
    entityMap,
    packet->effect(),
    packet->serverTick(),
    m_uPresentationServerTick);
```

`m_uPresentationServerTick`은 Scene이 최신 authoritative snapshot tick으로 갱신한다.

### 1-28a. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

`ApplyEffectTrigger`의 server-time 계약과 Scene이 갱신할 presentation tick을 헤더에 명시한다.

public 영역의 `SetFxMeshRenderer` 아래에 추가:

```cpp
void SetPresentationServerTick(u64_t serverTick)
{
    m_uPresentationServerTick = serverTick;
}
```

기존 선언:

```cpp
void ApplyEffectTrigger(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::EffectTriggerEvent* ev);
```

아래로 교체:

```cpp
void ApplyEffectTrigger(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::EffectTriggerEvent* ev,
    u64_t packetServerTick,
    u64_t presentationServerTick);
```

private member에 아래를 추가:

```cpp
u64_t m_uPresentationServerTick = 0u;
```

### 1-29. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

action controller에서 다음 blend 기본값을 사용하되 1-30의 visual data가 있으면 authored 값을 우선한다.

`SetOnAuthoritativeSnapshot` callback의 `OnAuthoritativeSnapshot(...)` 호출 바로 위에 추가:

```cpp
if (m_pEventApplier)
    m_pEventApplier->SetPresentationServerTick(serverTick);
```

```cpp
constexpr f32_t kDefaultLocomotionBlendSec = 0.08f;
constexpr f32_t kDefaultActionBlendInSec = 0.04f;
constexpr f32_t kDefaultActionBlendOutSec = 0.08f;
```

- Idle <-> Run: 80 ms crossfade.
- Locomotion -> attack/skill: 40 ms. 단, 즉시 반응이 중요한 dash/CC interrupt는 0~30 ms authored 값.
- Action -> locomotion: 80 ms.
- Death, teleport, form mesh replacement: 0 ms hard transition.
- 별도 end-transition clip이 있으면 clip 자체도 crossfade로 진입/이탈한다.

animation duration timer는 client가 임의로 만든 legacy 시간보다 replicated `startTick/lockEndTick` 및 authored visual playback speed를 기준으로 계산한다.

### 1-30. C:/Users/user/Desktop/Winters/Client/Private/Data/LoLVisualDefinitionPack.h

기존 코드:

```cpp
struct SkillVisualStageDef
{
    f32_t animationPlaybackSpeed = 1.f;
    f32_t castFrame = 0.f;
    f32_t recoveryFrame = 0.f;
};
```

아래로 교체:

```cpp
struct SkillVisualStageDef
{
    f32_t animationPlaybackSpeed = 1.f;
    f32_t castFrame = 0.f;
    f32_t recoveryFrame = 0.f;
    f32_t blendInSec = 0.04f;
    f32_t blendOutSec = 0.08f;
    f32_t cameraImpulse = 0.f;
};
```

이 값은 presentation 전용이다. damage, range, CC, gameplay lock duration은 이 pack에 넣지 않는다.

### 1-31. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/FxCuePlayer.h

늦게 도착한 FX를 올바른 age에서 시작하도록 context를 확장한다.

`FxCueContext`에 추가:

```cpp
f32_t fInitialAgeSec = 0.f;
u64_t uSourceServerTick = 0u;
```

### 1-32. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxCuePlayer.cpp

billboard/beam/ribbon/mesh builder에서 공통으로 아래를 설정한다.

```cpp
const f32_t fMaxElapsed =
    (std::max)(0.f, component.fStartDelay + component.fLifetime);
component.fElapsed = std::clamp(ctx.fInitialAgeSec, 0.f, fMaxElapsed);
```

`ctx.fInitialAgeSec >= startDelay + lifetime`인 emitter는 생성하지 않는다. `FX::LateSkipped`, `FX::InitialAgeMs`, `FX::SpawnUs`, `FX::FirstUseLoadUs`를 기록한다.

`FindCue`의 최초 재생 시 전체 directory scan은 Release 정상 경로에서 금지한다. `Loader.cpp`의 loading barrier가 WFX directory를 preload하고, runtime miss는 bounded log 후 실패한다. Debug Effect Tool에서만 explicit reload를 허용한다.

### 1-33. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

`ApplyEffectTrigger`에서 초기 age를 계산한다.

```cpp
const u64_t sourceTick = ev->startTick() != 0u
    ? ev->startTick()
    : packetServerTick;
const u64_t presentationTick =
    (std::max)(packetServerTick, presentationServerTick);
const f32_t initialAgeSec = presentationTick > sourceTick
    ? static_cast<f32_t>(presentationTick - sourceTick) / 30.f
    : 0.f;

FxCueContext fx{};
fx.fInitialAgeSec = initialAgeSec;
fx.uSourceServerTick = sourceTick;
```

VisualHook가 effect를 처리했다면 fallback cue를 추가로 재생하지 않는 현재 precedence를 유지한다. offline local cast frame의 GameplayHook/VisualHook/legacy SkillHook는 `VisualHook handled -> legacy visual fallback 금지`로 배타화해 연습 모드에서도 중복 FX/사운드를 막는다.

### 1-34. C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h

시각과 타격음을 같은 cue timeline에서 authoring할 수 있도록 cue-level sound metadata를 추가한다.

`FxAsset`에 추가:

```cpp
std::string strSoundKey;
f32_t fSoundVolume = 1.f;
f32_t fSoundStartSec = 0.f;
f32_t fSoundMaxLateSec = 0.12f;
```

WFX loader/writer/tool도 `sound`, `sound_volume`, `sound_start`, `sound_max_late`를 읽고 저장한다. `CFxCuePlayer`는 `initialAge`가 sound window 안일 때만 `CGameInstance::PlayEffect`를 한 번 호출한다. 동일 cue dedupe key를 공유해 영상과 음향이 따로 중복되지 않게 한다.

### 1-35. C:/Users/user/Desktop/Winters/Client/Public/DynamicCamera.h

base camera와 correction/shake offset을 분리한다.

private member에 추가:

```cpp
Vec3 m_vPresentationOffset{};
Vec3 m_vShakeOffset{};
Vec3 m_vEdgeScrollVelocity{};
f32_t m_fShakePhase = 0.f;
f32_t m_fShakeTrauma = 0.f;
f32_t m_fEdgeScrollAcceleration = 90.f;
f32_t m_fEdgeScrollDeceleration = 120.f;
```

`StartShake`는 duration/intensity를 eye에 직접 누적하지 않고 trauma impulse만 추가한다.

### 1-36. C:/Users/user/Desktop/Winters/Client/Private/DynamicCamera.cpp

기존 frame-dependent random eye mutation을 삭제한다.

삭제할 코드 범위:

```cpp
if (m_fShakeTimer < m_fShakeDuration)
{
    m_fShakeTimer += fTimeDelta;
    f32_t fStrength = m_fShakeIntensity
                    * (1.f - m_fShakeTimer / m_fShakeDuration);
    m_vEye.x += ((rand() % 100) / 100.f - 0.5f) * fStrength;
    m_vEye.y += ((rand() % 100) / 100.f - 0.5f) * fStrength;
    m_vEye.z += ((rand() % 100) / 100.f - 0.5f) * fStrength;
}
```

아래 방향으로 교체한다.

```cpp
m_fShakePhase += fTimeDelta;
m_fShakeTrauma = (std::max)(0.f, m_fShakeTrauma - fTimeDelta * 3.5f);
const f32_t amplitude = m_fShakeTrauma * m_fShakeTrauma;
m_vShakeOffset = {
    std::sinf(m_fShakePhase * 47.f) * amplitude,
    std::sinf(m_fShakePhase * 61.f + 1.7f) * amplitude * 0.35f,
    std::sinf(m_fShakePhase * 53.f + 0.9f) * amplitude
};
```

최종 view 계산에만 `m_vPresentationOffset + m_vShakeOffset`을 더하고 base `m_vEye/m_vAt`은 보존한다. locked follow가 local predicted presentation을 따라갈 때 actor correction과 camera exponential follow를 중복 적용하지 않는다. correction은 camera offset에서 한 번만 감쇠한다.

edge scroll은 18 pixel band에 들어오자마자 38 units/s로 점프하지 않고 band penetration을 SmoothStep으로 바꾸고 acceleration/deceleration으로 velocity를 갱신한다. 미니맵 jump는 사용자 의도인 hard cut으로 유지한다.

### 1-37. C:/Users/user/Desktop/Winters/Client/Private/main.cpp

현재 기본값 `60 FPS + VSync off`는 input quantization과 tearing을 만들 수 있지만, 수치만 즉시 300으로 바꾸지 않는다. 다음 세 실행 profile을 명시적으로 유지한다.

```text
ReferenceStable: --fps=<display refresh> --no-vsync
LatencyLab:      --uncapped --no-vsync
Presentation:    --uncapped --vsync
```

기본 shipping profile 변경은 `CONFIRM_NEEDED`: 실제 monitor refresh/VRR 탐지와 DXGI present queue 측정 없이 60, 144, 300 중 하나를 하드코딩하지 않는다.

### 1-38. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.cpp

현재 single-buffer DISCARD swap chain과 `Present(0/1)`에서 다음을 계측한다.

```cpp
WINTERS_PROFILE_COUNT("Present::IntervalUs", presentIntervalUs);
WINTERS_PROFILE_COUNT("Present::BlockedUs", presentBlockedUs);
WINTERS_PROFILE_COUNT("Present::SyncInterval", m_bVSync ? 1u : 0u);
```

flip-discard, tearing flag, waitable swap chain, maximum frame latency 도입은 `CONFIRM_NEEDED`: DX11 resize/backbuffer binding 경로 전체와 fullscreen/VRR 지원을 먼저 확인한다. 기존 renderer를 우회하는 두 번째 swap chain은 만들지 않는다.

### 1-39. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/NetworkEventTrace.h

Practice Tool 안에서 Feel Budget을 관찰할 수 있도록 trace kind와 timestamp를 확장한다.

`eTraceKind`에 추가:

```cpp
InputIntent,
CommandSent,
CommandAccepted,
CommandRejected,
FirstPresentedMotion,
PredictionCorrection,
AnimationTransition,
EffectPresented,
```

`Entry`에 추가:

```cpp
u64_t localTimestampUs = 0u;
u64_t sourceTimestampUs = 0u;
f32_t errorDistance = 0.f;
```

### 1-40. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

별도 scene을 만들지 않고 기존 Practice Tool에 `Feel Lab` tab을 추가한다. gameplay truth를 바꾸는 기존 server practice command와 client presentation tuning을 UI에서 명확히 분리한다.

추가할 UI 계약:

```text
Feel Lab / Observe
- FPS, frame p50/p95/p99, present interval, snapshot cadence/jitter
- input -> command, command -> accepted, input -> first motion
- prediction correction p50/p95/max, hard snap count
- actor velocity jerk, camera target error
- animation switch/restart/blend count
- FX first-use load, spawn latency, server age/local age error

Feel Lab / Presentation Scratch
- remote interpolation delay 33.3~100 ms
- local correction half-life 20~100 ms
- locomotion/action blend 0~150 ms
- camera follow/correction, edge acceleration, shake intensity
- Apply Local / Reset / Export JSON

Feel Lab / Scenario
- 90/180 degree move click x100
- S-curve/wall-corner movement x100
- attack -> move cancel before impact x100
- attack -> move after impact x100
- instant/cast/channel/dash/projectile skill suites
- localhost/LAN marker and 60/144/300 FPS marker
```

`Presentation Scratch`는 render/client 값만 바꾸며 damage, cooldown, CC, authoritative movement를 변경하지 않는다. 확정 값은 `ChampionVisualDefs.json` 또는 camera/FX canonical authoring data로 승격하고 scratch JSON을 shipping truth로 사용하지 않는다.

### 1-41. C:/Users/user/Desktop/Winters/Engine/Private/Manager/Profiler/ProfilerOverlay.cpp

기존 JSON 저장에 Feel Budget counter를 그대로 포함하고 capture metadata를 추가한다.

```json
{
  "capture": {
    "scenario": "attack_move_after_impact",
    "connection": "lan",
    "fpsProfile": "144",
    "champion": "Irelia",
    "serverTickHz": 30
  }
}
```

metadata는 Feel Lab에서 설정하고 profiler save가 읽는다. 평균 FPS만으로 pass를 내지 않는다.

## 2. 검증

미검증:
- 이번 session은 구조 감사와 계획 문서 작성만 수행했다.
- Client/Server/Engine 코드는 수정하지 않았고 build/runtime 결과를 주장하지 않는다.
- 현재 worktree의 기존 dirty 변경은 보존했다.

반영 순서와 각 단계의 중단 조건:

1. Authority/correctness gate
   - Direct IP와 shared roster 접속 모두 `m_bNetworkAuthoritativeGameplay == true`.
   - network match에서 client Navigation/LocalUnitAI/Turret/Projectile/BT/MCTS가 등록되지 않음.
   - local/offline에서는 기존 시스템이 정상 등록됨.
   - 실패하면 prediction/animation 튜닝으로 넘어가지 않는다.

2. Timing gate
   - 기본 공격 `startTick < impactTick <= endTick`.
   - pre-impact cancel은 피해/FX 0회, cooldown 소비 0회, 새 Move pose/action sequence 발생.
   - post-impact move는 피해 1회, recovery blend-out, 즉시 이동.
   - Kalista queue 정책 회귀 없음.

3. Input/command gate
   - 한 render frame 안의 down/up 및 우클릭 2회가 event queue에서 보존됨.
   - `Input::CollapsedEdgeCount == 0` 정상 플레이.
   - send 실패 시 sequence 0, prediction/yaw protection 미등록.
   - command ack와 accepted/rejected event를 혼동하지 않음.

4. Prediction/interpolation gate
   - local input-to-first-presented-motion p95 <= 1 render frame.
   - local correction distance p95 < 0.10 world unit, max < 0.25.
   - explicit teleport 외 hard snap 0회.
   - remote actor는 snapshot마다 ease-in을 재시작하지 않고 velocity jerk가 기준 capture보다 감소.
   - stale snapshot coalescing 중 Event/Hello 손실 0회.

5. Animation/FX/camera gate
   - 하나의 action sequence가 animation을 정확히 한 번 시작함.
   - 같은 locomotion clip 불필요 restart 0회.
   - Idle/Run, Action/Locomotion pose discontinuity를 주요 관절 delta로 기록.
   - FX server age와 local age phase error p95 <= 1 render frame.
   - gameplay 중 WFX directory scan 및 first-use model/texture load 0회.
   - locked camera target error p95 < 0.15 world unit.
   - shake 종료 뒤 base eye/at drift 0.

6. Frame pacing gate
   - 60/144/300 profile 각각 frame time p50/p95/p99와 1% low를 저장.
   - 144 Hz target: p99 <= 6.94 ms를 우선 release gate로 사용.
   - 300 FPS는 평균값이 아니라 p95 <= 3.33 ms를 stretch gate로 사용하며 전체 roster/map/minion/UI/FX를 끄지 않는다.
   - Present blocked time, limiter oversleep, snapshot jitter를 함께 비교한다.

필수 자동 검증 명령:

```text
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
git diff --check
MSBuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
UpdateLib.bat
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

필수 수동/캡처 matrix:

```text
Connection: localhost / 동일 Wi-Fi LAN
FPS:        60 / 144 / 300
Actor:      local champion / remote champion / melee minion / ranged minion
Movement:   short click / repeated click / 90 turn / 180 turn / S-curve / wall corner
Combat:     chase / pre-impact cancel / post-impact move / attack-move / staged skill / dash / projectile
Camera:     locked / free edge scroll / space hold / minimap jump / hit shake
```

실제 게임 비교 방법:
- 동일 champion, 이동 속도, 공격 속도, 화면 해상도, camera 조건을 맞춘다.
- 120/240 FPS 영상에서 click 표시, 첫 facing 변화, 첫 위치 변화, windup, impact, projectile/FX, recovery 종료 frame을 기록한다.
- 단일 장면의 “비슷해 보임”이 아니라 각 시나리오 30~100회의 median/p95/variance를 비교한다.
- 먼저 phase와 variance를 맞추고 마지막에 blend duration, camera response, targeting radius, FX scale/brightness를 조정한다.

Profiler 필수 scope/counter:

```text
Input::EventQueueDepth
Input::CollapsedEdgeCount
Input::ClickToCommandUs
Command::SendUs
Command::SendFailure
Command::IngressToExecuteTicks
Command::RejectReason
Prediction::InputToFirstMotionUs
Prediction::SendToAcceptedUs
Prediction::CorrectionDistanceMm
Prediction::OldestPendingAgeUs
Attack::RangeEntryToWindupTicks
Attack::WindupTicks
Attack::RecoveryTicks
Attack::CanceledBeforeImpact
Attack::CooldownConsumedOnCancel
Net::RxPendingFrames
Net::RxPendingBytes
Net::RxCoalescedSnapshots
Net::SnapshotInterarrivalUs
Interp::TargetErrorMm
Interp::ResetBeforeComplete
Animation::SwitchCount
Animation::RestartCount
Animation::BlendCount
FX::SpawnUs
FX::FirstUseLoadUs
FX::InitialAgeMs
Camera::TargetErrorMm
Present::IntervalUs
Present::BlockedUs
Server::TickOverrunUs
```

완료 판정:
- “LoL과 완벽히 동일”이라는 주관 문장으로 완료 처리하지 않는다.
- authority/timing/input/prediction gate가 모두 통과하고, reference capture 대비 주요 latency/phase/variance 값이 목표 범위 안에 들어오며, LAN 두 클라이언트에서 hard snap·이중 animation·중복 FX가 재현되지 않을 때 S010 구현 session을 완료한다.
