Session - Irelia W/E·Viego W·Ezreal E의 입력/stage/Data Driven 회귀를 BP로 확정하고 버전별로 복구
좌표: new · 축: C1 · C7
관련: 2026-07-18_DATA_DRIVEN_GAMEPLAY_REGRESSION_AUDIT_RESULT.md

# 1. 결정 기록

1. 문제: Irelia W1은 전송되고 client `currentStage=1`도 설정되지만 W key-up에서 W2가 전송되지 않거나, W2 이후 이동이 즉시 풀리지 않으며 E가 함께 막힌다.
2. 실측 기준: W1 action lock 5.0초, stage window 4.0초, W2 lock 0.4초, `QueueUntilUnlock` 상한 8 tick, server 30 Hz 기준 최대 약 267 ms다.
3. 제약: gameplay truth는 Server/GameSim에 두고 Client는 입력·약한 예측·표현만 담당한다. Bot AI도 직접 상태를 바꾸지 않고 `GameCommand`를 생산한다.
4. 단순 롤백이 실패하는 이유: W press/release 입력은 Data Driven cutover 전에도 같았고, Ezreal E의 client Direction/server Ground 충돌과 charge 상태 부재도 과거부터 존재한다.
5. 확정 메커니즘: cutover 전 `ActionStateComponent`는 표시용 상태였지만, 이후 `lockEndTick/movePolicy/ActionBlocked`가 붙어 W2 유실이 W1 5초 하드 락으로 증폭된다.
6. 확정 메커니즘: `Conditional`은 target resolve 의미이며 hold/release 입력 의미가 아니다. 현재 입력 코드는 `resolvePolicy`를 읽지 않고 schema에는 activation mode가 없다.
7. 확정 메커니즘: Irelia W2는 성공해도 기본 분기상 `QueueUntilUnlock`이므로 “떼자마자 이동” 요구와 충돌한다.
8. 이전 SkillTable이 동작한 이유: Irelia W/E target을 Direction/Ground로 명시하고 hard-coded W release를 썼으며 server action state가 다른 command를 차단하지 않았다.
9. 선택: V0 BP로 input edge와 server stage2 수락을 먼저 분리하고, V1은 release 신뢰성+W2 즉시 unlock, V2는 activation/target schema, V3은 ack/authoritative charge, V4는 전수 회귀 gate 순서로 간다.
10. 비교: champion 이름 분기를 더 추가하는 hotfix는 빠르지만 LeeSin/Zed/Jax 등 13개 2단계 스킬을 계속 깨뜨린다. typed activation data가 장기안이다.
11. 비용: Engine input latch, client/server generated schema, protocol rejection visibility, SimLab/client contract test가 함께 필요하다.
12. 작업 예산: 원인 재현·핵심 복구·테스트 70%, activation/action-policy 데이터 모델과 생성 parity 같은 ceiling work 30%를 예약한다.

# 2. 코드 구현 계획

## 2-0. 먼저 고정할 사실: `Conditional` 두 종류를 섞지 않는다

현재 호출 순서는 다음과 같다.

```text
Windows key message
  -> CInput key edge
  -> Scene_InGame::UpdateCombatInput
  -> DispatchSkillInput(stage)
  -> CSkillRegistry::ResolveGameAtoms
  -> BuildCastCommand(target shape)
  -> SendCastSkill(itemId = stage)
  -> Server IsCommandBlockedByAuthoritativeAction
  -> Server HandleCastSkill
```

따라서 `Scene_InGameInput.cpp`의 W release 분기보다 뒤에 있는 `targetMode/resolvePolicy`는 W key-up 분기 진입 여부에 영향을 줄 수 없다. 필요한 데이터는 아래처럼 서로 다른 타입이다.

| 계약 | 예 | 소비자 |
|---|---|---|
| input activation | `Press`, `PressRecast`, `PressRelease` | Client input, Bot command producer |
| target shape | `Self`, `Unit`, `Ground`, `Direction` | command builder, server validation |
| target resolve policy | `Direct`, `StageDependent`, `ChampionStateDependent` | target resolver |
| action move policy | `Allow`, `QueueUntilUnlock`, `StationaryChannel`, `ForcedMotion` | Server CommandExecutor/MoveSystem |
| charge policy | max hold, release-on-timeout, charge curve | Server champion simulation |

현재 `Conditional`은 두 번째/세 번째 계약에만 속한다. Irelia/Viego W의 hold는 첫 번째와 다섯 번째 계약이다.

### 2-0-1. Data Driven 이전 SkillTable이 동작한 정확한 이유

`git show e6ded62:Client/Private/GameObject/SkillTable.cpp`의 Irelia 정의는 다음 계약이었다.

```text
W stage1 Direction / stage2 Direction / stageCount 2 / stageWindow 4.0
E stage1 GroundTarget / stage2 GroundTarget / stageCount 2
```

현재 JSON처럼 W/E를 `Conditional`로 두지 않았다. 더 중요한 차이는 server action state다. `git show 18ca031^:Shared/GameSim/Components/ActionStateComponent.h`의 전체 gameplay 필드는 아래뿐이었다.

```cpp
struct ActionStateComponent
{
    u16_t actionId = static_cast<u16_t>(eActionStateId::None);
    u64_t startTick = 0;
    u32_t sequence = 0;
    u8_t stage = 1;
};
```

당시 `StartCommandActionState`도 `StartActionState(...)`를 호출해 action 표시만 바꿨다. 이동/다른 cast를 거절할 `lockEndTick`, `movePolicy`, `sourceSlot`, `ActionBlocked`가 없었다.

commit `18ca031`의 Data Driven cutover 뒤 `ActionStateComponent`에 다음 필드가 추가됐다.

```cpp
    u64_t lockEndTick = 0;
    u32_t commandSequence = 0;
    eChampion sourceChampion = eChampion::NONE;
    u8_t sourceSlot = 0;
    eSkillActionMovePolicy movePolicy = eSkillActionMovePolicy::Allow;
    bool_t bHasQueuedMove = false;
```

그리고 `CommandExecutor` 입구에 아래 차단기가 생겼다.

```cpp
        const bool_t bSameSkillStageRelease =
            cmd.kind == eCommandKind::CastSkill &&
            cmd.itemId == 2u &&
            cmd.slot == action.sourceSlot;
        return !bSameSkillStageRelease;
```

