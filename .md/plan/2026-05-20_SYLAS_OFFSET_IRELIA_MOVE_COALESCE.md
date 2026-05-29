Session - Sylas offset restore and rapid move coalescing

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

`GetDefaultChampionVisualYawOffset` 함수 안에서 아래 기존 코드를 아래로 교체:

기존 코드:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    (void)champion;
    return 0.f;
}
```

아래로 교체:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    if (champion == eChampion::SYLAS)
        return kChampionYawPi;

    return 0.f;
}
```

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

`CGameRoom::EnqueueCommand` 함수 안에서 아래 기존 코드를 아래로 교체:

기존 코드:

```cpp
    PendingCommand pending{};
    pending.sessionId = sessionId;
    pending.sequenceNum = wire.sequenceNum;
    pending.wire = wire;
    pending.acceptedTick = acceptedTick;
    pending.recvTimeMs = recvTimeMs;

    m_pendingCommands.push_back(pending);
```

아래로 교체:

```cpp
    PendingCommand pending{};
    pending.sessionId = sessionId;
    pending.sequenceNum = wire.sequenceNum;
    pending.wire = wire;
    pending.acceptedTick = acceptedTick;
    pending.recvTimeMs = recvTimeMs;

    if (wire.kind == eCommandKind::Move)
    {
        for (PendingCommand& oldPending : m_pendingCommands)
        {
            if (oldPending.sessionId == sessionId &&
                oldPending.wire.kind == eCommandKind::Move)
            {
                static u32_t s_moveCoalesceLogCount = 0;
                if (s_moveCoalesceLogCount < 64u)
                {
                    char msg[192]{};
                    sprintf_s(
                        msg,
                        "[Command] coalesce-move sid=%u oldSeq=%u newSeq=%u\n",
                        sessionId,
                        oldPending.sequenceNum,
                        pending.sequenceNum);
                    OutputServerAITrace(msg);
                    ++s_moveCoalesceLogCount;
                }
                oldPending = pending;
                return;
            }
        }
    }

    m_pendingCommands.push_back(pending);
```

1-3. C:/Users/user/Desktop/Winters/.claude/gotchas.md

아래 기존 코드를 아래로 교체:

기존 코드:

```text
- 2026-05-20 - [Champion body yaw offset] a `+PI` champion visual yaw offset makes movement yaw point opposite the actual XZ direction for body transforms -> keep champion body yaw offsets at `0.f`; reserve mesh/FX-specific 180-degree corrections for the asset or effect path that actually needs them.
```

아래로 교체:

```text
- 2026-05-20 - [Champion body yaw offset] body yaw offset is per champion asset, not a global rule -> keep Irelia at `0.f`, restore Sylas `+PI` when his body mesh faces backward, and reserve projectile/FX 180-degree corrections for their own paths.
- 2026-05-20 - [Move coalescing] rapid right-clicks should not execute every stale Move as a visible steering turn -> while commands are still pending, replace an older pending Move from the same session with the newest Move and let non-move commands remain authoritative.
```

2. 검증

검증 명령:

```text
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
```

수동 확인:

```text
- Sylas 이동이 다시 정방향인지 확인.
- Irelia 빠른 우클릭/더블클릭 때 `[Command] coalesce-move` 로그가 찍히고, 오래된 Move seq가 별도 `[YawTrace][ServerCommand]` 회전을 만들지 않는지 확인.
- 일반 단일 우클릭은 기존처럼 즉시 회전/이동해야 한다.
```
