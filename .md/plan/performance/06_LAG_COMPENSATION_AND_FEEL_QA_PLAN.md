Session - 랙 보정과 플레이 감각 QA 루프

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:
```cpp
    void EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
        u64_t acceptedTick, u64_t recvTimeMs);
```

아래로 교체:
```cpp
    void EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
        u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs);
```

기존 코드:
```cpp
struct PendingCommand
{
    u32_t sessionId = 0;
    u32_t sequenceNum = 0;
    GameCommandWire wire{};
    u64_t acceptedTick = 0;
    u64_t recvTimeMs = 0;
};
```

아래로 교체:
```cpp
struct PendingCommand
{
    u32_t sessionId = 0;
    u32_t sequenceNum = 0;
    GameCommandWire wire{};
    u64_t acceptedTick = 0;
    u64_t recvTimeMs = 0;
    u64_t clientTimestampMs = 0;
};
```

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

기존 코드:
```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
```

아래에 추가:
```cpp
#include "Security/LagCompensation.h"
```

기존 코드:
```cpp
    const u64_t acceptedTick = GetCurrentTickIndex();
    const u64_t recvMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
```

아래에 추가:
```cpp
    const u64_t clientTimestampMs = batch->clientTimestampMs();
```

기존 코드:
```cpp
        EnqueueCommand(sessionId, wire, acceptedTick, recvMs);
```

아래로 교체:
```cpp
        EnqueueCommand(sessionId, wire, acceptedTick, recvMs, clientTimestampMs);
```

기존 코드:
```cpp
void CGameRoom::EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
    u64_t acceptedTick, u64_t recvTimeMs)
{
    std::lock_guard lk(m_pendingMutex);

    PendingCommand pending{};
    pending.sessionId = sessionId;
    pending.sequenceNum = wire.sequenceNum;
    pending.wire = wire;
    pending.acceptedTick = acceptedTick;
    pending.recvTimeMs = recvTimeMs;
```

아래로 교체:
```cpp
void CGameRoom::EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
    u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs)
{
    std::lock_guard lk(m_pendingMutex);

    PendingCommand pending{};
    pending.sessionId = sessionId;
    pending.sequenceNum = wire.sequenceNum;
    pending.wire = wire;
    pending.acceptedTick = acceptedTick;
    pending.recvTimeMs = recvTimeMs;
    pending.clientTimestampMs = clientTimestampMs;
```

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

기존 코드:
```cpp
        GameCommand cmd = BuildServerCommand(
            pending.wire, pending.sessionId, controlledEntity, m_entityMap);
        cmd.issuedAtTick = tc.tickIndex;

        m_pendingExecCommands.push_back(cmd);
```

아래에 추가:
```text
CONFIRM_NEEDED
GameCommand에는 이미 rewindTicks 필드가 있고, CLagCompensation도 존재한다.
하지만 CommandBatch.clientTimestampMs와 서버 recvTimeMs는 서로 다른 머신의 system_clock일 수 있으므로 곧바로 신뢰하면 안 된다.
구현 세션에서는 먼저 clock offset/RTT 추정 또는 localhost 전용 디버그 모드를 분리한다.

적용 순서:
1. localhost/debug 한정으로 pending.clientTimestampMs와 pending.recvTimeMs 차이를 OutputDebugStringA로 기록한다.
2. clock sync가 없는 원격 세션에서는 cmd.rewindTicks를 0으로 유지한다.
3. clock sync가 확인된 세션에서만 CLagCompensation::kMaxRewindMs 안으로 clamp한 rewindTicks를 넣는다.
4. BasicAttack/CastSkill 판정에서만 lag compensated target state를 조회한다.
5. 이동 명령에는 rewind 판정을 적용하지 않는다.
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

기존 코드:
```cpp
    u64_t rewindTicks = 0;
```

아래에 추가:
```text
코드 변경 없음.
이미 존재하는 rewindTicks를 서버 측 판정 세션에서 사용한다.
```

1-5. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Command.fbs

기존 코드:
```fbs
table CommandBatch {
    commands:[CommandPacket];
    clientTimestampMs:ulong;
}
```

아래에 추가:
```text
코드 변경 없음.
현재 세션에서는 schema를 확장하지 않는다.
clock sync/RTT echo가 필요하다고 확정되면 별도 네트워크 스키마 세션으로 분리한다.
```

2. 검증

```text
빌드:
- git diff --check
- msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64
- schema 변경이 없으므로 flatc 재생성은 하지 않는다.

서버 QA:
- CommandBatch 수신 시 clientTimestampMs가 PendingCommand까지 보존되는지 로그로 확인
- lastAckedCommandSeq가 기존처럼 snapshot에 들어가는지 확인
- cmd.rewindTicks를 0으로 둔 상태에서 기존 판정이 변하지 않는지 확인

랙 보정 QA:
- localhost에서는 timestamp 차이를 관측만 한다.
- 원격/지연 환경에서는 clock sync 전까지 rewindTicks를 신뢰하지 않는다.
- 실제 lag compensation 적용은 BasicAttack/CastSkill 판정에서만 진행한다.

플레이 감각 QA:
- S05 클라 예측과 S06 서버 판정 보정은 별개이다.
- 클라 예측은 입력 반응을 부드럽게 하고, 서버 lag compensation은 판정 공정성을 보정한다.
- 두 기능 모두 서버 권위 결과를 뒤집으면 안 된다.
```