W 입력 자체는 `18ca031^:Scene_InGame.cpp`와 현재 `Scene_InGameInput.cpp` 모두 press=stage1, release=stage2였다. 따라서 회귀는 “입력 코드를 새로 바꿔서”가 아니라, 과거 한-frame release 방식 아래에 5초 권위 락을 새로 연결하면서 실패 비용이 커진 것이다.

현재 `SkillDefGameDataAdapter`는 `Conditional`을 Direction shape와 `ChampionStateDependent` policy로 변환하지만 `BuildCastCommand`는 shape만 읽는다. 즉 policy는 hold 분기로 이어지지 않는 dead contract다. 현재 worktree의 `SkillRegistry`가 manual target을 보존하는 것은 target payload 응급 완화일 뿐, W release/ActionState 회귀를 고치지 않는다.

## 2-1. V0 — 코드 수정 전 BP 검증 버전

### 공통 실행 원칙

- Debug Client와 Debug Server의 현재 PDB를 다시 빌드한 뒤 붙는다.
- W를 누르고 있는 동안 매 frame 지나는 `Scene_InGameInput.cpp:500`에 무조건 중단 BP를 걸지 않는다. VS로 포커스가 넘어가면 W key-up이 게임이 아니라 VS로 들어가 관찰자가 증상을 만든다.
- 첫 회차는 조건부 BP 또는 “메시지 출력 후 계속 실행” tracepoint만 쓴다.
- client/server 양쪽에 `sequenceNum`, `slot`, `itemId`, `tickIndex`를 기록해 같은 command를 잇는다.

### H-I0 — 디버거가 key-up을 먹는가

1. `Engine/Private/Platform/CWin32Window.cpp`, `case WM_KEYUP:`의 `input.OnKeyUp(...)`에 조건부 BP를 건다.
2. 조건은 `wParam == 0x57`이다.
3. W를 누르고 뗄 때 여기까지 오면 Windows message는 정상이다.
4. 여기서 멈추지 않으면 게임 창 focus/ImGui 선처리/디버거 focus가 key-up을 잃게 한 것이다.
5. 특히 W를 누른 상태로 다른 BP에 먼저 멈춘 뒤 W를 떼는 실험은 판정 자료로 쓰지 않는다.

### H-I1 — release edge가 한 frame 뒤 지워지는가

`Engine/Private/Platform/CInput.cpp`의 현재 코드는 다음과 같다.

```cpp
bool CInput::IsKeyReleased(uint8 vKey) const
{
    return !m_Keys[vKey] && m_pPrevKeys[vKey];
}
```

`vKey == 0x57` 조건 tracepoint에서 다음을 기록한다.

| `m_Keys[0x57]` | `m_pPrevKeys[0x57]` | 판정 |
|---:|---:|---|
| 0 | 1 | 정상 release edge |
| 1 | 1 | WM_KEYUP/focus 유실 |
| 0 | 0 | release frame은 이미 지나갔고 `EndFrame()`이 edge를 지움 |

동시에 `Client/Private/Scene/Scene_InGameInput.cpp`에서 `bImGuiKbd`와 아래 조기 반환을 기록한다.

- `IsPlayerDead()`
- `UpdatePingWheelInput(...)`
- `!HasPlayerRenderer()`
- `bKalistaCarried`
- `IsPlayerStunned()`
- `ImGui::GetIO().WantCaptureKeyboard`

WM_KEYUP과 정상 edge가 있었는데 `UpdateCombatInput`이 위 분기로 반환하면 H-I1 확정이다.

### H-I2 — W1 local stage 북키핑이 누락되는가

사용자 BP 결과로 현재 재현에서는 반증됐다. 그래도 회귀 테스트에는 남긴다.

`Client/Private/Scene/Scene_InGameLocalSkills.cpp`의 아래 값이 W1 직후 모두 참이어야 한다.

```text
gameData.stage.stageCount == 2
slotState.currentStage == 1
slotState.stageWindow == 4.0f 근처
DispatchSkillInput(...) == true
s_bWReleasePending == true
```

W1이 `return true`이고 `currentStage=1`이면 release 분기 우변은 참이다. 그 뒤 분기 미진입 원인은 `IsKeyReleased('W') == false` 또는 `UpdateCombatInput` 미도달뿐이다.

### H-I3 — W2 client command가 만들어지고 전송되는가

BP 순서:

1. `Scene_InGameInput.cpp`의 `DispatchSkillInput(wSlot, 2u)` — 진입 여부.
2. `Scene_InGameLocalSkills.cpp`의 `gameData.stage.stageCount == 2 && (bLocalStage2Ready || bRequestedStage2)` — `bRequestedStage2 == true`.
3. 같은 파일의 `SendNetworkSkillCommand(slot, cmd, 2)` — `slot == 2`, Direction payload 존재.
4. `Client/Private/Network/Client/CommandSerializer.cpp`의 `wire.itemId = static_cast<u16_t>(skillStage)` — `wire.slot == 2`, `wire.itemId == 2`.

3번까지 왔는데 network가 끊겼다면 현재 `SendNetworkSkillCommand`는 조용히 return하고 상위는 성공으로 간주한다. 이것은 H-N0이다.

### H-S0 — server가 W2를 ingress에서 보존하는가

BP 순서:

1. `Server/Private/Game/CommandIngress.cpp`의 `wire.itemId = packet->itemId()` — `slot == 2`, `itemId == 2`.
2. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`의 `BuildServerCommand` — `cmd.itemId == 2`.
3. 같은 파일 `IsCommandBlockedByAuthoritativeAction` — `action.sourceSlot == 2`, `cmd.slot == 2`, `bSameSkillStageRelease == true`.
4. `HandleCastSkill`의 `bRequestedStage2`와 `bStage2` — 둘 다 참.
5. `LogCastSkill("accept", "stage2", ...)` — server 수락 확정.
6. `IreliaGameSim.cpp::OnW` — `bStage2 == true`.

3번에서 `sourceSlot != cmd.slot`이면 form/spellbook override slot 변환 전후 불일치 가설이다. 일반 Irelia에서는 둘 다 2여야 한다.

### H-S1 — W2 성공 뒤에도 이동이 지연되는가

`GameplayDefinitionQuery::ResolveSkillActionMovePolicy(IRELIA, W, 2)`가 현재 `QueueUntilUnlock`인지 확인한다. `StartCommandActionState` 직후 기대되는 현재 값은 다음이다.

```text
action.stage == 2
action.movePolicy == QueueUntilUnlock
action.lockEndTick - tickIndex <= 8
```

이 상태에서 즉시 Move command를 보내 `ActionStateComponent.bHasQueuedMove == true`가 되면 H-S1 확정이다. 이것은 W2 전송 실패와 별개의 확정 회귀다.

### H-S2 — E 실패가 W1 잔류의 연쇄 증상인가

E command가 `CommandExecutor.cpp:1933`에서 `ActionBlocked`로 반환되는 순간 다음을 본다.

```text
action.sourceChampion == IRELIA
action.sourceSlot == W
action.stage == 1
action.movePolicy == StationaryChannel
tickIndex < action.lockEndTick
cmd.slot == E
```

모두 맞으면 E 자체 target/conditional 문제가 아니라 W1 channel이 남아 E를 막은 것이다. W2 수락 후 E가 `HandleCastSkill`까지 가면 이 연쇄 가설이 맞다.

### H-D0 — `Conditional`이 hold 입력에서 소비되는가

다음 두 곳에 read BP를 걸어도 W release 전에 호출되지 않아야 한다.

- `SkillDefAdapters::ToTargetResolvePolicy`
- `CScene_InGame::BuildCastCommand`

`SkillGameAtomBundle.target.resolvePolicy == ChampionStateDependent`여도 `UpdateCombatInput`은 값을 읽지 않는다. 따라서 “Conditional hold 분기”는 현재 구현에 존재하지 않는다.

### H-EZ0 — Ezreal E가 Direction payload로 world origin을 향하는가

1. `Ezreal_Registration.cpp`에서 E `targetMode == Direction` 확인.
2. `BuildCastCommand` Direction 분기에서 `groundPos == {0,0,0}` 확인.
3. serializer/server ingress에서도 ground zero 확인.
4. `EzrealGameSim::QueuePendingCast`가 E라는 이유로 `bHasGroundTarget = true`로 설정하는지 확인.
5. `LaunchArcaneShift`가 `(groundTarget - origin)`을 쓰는지 확인.

이 다섯 값이 맞으면 client/server target contract 회귀 확정이다.

## 2-2. V1 — 최소 행동 복구 버전

V1은 H-I0/H-I1과 무관하게 확정된 W2 정책을 고치고, H-I0/H-I1이 재현되면 release latch를 함께 적용한다.

### 2-2-1. `Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp` — Irelia W2 즉시 unlock

현재 블록:

```cpp
        case eChampion::IRELIA:
            if (slot == q) return eSkillActionMovePolicy::ForcedMotion;
            if (slot == w && stage <= 1u)
                return eSkillActionMovePolicy::StationaryChannel;
            if (slot == e) return eSkillActionMovePolicy::Allow;
            return eSkillActionMovePolicy::QueueUntilUnlock;
```

아래로 교체:

```cpp
        case eChampion::IRELIA:
            if (slot == q) return eSkillActionMovePolicy::ForcedMotion;
            if (slot == w)
            {
                return stage <= 1u
                    ? eSkillActionMovePolicy::StationaryChannel
                    : eSkillActionMovePolicy::Allow;
            }
            if (slot == e) return eSkillActionMovePolicy::Allow;
            return eSkillActionMovePolicy::QueueUntilUnlock;
```

예상: server가 W2를 수락한 tick부터 `ResolveSkillActionLockTicks`가 0을 반환하고 Move/E가 즉시 통과한다.

### 2-2-2. `Engine/Public/Core/CInput.h` — 소비될 때까지 남는 release latch

H-I0 또는 H-I1이 확인될 때만 적용한다.

현재 블록:

```cpp
    void OnKeyDown(uint8 vKey) { m_Keys[vKey] = true; }
    void OnKeyUp(uint8 vKey) { m_Keys[vKey] = false; }
```

아래로 교체:

```cpp
    void OnKeyDown(uint8 vKey)
    {
        m_KeyReleased[vKey] = false;
        m_Keys[vKey] = true;
    }
    void OnKeyUp(uint8 vKey)
    {
        if (m_Keys[vKey])
            m_KeyReleased[vKey] = true;
        m_Keys[vKey] = false;
    }
    bool ConsumeKeyReleased(uint8 vKey)
    {
        const bool released = m_KeyReleased[vKey];
        m_KeyReleased[vKey] = false;
        return released;
    }
    void OnFocusGained();
```

현재 멤버 블록:

```cpp
    bool  m_Keys[256] = {};

    bool m_pPrevKeys[256] = {};
```

아래로 교체:

```cpp
    bool m_Keys[256] = {};
    bool m_KeyReleased[256] = {};

    bool m_pPrevKeys[256] = {};
```

### 2-2-3. `Engine/Private/Platform/CInput.cpp` — focus 복귀 시 실제 key 상태 재동기화

`CInput::IsKeyReleased` 아래에 추가:

```cpp
void CInput::OnFocusGained()
{
    for (u32_t key = 0; key < 256u; ++key)
    {
        const bool wasDown = m_Keys[key];
        const bool isDown = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
        if (wasDown && !isDown)
            m_KeyReleased[key] = true;

        m_Keys[key] = isDown;
        m_pPrevKeys[key] = isDown;
    }
}
```

`m_pPrevKeys`를 실제 상태에 맞추는 이유는 focus 밖에서 누른 키를 새 press로 합성하지 않기 위해서다. release는 별도 latch가 보존한다.

### 2-2-4. `Engine/Private/Platform/CWin32Window.cpp` — focus message 연결

현재 두 번째 `switch (msg)`의 `WM_ERASEBKGND` 위에 추가:

```cpp
    case WM_SETFOCUS:
        input.OnFocusGained();
        break;

```

BP 결과 ImGui handler가 `WM_SETFOCUS`를 선점한다면 이 case는 `ImGui_ImplWin32_WndProcHandler` 호출 전의 첫 번째 switch로 이동한다. 이 위치 선택은 H-I0 결과로 확정한다.

### 2-2-5. `Client/Public/Scene/Scene_InGame.h` — TU-global pending 제거

현재 `m_bPingWheelActive` 아래에 추가:

```cpp
    bool_t m_bSkillReleasePending[5]{};
```

현재 메서드 블록:

```cpp
    void UpdateCombatInput(bool& outSkipGroundMove);
```

아래로 교체:

```cpp
    void UpdateCombatInput(bool& outSkipGroundMove);
    void UpdateSkillKeyInput(u8_t slot, uint8 key);
```

`Scene_InGameInput.cpp` namespace의 아래 전역은 삭제:

```cpp
    bool_t s_bWReleasePending = false;
```

이 상태를 scene member로 옮겨 champion/scene 교체 뒤 stale latch가 남지 않게 한다.

### 2-2-6. `Client/Private/Scene/Scene_InGameInput.cpp` — activation별 입력 처리

`UpdateCombatInput` 위에 아래 메서드를 추가한다. V2 schema가 들어오기 전 V1에서는 Irelia/Viego W만 `PressRelease`, Zed W만 `PressRecast`로 임시 해석한다. 이 champion fallback은 V2 완료와 동시에 삭제한다.

```cpp
void CScene_InGame::UpdateSkillKeyInput(u8_t slot, uint8 key)
{
    SkillGameAtomBundle gameData{};
    if (!CSkillRegistry::Instance().ResolveGameAtoms(
        GetPlayerChampionId(), slot, gameData))
    {
        return;
    }

    const bool_t pressed = CInput::Get().IsKeyPressed(key);
    const bool_t released = CInput::Get().ConsumeKeyReleased(key);
    const bool_t pending = HasPendingSkillStage(*this, slot);
    const eChampion champion = GetPlayerChampionId();
    const bool_t championRecastReady =
        champion == eChampion::ZED &&
        (slot == static_cast<u8_t>(eSkillSlot::W) ||
            slot == static_cast<u8_t>(eSkillSlot::R)) &&
        ZedFx::CanSwapToShadowClone(GetWorld(), GetPlayerEntity(), slot);
    const bool_t recastReady = pending || championRecastReady;
    const bool_t pressRelease =
        slot == static_cast<u8_t>(eSkillSlot::W) &&
        (champion == eChampion::IRELIA || champion == eChampion::VIEGO);

    if (pressRelease)
    {
        if (pressed && !pending)
        {
            ClearNetworkAttackIntent();
            const bool_t dispatched = DispatchSkillInput(slot, 1u);
            m_bSkillReleasePending[slot] =
                dispatched && HasPendingSkillStage(*this, slot);
        }
        else if (released && (m_bSkillReleasePending[slot] || pending))
        {
            ClearNetworkAttackIntent();
            if (DispatchSkillInput(slot, 2u))
                m_bSkillReleasePending[slot] = false;
        }
        return;
    }

    if (!pressed)
        return;

    ClearNetworkAttackIntent();
    DispatchSkillInput(slot, recastReady ? 2u : 1u);
}
```

주의: 이 코드는 `ConsumeKeyReleased`가 있으므로 조기 반환된 frame의 release를 다음 gameplay input frame까지 보존한다. `pressed`와 `released`를 함수 진입 직후 읽어야 한다.

현재 Q/W/E/R 입력 블록 전체를 아래로 교체:

```cpp
        UpdateSkillKeyInput(static_cast<u8_t>(eSkillSlot::Q), 'Q');
        if (in.IsKeyPressed('F'))
        {
            ClearNetworkAttackIntent();
            TriggerFlash();
        }
        UpdateSkillKeyInput(static_cast<u8_t>(eSkillSlot::W), 'W');
        UpdateSkillKeyInput(static_cast<u8_t>(eSkillSlot::E), 'E');
        UpdateSkillKeyInput(static_cast<u8_t>(eSkillSlot::R), 'R');
```

이 교체는 현재 Zed 전용 W/R 분기를 helper 안의 `championRecastReady`로 보존한다. local stage가 없어도 실제 shadow clone이 존재하면 swap stage2를 보내야 하기 때문이다. V2/V3에서 authoritative recast-ready 상태가 일반화되기 전에는 이 Zed fallback을 삭제하지 않는다. 그 밖의 pending 2단계 스킬은 두 번째 press로 stage2를 보내며 Irelia/Viego W만 release로 stage2를 보낸다.

`ResetLocalControlHandoffState()`의 `ResetLocalSkillRuntimeState();` 아래에 추가:

```cpp
    for (bool_t& pending : m_bSkillReleasePending)
        pending = false;
```

### 2-2-7. network send가 실제로 실패했는데 성공 처리하는 문제

`Client/Public/Network/Client/CommandSerializer.h` 현재 선언:

```cpp
    void SendCastSkill(CClientNetwork& net, u8_t slot, NetEntityId targetNet,
        const Vec3& groundPos, const Vec3& direction, u8_t skillStage = 1);
```

아래로 교체:

```cpp
    u32_t SendCastSkill(CClientNetwork& net, u8_t slot, NetEntityId targetNet,
        const Vec3& groundPos, const Vec3& direction, u8_t skillStage = 1);
```

`Client/Private/Network/Client/CommandSerializer.cpp` 함수 반환형을 `u32_t`로 바꾸고 마지막을 아래로 교체:

```cpp
    SendSingle(net, wire);
    return wire.sequenceNum;
```

`Client/Public/Scene/Scene_InGame.h` 현재 선언:

```cpp
    void SendNetworkSkillCommand(u8_t slot, const CastSkillCommand& cmd, u8_t skillStage = 1);
```

아래로 교체:

```cpp
    u32_t SendNetworkSkillCommand(u8_t slot, const CastSkillCommand& cmd, u8_t skillStage = 1);
```

`Scene_InGameLocalSkills.cpp`의 함수는 연결/serializer 누락 시 `0u`, basic attack은 `attackSeq`, cast는 `SendCastSkill` 반환 sequence를 반환하도록 교체한다. 이후 stage1/stage2 network 분기에서 sequence가 0이면 `false`를 반환하고 local stage를 arm/clear하지 않는다.

```cpp
    const u32_t commandSeq = SendNetworkSkillCommand(slot, cmd, skillStage);
    if (commandSeq == 0u)
        return false;
```

정확한 세 호출부의 최종 교체는 V0에서 BasicAttack 경로와 spellbook override가 같은 반환 계약을 쓰는지 재확인 후 적용한다. `CONFIRM_NEEDED`: `SendNetworkSkillCommand` basic-attack 보호 yaw와 모든 caller의 반환값 사용 여부.

### 2-2-8. Ezreal E 즉시 계약 복구

`Client/Private/GameObject/Champion/Ezreal/Ezreal_Registration.cpp` 현재 E 블록:

```cpp
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 3;
                s.targetMode = eTargetMode::Direction;
                s.animKey = "spell3_generic"; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.onCastAcceptedHookId = kEz_E_OnAccept;
                s.keySwapHookId = kEz_E_KeySwap;
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 3, s);
            }
```

아래로 교체:

```cpp
            {
                SkillDef s{};
                s.champ = eChampion::EZREAL; s.slot = 3;
                s.targetMode = eTargetMode::GroundTarget;
                s.animKey = "spell3_generic"; s.bOneShot = true;
                s.rotate = eRotateMode::TowardsCursor;
                s.onCastAcceptedHookId = kEz_E_OnAccept;
                s.keySwapHookId = kEz_E_KeySwap;
                CSkillRegistry::Instance().Add(eChampion::EZREAL, 3, s);
            }
```

현재 GroundTarget builder는 `groundPos`와 `direction`을 모두 채우므로 server `LaunchArcaneShift`와 시각 yaw가 동시에 유지된다.

## 2-3. V2 — Data Driven input/target 계약 버전

V2가 끝나면 V1의 champion 이름 fallback과 `SkillRegistry` target 보존 임시 조치를 삭제한다.

### 2-3-1. `Shared/GameSim/Definitions/SkillTypes.h`와 `SkillAtomData.h` — input activation 타입 추가

`SkillTypes.h`의 `eSkillSlot` 위에 추가:

```cpp
enum class eSkillInputActivation : uint8_t
{
    Press = 0,
    PressRecast = 1,
    PressRelease = 2,
};
```

공통 enum을 `SkillTypes.h`에 두어 이 파일을 이미 include하는 `SkillDef`, `ChampionGameData`, `SkillAtomData`가 동일 타입을 쓴다. `SkillAtomData.h`에 enum을 두고 `SkillDef.h`가 역방향 include하게 만들지 않는다.

`SkillAtomData.h`의 `SkillTargetSpec` 위에 추가:

```cpp
struct SkillInputSpec
{
    eSkillInputActivation activation = eSkillInputActivation::Press;
};
```

현재 `SkillGameAtomBundle` 블록:

```cpp
struct SkillGameAtomBundle
{
    bool_t bValid = false;
    SkillSlotBinding slot{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
    SummonPolicySpec summonPolicy{};
};
```

아래로 교체:

```cpp
struct SkillGameAtomBundle
{
    bool_t bValid = false;
    SkillSlotBinding slot{};
    SkillInputSpec input{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
    SummonPolicySpec summonPolicy{};
};
```

### 2-3-2. `Shared/GameSim/Definitions/SkillDef.h`와 `ChampionGameData.h`

`SkillDef`의 `scalingTableId` 아래에 추가:

```cpp
    eSkillInputActivation inputActivation = eSkillInputActivation::Press;
```

field 끝에 추가해 과거 aggregate initializer의 필드 위치를 바꾸지 않는다.

`ChampionGameDataSkillStage` 현재 블록:

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
    eTargetMode targetMode = eTargetMode::Self;
    f32_t lockDurationSec = 0.6f;
};
```

`ChampionGameDataSkill`의 `targetMode` 아래에 추가:

```cpp
    eSkillInputActivation inputActivation = eSkillInputActivation::Press;
```

schema version은 2로 올린다.

### 2-3-3. `Tools/ChampionData/build_champion_game_data.py`

`normalize_skill`은 `activationMode`를 읽고, `stageCount == 2`인데 필드가 없으면 실패하게 한다. 허용 값은 `Press`, `PressRecast`, `PressRelease` 세 개뿐이다.

`append_skill`의 현재 target 출력 아래에 추가:

```python
    lines.append(
        f"        skill{slot}.inputActivation = "
        f"{enum_value('eSkillInputActivation', skill['activationMode'])};")
```

현재 stage 출력:

```python
        lines.append(f"        stage{slot}_{stage_index}.lockDurationSec = {cpp_float(stage['lockDurationSec'])};")
```

아래로 교체:

```python
        lines.append(
            f"        stage{slot}_{stage_index}.targetMode = "
            f"{enum_value('eTargetMode', stage['targetMode'])};")
        lines.append(
            f"        stage{slot}_{stage_index}.lockDurationSec = "
            f"{cpp_float(stage['lockDurationSec'])};")
```

### 2-3-4. `Data/Gameplay/ChampionGameData/champions.json` — 13개 2단계 스킬 activation 전수 지정

각 skill object의 `"stageCount": 2` 아래에 완전한 값으로 추가한다.

| champion/slot | activationMode | 이유 |
|---|---|---|
| IRELIA W | `PressRelease` | hold 후 key-up W2 |
| VIEGO W | `PressRelease` | charge 후 key-up W2 |
| IRELIA E | `PressRecast` | E1/E2 두 번 입력 |
| KALISTA R | `PressRecast` | call/launch 두 단계 |
| ZED W | `PressRecast` | summon/swap |
| ZED R | `PressRecast` | mark/return |
| RIVEN R | `PressRecast` | transform/wind slash |
| JAX E | `PressRecast` | counter/early release |
| LEESIN Q | `PressRecast` | projectile/dash |
| LEESIN W | `PressRecast` | safeguard/second cast |
| LEESIN E | `PressRecast` | tempest/cripple |
| YONE E | `PressRecast` | enter/return |
| SYLAS E | `PressRecast` | dash/chain |

모든 1단계 skill은 generator default `Press`를 사용한다. 2단계인데 activation 필드가 빠지면 generator가 실패해야 한다.

Irelia의 현재 Q/W/E 연속 블록:

```json
                {
                    "slot": 1,
                    "targetMode": "Conditional",
                    "stageCount": 1,
                    "stageWindowSec": 0.0,
                    "cooldownSec": 3.0,
                    "rangeMax": 6.0,
                    "manaCost": 20.0,
                    "skillId": 0,
                    "scalingTableId": 0,
                    "gameplayPolicyId": 0,
                    "visualCueId": 0,
                    "stages": [
                        {
                            "lockDurationSec": 0.36
                        }
                    ]
                },
                {
                    "slot": 2,
                    "targetMode": "Conditional",
                    "stageCount": 2,
                    "stageWindowSec": 4.0,
                    "cooldownSec": 3.0,
                    "rangeMax": 0.0,
                    "manaCost": 70.0,
                    "skillId": 0,
                    "scalingTableId": 0,
                    "gameplayPolicyId": 0,
                    "visualCueId": 0,
                    "stages": [
                        {
                            "lockDurationSec": 5.0
                        },
                        {
                            "lockDurationSec": 0.4
                        }
                    ]
                },
                {
                    "slot": 3,
                    "targetMode": "Conditional",
                    "stageCount": 2,
                    "stageWindowSec": 3.5,
                    "cooldownSec": 3.0,
                    "rangeMax": 9.0,
                    "manaCost": 50.0,
                    "skillId": 0,
                    "scalingTableId": 0,
                    "gameplayPolicyId": 0,
                    "visualCueId": 0,
                    "stages": [
                        {
                            "lockDurationSec": 0.9
                        },
                        {
                            "lockDurationSec": 0.45
                        }
                    ]
                },
```

아래로 교체:

```json
                {
                    "slot": 1,
                    "targetMode": "UnitTarget",
                    "stageCount": 1,
                    "stageWindowSec": 0.0,
                    "cooldownSec": 3.0,
                    "rangeMax": 6.0,
                    "manaCost": 20.0,
                    "skillId": 0,
                    "scalingTableId": 0,
                    "gameplayPolicyId": 0,
                    "visualCueId": 0,
                    "stages": [
                        {
                            "targetMode": "UnitTarget",
                            "lockDurationSec": 0.36
                        }
                    ]
                },
                {
                    "slot": 2,
                    "targetMode": "Direction",
                    "stageCount": 2,
                    "activationMode": "PressRelease",
                    "stageWindowSec": 4.0,
                    "cooldownSec": 3.0,
                    "rangeMax": 0.0,
                    "manaCost": 70.0,
                    "skillId": 0,
                    "scalingTableId": 0,
                    "gameplayPolicyId": 0,
                    "visualCueId": 0,
                    "stages": [
                        {
                            "targetMode": "Direction",
                            "lockDurationSec": 5.0
                        },
                        {
                            "targetMode": "Direction",
                            "lockDurationSec": 0.4
                        }
                    ]
                },
                {
                    "slot": 3,
                    "targetMode": "GroundTarget",
                    "stageCount": 2,
                    "activationMode": "PressRecast",
                    "stageWindowSec": 3.5,
                    "cooldownSec": 3.0,
                    "rangeMax": 9.0,
                    "manaCost": 50.0,
                    "skillId": 0,
                    "scalingTableId": 0,
                    "gameplayPolicyId": 0,
                    "visualCueId": 0,
                    "stages": [
                        {
                            "targetMode": "GroundTarget",
                            "lockDurationSec": 0.9
                        },
                        {
                            "targetMode": "GroundTarget",
                            "lockDurationSec": 0.45
                        }
                    ]
                },
```

### 2-3-5. `Client/Private/GamePlay/SkillRegistry.cpp`

`ApplyAuthoredGameplayData`에서 authored skill을 얻은 뒤 아래를 추가한다.

```cpp
        def.inputActivation = skill.inputActivation;
        def.targetMode = skill.stages[0].targetMode;
        if (skill.stageCount >= 2u)
            def.stage2TargetMode = skill.stages[1].targetMode;
```

그 뒤 “targetMode는 의도적으로 덮지 않는다” 임시 주석과 보존 정책을 삭제한다. target/stage schema 생성 parity test가 먼저 통과해야 삭제할 수 있다.

`SkillDefGameDataAdapter.h::BuildSkillGameAtomBundle`의 slot 설정 아래에 추가:

```cpp
        data.input.activation = def.inputActivation;
```

`Scene_InGame::UpdateSkillKeyInput`은 activation 선택을 위한 champion 비교를 삭제하고 아래만 사용한다. Zed clone 존재 여부처럼 “recast가 아직 가능한가”를 판단하는 champion fallback은 V3 authoritative recast-state가 생길 때까지 별도 유지한다.

```cpp
    switch (gameData.input.activation)
    {
    case eSkillInputActivation::PressRelease:
        // press=stage1, release=stage2
        break;
    case eSkillInputActivation::PressRecast:
        // press=pending ? stage2 : stage1
        break;
    case eSkillInputActivation::Press:
    default:
        // press=stage1
        break;
    }
```

`CONFIRM_NEEDED`: 이 switch의 완전한 교체 body는 V1 구현 결과의 helper 시그니처와 `ConsumeKeyReleased` 적용 여부를 확인한 뒤 확정한다. activation mode 선택을 위한 champion 이름 분기가 0개라는 것이 V2 완료 조건이며, recast-ready champion fallback 0개는 V3 완료 조건이다.

### 2-3-6. server pack도 같은 필드를 생성한다

`Tools/LoLData/Build-LoLDefinitionPack.py`는 같은 `champions.json`에서 `activationMode`와 stage별 target을 읽어 다음 두 산출물에 기록한다.

- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`

`SkillGameplayDef`에 `SkillInputSpec input{}`을 추가하고 generated code가 `def.input.activation`을 설정한다. client generator와 server generator가 서로 다른 default를 갖지 않게 공통 허용 값/parity 검사를 추가한다.

## 2-4. V3 — action lock과 charge를 데이터/서버 권위로 완성

### 2-4-1. animation timing과 command lock을 분리한다

현재 `lockDurationSec` 하나가 과거 client animation 보호 시간과 새 server command lock으로 동시에 쓰인 것이 회귀의 구조적 원인이다. stage schema를 아래 의미로 확장한다.

```json
{
  "targetMode": "Direction",
  "animationLockSec": 0.4,
  "commandLockSec": 0.0,
  "movePolicy": "Allow"
}
```

Irelia W 권위 값:

| stage | activation | movePolicy | commandLockSec | 의미 |
|---|---|---|---:|---|
| W1 | PressRelease | StationaryChannel | 4.0 | 최대 hold window까지만 정지 |
| W2 | PressRelease | Allow | 0.0 | release 수락 즉시 이동/E 허용 |

`GameplayDefinitionQuery::ResolveSkillActionMovePolicy`의 champion switch는 generated stage policy 조회로 교체한다. 값 누락 시 Debug에서 fail-fast하고 Release에서 `Allow`로 조용히 완화하지 않는다. 85 slots × 실제 stage 전체가 채워져야 hard-coded switch를 삭제한다.

`CONFIRM_NEEDED`: 전체 96 stage의 `movePolicy/commandLockSec` 값은 V0 BP와 champion별 현재 구현을 표로 동결한 뒤 입력한다. 일부만 채운 상태에서 switch를 삭제하지 않는다.

### 2-4-2. server authoritative charge state

Irelia W와 Viego W는 “2단계”만으로 충분하지 않다. server component에 최소 다음 상태가 필요하다.

```cpp
struct SkillChargeState
{
    bool_t bActive = false;
    u8_t slot = 0u;
    u64_t startTick = 0u;
    u64_t maxReleaseTick = 0u;
    Vec3 aimDirection{};
};
```

- W1 server accept: charge state arm, start tick/aim 저장.
- W2 server accept: elapsed tick으로 charge ratio 계산, 상태 consume, W2 gameplay 실행.
- max hold 도달: server가 deterministic auto-release command/effect를 실행하고 channel을 반드시 해제.
- death/stun/form change/disconnect: explicit cancel reason으로 charge와 action lock을 함께 정리.
- Client는 damage/range/stun truth를 계산하지 않고 aim/press/release `GameCommand`만 보낸다.
- Bot AI도 component를 직접 바꾸지 않고 W1/W2 `GameCommand`를 순서대로 생산한다.

현재 data에는 Irelia/Viego W charge curve/maxHold 값이 없다. `CONFIRM_NEEDED`: 원본 스킬 사양 또는 프로젝트 의도값을 확정한 뒤 `maxHoldSec`, min/max damage/range/stun curve를 authored data에 추가한다. 임의 숫자로 구현하지 않는다.

### 2-4-3. command acceptance/rejection 가시성

현재 client는 network send 성공을 server cast accept로 오인한다. `lastAckedCommandSeq`는 수신 순서 ack이지 gameplay accept가 아니다. protocol에 아래 결과를 추가한다.

```text
CommandResult {
  sequenceNum
  accepted
  reason
  authoritativeStage
  stageWindowEndTick
}
```

Client pending stage는 send 직후 지우지 않고 해당 sequence의 accepted result 또는 authoritative snapshot stage로 확정한다. `ActionBlocked`, `StageWindowExpired`, `Cooldown`, `NoMana`, `InvalidTarget`을 구분해 Debug overlay와 bounded `OutputDebugStringA`에 표시한다.

`CONFIRM_NEEDED`: FlatBuffers schema와 snapshot/event 중 어느 경로를 사용할지는 현재 mixed-version compatibility 정책을 확인한 뒤 선택한다. 새 wire field를 넣기 전 old reader fallback test를 먼저 만든다.

## 2-5. V4 — 표현과 기존 누락 회귀 복구

### Irelia W aim cue

`Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp` 현재 블록:

```cpp
        const bool_t bPlayed =
            CFxCuePlayer::PlayAll(world, "Irelia.W.Spin", hold, &state.wHoldCueIds) != NULL_ENTITY;
        state.bWHoldCueActive = false;
        return bPlayed;
```

아래로 교체:

```cpp
        const bool_t bPlayed =
            CFxCuePlayer::PlayAll(world, "Irelia.W.Spin", hold, &state.wHoldCueIds) != NULL_ENTITY;
        state.bWHoldCueActive = bPlayed;
        return bPlayed;
```

현재 false 고정 때문에 `UpdateWAimCue`는 항상 즉시 return한다.

### network action loop

현재 Jax E만 별도 loop 처리가 있다. stage action data에 `presentationLoopWhileActive`를 두고 Irelia W1/Viego W1 등 channel을 일반화한다. snapshot의 authoritative action stage가 끝나거나 W2 event가 오면 한 번만 loop를 해제한다.

### Irelia E client fake auto-stage2

현재 2초 후 두 번째 blade visual만 만드는 local path는 authoritative W/E stage와 분리돼 있다. server auto-stage2 사양이 아니라면 이 visual-only path를 제거한다. 자동 E2가 제품 요구라면 Client가 임의 gameplay를 만들지 않고 Server가 timer 만료 시 stage2 command/effect를 생성한다.

## 2-6. V5 — 100% 전수 회귀 gate

### 자동 테스트

1. Client command-contract test: 17 champions × 5 slots = 85 slots의 stage별 activation/target/payload를 생성해 expected matrix와 비교한다.
2. Generated parity: `champions.json -> ChampionGameData.generated.cpp`와 `champions.json -> SkillGameplayDefs.json/generated.cpp`의 activation, stage target, stage count/window, command lock, move policy가 전부 같아야 한다.
3. Irelia W: W1 → hold N ticks → W2 → 같은 tick Move 수락.
4. Irelia W/E: W1 → W2 → 다음 sequence E1 수락, `ActionBlocked` 0.
5. Irelia W timeout/death/stun/disconnect: charge/action state 잔류 0.
6. Viego W: hold duration별 charge ratio와 range/stun curve, release 후 state 잔류 0.
7. Ezreal E end-to-end: client builder부터 server destination까지 cursor 방향, 최대 range clamp, world origin `(0,0,0)` cast.
8. 13개 2단계 skill: PressRelease 2개, PressRecast 11개가 올바른 stage command를 낸다.
9. rejection result: stage window 만료/ActionBlocked/disconnect가 client pending state를 거짓 성공으로 지우지 않는다.
10. mixed-version fixture: 새 input/action fields가 없는 구버전 replay/snapshot을 정책대로 reject 또는 fallback한다.

### 수동 F5 판정

- Irelia: W를 0.1/1/3.9초 hold 후 release, 즉시 우클릭 이동, 즉시 E1/E2, aim FX 유지.
- Viego: W hold/release, charge별 dash/stun 차이, release 후 이동/다른 스킬.
- Ezreal: E를 네 방향과 world origin 근처에서 사용해 cursor 쪽으로 이동.
- LeeSin/Zed/Jax/Riven/Sylas/Yone/Kalista: 두 번째 press가 stage2이고 key-up이 stage2를 발생시키지 않음.
- 정상 F5 roster/map/minion/snapshot/champion/UI/FX를 숨기지 않는다.

# 3. 예측·검증·인수인계

## 3-1. 수정 전 예측

| ID | 예측 | 틀리면 다음 가설 |
|---|---|---|
| P0 | 사용자처럼 W1이 `return true`면 local stage/latch는 arm된다 | registry/stageCount 생성 불일치 재조사 |
| P1 | 무조건 BP 없이 trace하면 WM_KEYUP은 들어온다 | focus/ImGui 선처리 확인 후 Engine latch 적용 |
| P2 | W2가 client serializer까지 가면 `itemId=2`다 | generic activation/serializer stage 전달 오류 |
| P3 | server W2 수락 전 E는 W1 StationaryChannel 때문에 ActionBlocked다 | E payload/target validation 독립 원인 |
| P4 | server W2 수락 뒤 이동은 최대 8 tick queue된다 | client prediction/animation local lock 조사 |
| P5 | W2 policy를 Allow로 바꾸면 server Move/E 지연은 0 tick이다 | MoveSystem/animation handoff의 별도 lock 조사 |
| P6 | Ezreal E는 Direction payload라 ground zero다 | runtime authored target override 경로 재조사 |

## 3-2. 정확한 검증 명령

```powershell
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py --check
powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1 -RequireComplete
```

```powershell
cmake --build build --config Debug --target GameSim SimLab WintersServer WintersGame
```

```powershell
build/Debug/SimLab.exe
```

```powershell
git diff --check -- Engine/Public/Core/CInput.h Engine/Private/Platform/CInput.cpp Engine/Private/Platform/CWin32Window.cpp Client/Public/Scene/Scene_InGame.h Client/Private/Scene/Scene_InGameInput.cpp Client/Private/Scene/Scene_InGameLocalSkills.cpp Client/Public/Network/Client/CommandSerializer.h Client/Private/Network/Client/CommandSerializer.cpp Client/Private/GamePlay/SkillRegistry.cpp Client/Private/GameObject/Champion/Ezreal/Ezreal_Registration.cpp Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp Shared/GameSim/Definitions/SkillTypes.h Shared/GameSim/Definitions/SkillAtomData.h Shared/GameSim/Definitions/ChampionGameData.h Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp Data/Gameplay/ChampionGameData/champions.json Tools/ChampionData/build_champion_game_data.py Tools/LoLData/Build-LoLDefinitionPack.py Tools/SimLab/main.cpp
```

실제 solution/target 이름이 다르면 build 명령은 현재 preset을 확인해 교체한다. 지금 문서 작성 시점에는 빌드/런타임 수정 검증을 실행하지 않았다.

현재 수정 전 생성 baseline은 확인했다.

```text
ChampionGameData --check: PASS, hash 0x780B63F7
LoL Definition Pack --check: PASS, hash 0x8C5D9212
Champions 17 / skills 85 / two-stage skills 13
```

이 PASS는 생성물이 현재 JSON과 일치한다는 뜻일 뿐, activation/hold/action contract가 올바르다는 뜻은 아니다. 현재 schema에 그 필드가 없기 때문에 지금도 PASS하는 것이 이번 gate 누락의 증거다.

## 3-3. Claude 교차 검증 절차와 현재 상태

독립 감사 1차 프롬프트:

```text
Winters current worktree를 독립 감사하라. Irelia W1 client dispatch 성공 후 W key-up이 W2로 이어지는 전체 경로, Data Driven cutover 전후 ActionState 차이, Conditional target resolve와 input activation의 관계, Irelia W2 이동/E 지연, Viego W charge, Ezreal E payload를 코드와 git history로 검증하라. 내 결론을 전제하지 말고 반증 가능한 지점과 정확한 BP를 먼저 제시하라. gameplay source는 수정하지 마라.
```

계획서 리뷰 2차 프롬프트:

```text
.md/plan/2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_PLAN.md를 review하라. (1) 과거 SkillTable이 왜 동작했는지, (2) Conditional을 hold로 해석하지 않는 결정, (3) release latch의 focus/early-return 안전성, (4) W2 Allow의 서버 권위 부작용, (5) 13개 2단계 activation 분류, (6) client/server generator parity, (7) 빠진 regression test를 반대자 관점에서 지적하라. 코드 수정은 하지 마라.
```

2026-07-18 현재 local Claude CLI 2.1.209는 설치되어 있지만 OAuth session이 만료되어 `Failed to authenticate: OAuth session expired and could not be refreshed`로 독립 감사를 실행하지 못했다. 상태는 `CLAUDE_REVIEW_PENDING`이다. `claude auth login` 후 위 두 pass를 실행하고 지적을 이 문서의 결정 기록/버전 gate에 반영하기 전에는 “Claude 교차 검증 완료”로 표시하지 않는다.

## 3-4. 완료 조건

- 사용자 BP에서 W1/W2 command sequence와 server accept/reject가 한 줄로 이어진다.
- Irelia W2 수락 tick 직후 Move/E가 지연 없이 수락된다.
- Irelia/Viego만 release activation이며 나머지 2단계 스킬은 second press다.
- client/server generated activation/target/action 계약 차이가 0이다.
- charge/cancel/timeout 뒤 server ActionState와 client pending state 잔류가 0이다.
- SimLab, client contract test, Debug Client/Server build, 정상 F5 수동 matrix가 모두 통과한다.
- Claude 2-pass 또는 명시적으로 동등한 독립 리뷰가 완료되고 반대 근거가 처리된다.

구현 후 별도 `Session - ... RESULT` 문서에 BP 캡처 값, command sequence/tick, 테스트 출력, 실패/보류 항목을 기록한다.
