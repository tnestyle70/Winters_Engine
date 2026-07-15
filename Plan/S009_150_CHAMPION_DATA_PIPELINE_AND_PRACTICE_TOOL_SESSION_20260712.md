Session - 150개 챔피언이 늘어나도 공통 실행기·검증·데이터 계약이 먼저 확장되는 구조를 확정하고, 별도 Scene 없이 정상 네트워크 경기 위에서 서버 권위 연습 도구와 JSON 밸런스 임시 오버레이를 검증한다.

1. 반영해야 하는 코드

1-1. 현재 구조의 판정과 북극성

현재 `Data/GameModes/gameMode.json`과 `CGameModeCatalog`의 `practice_tool`은 클라이언트 메뉴 메타데이터일 뿐이다. Lobby/Hello/GameRoom에는 mode 또는 ruleset이 전달되지 않고 `CLOLGameModeRuntime`도 gameplay 동작을 소유하지 않는다. 반대로 실제 gameplay truth는 이미 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 경로에 있다. 따라서 연습 도구를 별도 Scene으로 만들면 정상 연결·월드·AI·snapshot 경로가 갈라져 밸런스 검증 결과가 실제 경기와 달라진다.

연습 도구는 다음 경계를 지킨다.

```text
F10 Champion Data / Practice Tool ImGui
  -> typed PracticeControl command
  -> Server GameRoom Debug + host gate
  -> tick-boundary practice executor
  -> Shared/GameSim component / definition overlay
  -> existing Snapshot/Event
  -> client read-only observation
```

150개 챔피언 확장 방향은 다음 다섯 층으로 고정한다.

```text
Authoring JSON/Spreadsheet
  -> schema + semantic validator
  -> immutable ServerPrivate gameplay pack / ClientPublic visual pack
  -> common skill atoms (target, timing, cost, cooldown, damage, CC, motion, cue)
  -> champion module (공통 atom으로 표현할 수 없는 고유 규칙만)
  -> deterministic SimLab + normal-match Practice Tool 검증
```

안정 ID는 `DefinitionKey`, dense ID는 한 cooked pack 내부에서만 사용한다. 밸런스 값은 JSON/팩이 소유하고 챔피언 C++ hook은 고유 상태 전이만 소유한다. 런타임 연습 오버레이는 source JSON을 대신하지 않으며, 세션 종료 또는 Clear에서 사라지는 임시 패치다. 승인된 값은 반드시 authoring JSON에 다시 반영하고 cook/check/build를 거친다.

현재 남은 구조 부채는 다음 단계에서 제거한다.

- `GameplayDefinitionQuery::ResolveSkillActionMovePolicy`의 champion switch를 stage atom 데이터로 이전한다.
- champion hook이 cast 승인 시 즉시 결과를 만드는 구조를 Cast/Impact/Recovery phase executor로 분리한다.
- 공통 target shape, damage/scaling, status, forced motion, summon, projectile를 generic executor로 올린다.
- champion별 C++은 Viego possession, Sylas ultimate theft처럼 고유 상태 기계만 남긴다.
- schema validation, pack hash/epoch, deterministic per-champion test matrix를 CI gate로 만든다.
- designer patch의 Apply/Reject/EffectiveTick/Hash를 명시적 결과 이벤트로 복제한다.

1-2. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

`CommandKind`의 `CompanionCommand = 11` 아래에 연습 명령을 추가하고, enum 바로 아래에 명시적인 operation을 추가한다.

```fbs
    CompanionCommand = 11,
    PracticeControl = 12
}

enum PracticeOperation : ushort {
    None = 0,
    SetEnabled = 1,
    SetOptions = 2,
    RestoreHealthMana = 3,
    ResetCooldowns = 4,
    AddGold = 5,
    SetLevel = 6,
    Teleport = 7,
    SpawnMinion = 8,
    ClearPracticeSpawns = 9,
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11
}
```

`CommandPacket`의 끝에 하위 호환 필드를 추가한다.

```fbs
    practiceOperation:PracticeOperation;
    practiceValue:float;
    practiceFlags:uint;
```

기존 `slot`, `itemId`, `direction.x`에 관리자 값을 숨겨 넣지 않는다. `slot`은 스킬 슬롯 또는 미니언 role처럼 기존 의미가 분명한 경우에만 재사용하고, operation/value/flags는 wire contract에 이름을 남긴다.

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

`eCommandKind`, companion enum, wire/command를 다음처럼 확장한다.

```cpp
    CompanionCommand = 11,
    PracticeControl = 12,
};

enum class ePracticeOperation : uint16_t
{
    None = 0,
    SetEnabled = 1,
    SetOptions = 2,
    RestoreHealthMana = 3,
    ResetCooldowns = 4,
    AddGold = 5,
    SetLevel = 6,
    Teleport = 7,
    SpawnMinion = 8,
    ClearPracticeSpawns = 9,
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11,
};

inline constexpr u32_t kPracticeInfiniteHealthFlag = 1u << 0;
inline constexpr u32_t kPracticeInfiniteManaFlag = 1u << 1;
inline constexpr u32_t kPracticeNoCooldownFlag = 1u << 2;
inline constexpr u32_t kPracticeInfiniteGoldFlag = 1u << 3;
inline constexpr u32_t kPracticeAllOptionFlags =
    kPracticeInfiniteHealthFlag |
    kPracticeInfiniteManaFlag |
    kPracticeNoCooldownFlag |
    kPracticeInfiniteGoldFlag;
```

`GameCommandWire`와 `GameCommand`의 `itemId` 아래에 각각 추가한다. `sourceSessionId`는 server host 검증이 `BuildServerCommand` 뒤에서도 가능하게 보존한다.

```cpp
    ePracticeOperation practiceOperation = ePracticeOperation::None;
    f32_t practiceValue = 0.f;
    u32_t practiceFlags = 0u;
```

```cpp
    uint32_t sourceSessionId = 0u;
    ePracticeOperation practiceOperation = ePracticeOperation::None;
    f32_t practiceValue = 0.f;
    u32_t practiceFlags = 0u;
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`BuildServerCommand`에서 현재 버리는 session과 신규 typed payload를 보존한다.

```cpp
    cmd.sourceSessionId = sessionId;
    cmd.practiceOperation = wire.practiceOperation;
    cmd.practiceValue = wire.practiceValue;
    cmd.practiceFlags = wire.practiceFlags;
```

`PracticeControl`은 일반 플레이어 action gate와 공용 executor에 넣지 않는다. 죽은 대상 복구, entity spawn, room policy를 다루므로 `CGameRoom`이 소비한다.

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

`PracticeDummyTag` 바로 아래에 서버 연습 상태와 bounded definition overlay를 추가한다.

```cpp
struct PracticeSpawnedTag
{
    EntityID ownerEntity = NULL_ENTITY;
};

struct PracticePlayerComponent
{
    u32_t optionFlags = 0u;
    u32_t revision = 0u;
};

struct PracticeSkillEffectOverrideEntry
{
    u8_t slot = 0u;
    u8_t paramId = 0u;
    f32_t value = 0.f;
};

struct PracticeSkillEffectOverrideComponent
{
    static constexpr u8_t kMaxEntries = 32u;
    PracticeSkillEffectOverrideEntry entries[kMaxEntries]{};
    u8_t count = 0u;
    u32_t revision = 0u;
};
```

동적 map/JSON 객체를 GameSim component에 넣지 않는다. 동일 slot/param은 replace하고 entry 수는 32개로 제한해 결정성과 패킷/메모리 상한을 유지한다.

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp

`ResolveSkillEffectParam`의 pack 조회 전에 issuer의 임시 overlay를 확인한다.

```cpp
        if (entity != NULL_ENTITY &&
            world.HasComponent<PracticeSkillEffectOverrideComponent>(entity))
        {
            const auto& overrides =
                world.GetComponent<PracticeSkillEffectOverrideComponent>(entity);
            for (u8_t i = 0u; i < overrides.count; ++i)
            {
                const auto& entry = overrides.entries[i];
                if (entry.slot == slot &&
                    entry.paramId == static_cast<u8_t>(param))
                {
                    return entry.value;
                }
            }
        }
```

이 경로를 사용하는 실제 서버 damage/CC/range reader가 즉시 새 값을 읽는다. 아직 legacy hardcode를 읽는 스킬은 자동으로 바뀌지 않으며, pack parity/reader coverage 검증으로 따로 제거한다.

1-7. C:/Users/user/Desktop/Winters/Server/Private/Game/CommandIngress.cpp

`wire.itemId = packet->itemId();` 아래에서 신규 FlatBuffers 필드를 복사한다.

```cpp
        wire.practiceOperation =
            static_cast<ePracticeOperation>(packet->practiceOperation());
        wire.practiceValue = packet->practiceValue();
        wire.practiceFlags = packet->practiceFlags();
```

1-8. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

phase 선언부에 추가한다.

```cpp
    bool_t TryHandlePracticeControl(const TickContext& tc, const GameCommand& cmd);
    void TickPracticeControls(const TickContext& tc);
    void ClearPracticeSpawns();
```

room 상태 멤버에 추가한다.

```cpp
    bool_t m_bPracticeModeEnabled = false;
    std::vector<EntityID> m_PracticeSpawnedEntities;
```

정식 제품 경로에서는 이 bool을 LobbyAuthority가 소유한 `lol_practice` ruleset/capability로 교체한다. 이번 수직 슬라이스는 `_DEBUG` 서버 + room host + host가 보낸 `SetEnabled` 세 조건을 모두 만족할 때만 열린다.

1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`Phase_DrainCommands`는 기존 sequence/session binding을 유지한다. 신규 helper는 다음 정책을 구현한다.

```text
PracticeControl인가?
  아니오 -> 기존 executor로 전달
  예 -> Release에서는 consume/reject
       Debug에서는 sourceSessionId == lobby host 검증
       SetEnabled만 비활성 room에서 허용
       이후 operation/finite/range/quota/nav 검증
       server component/room spawn만 변경
```

동작 계약:

- `SetOptions`: health/mana/no-cooldown/infinite-gold flag 전체를 원자적으로 교체한다.
- `RestoreHealthMana`: 살아 있는 champion의 `HealthComponent`와 `ChampionComponent` mirror를 최대치로 맞춘다. 부활은 별도 operation 전까지 수행하지 않는다.
- `ResetCooldowns`: `SkillStateComponent`, `ChampionComponent.cooldowns`, `SummonerSpellStateComponent`를 0으로 만든다.
- `AddGold`: finite 양수만 받고 saturating cap 1,000,000을 적용한다.
- `SetLevel`: 1~18만 허용하고 `ExperienceComponent`, `StatComponent`, `ChampionComponent`, `SkillRankComponent`를 함께 동기화한다.
- `Teleport`: finite position을 navmesh로 resolve한 뒤 move/chase/combat/recall/forced-motion을 정리하고 Transform을 서버에서 바꾼다.
- `SpawnMinion`: team 0~1, role 0~3, lane 0~2, room quota 100, nav-resolved position만 허용하고 `SpawnServerMinion`을 재사용한다.
- `ClearPracticeSpawns`: `PracticeSpawnedTag`가 붙은 entity만 net-id unbind 후 제거한다.
- `ApplySkillEffectOverride`: slot 0~4, 유효 `eSkillEffectParamId`, finite/semantic range, 32 entry 상한을 검증한다.
- `ClearSkillEffectOverrides`: issuer의 임시 overlay만 제거한다.

모든 거절/승인은 Debug `OutputDebugStringA`에 sequence, session, operation, reason을 bounded trace로 남긴다.

1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

`Phase_ExecuteCommands`를 다음처럼 교체해 관리자 명령이 일반 action gate로 들어가지 않게 한다.

```cpp
void CGameRoom::Phase_ExecuteCommands(TickContext& tc)
{
    for (const auto& cmd : m_pendingExecCommands)
    {
        if (!TryHandlePracticeControl(tc, cmd))
            m_pExecutor->ExecuteCommand(m_world, tc, cmd);
    }
    m_pendingExecCommands.clear();
}
```

`Phase_SimulationSystems`에서 `CDamageQueueSystem::Execute`와 최종 `CDeathSystem::Execute` 사이에 추가한다.

```cpp
    CStatSystem::Execute(m_world, definitions);
    TickPracticeControls(tc);
    CDeathSystem::Execute(m_world, tc);
```

이 위치에서 infinite health는 동일 tick의 damage 이후, death 판정 전에 복구되고 cooldown/mana/gold도 최종 snapshot 전에 서버 truth로 정리된다.

1-10a. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

현재 `StatComponent`가 존재하면 `mana`와 `maxMana`를 둘 다 `stat.manaMax`로 기록해 실제 `ChampionComponent.mana` 소비를 숨긴다. `ChampionComponent` snapshot 블록에 아래를 추가한다.

```cpp
            mana = champion.mana;
```

Yasuo flow처럼 명시적인 자원 override는 뒤쪽 champion-specific snapshot 블록이 계속 최종 값을 덮어쓴다. 이 수정으로 일반 챔피언은 연습 도구의 infinite mana뿐 아니라 정상 경기의 실제 현재 마나도 관찰할 수 있다.

1-11. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

public method를 추가한다.

```cpp
    u32_t SendPracticeControl(
        CClientNetwork& net,
        ePracticeOperation operation,
        f32_t value = 0.f,
        u32_t flags = 0u,
        u8_t slot = 0u,
        const Vec3& groundPos = {});
```

상단 forward declaration에 `enum class ePracticeOperation : uint16_t;`를 추가한다.

1-12. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

`SendCompanionCommand` 아래에 typed serializer를 추가한다.

```cpp
u32_t CCommandSerializer::SendPracticeControl(
    CClientNetwork& net,
    ePracticeOperation operation,
    f32_t value,
    u32_t flags,
    u8_t slot,
    const Vec3& groundPos)
{
    if (operation == ePracticeOperation::None ||
        !std::isfinite(value) ||
        !IsValidMoveGroundPos(groundPos))
    {
        return 0u;
    }

    GameCommandWire wire{};
    wire.kind = eCommandKind::PracticeControl;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.slot = slot;
    wire.groundPos = groundPos;
    wire.practiceOperation = operation;
    wire.practiceValue = value;
    wire.practiceFlags = flags;
    SendSingle(net, wire);
    return wire.sequenceNum;
}
```

`GetCommandKindName`에 `PracticeControl`을 추가하고 `CreateCommandPacket`의 마지막 인자에 신규 fields를 전달한다.

1-13. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

현재 placeholder를 `Practice Tool / Balance Lab`으로 교체한다. 신규 panel 파일을 만들지 않아 이미 등록된 F10 단일 호출과 project wiring을 재사용한다.

UI 계약:

```text
Connection/authority status
Enable Practice Session (Debug host only)
Infinite Health / Infinite Mana / No Cooldowns / Infinite Gold + Apply Options
Restore HP+Mana / Reset Cooldowns / +10,000 Gold / Level Up / Level 18
Teleport X/Y/Z / Use Current Position / Teleport
Spawn Minion team/role/lane/position / Clear Practice Spawns
JSON path / Load / editable override table / Save / Apply / Clear Server Overrides
Observed snapshot HP, mana, gold, level, Q/W/E/R cooldown
last sent sequence + server-authority warning
```

버튼 활성 조건은 `IsNetworkAuthoritativeGameplay`, connected network, serializer 존재다. client ECS는 관찰만 하고 mutation하지 않는다. JSON parser는 다음 param 이름만 명시적으로 허용하고 unknown은 오류로 표시한다.

```text
BaseDamage, DamagePerRank, Range, Speed, MoveSpeedMul,
StunDurationSec, SlowDurationSec, AirborneDurationSec,
DashDistance, DashDurationSec, Radius, EffectDurationSec,
BonusAd, BonusAttackSpeed
```

1-14. 새 파일: C:/Users/user/Desktop/Winters/Client/Bin/Resource/Config/Practice/practice_balance_overrides.json

```json
{
  "version": 1,
  "profileName": "Designer Scratch",
  "scope": "current-player",
  "overrides": [
    {
      "slot": "Q",
      "param": "BaseDamage",
      "value": 100.0
    },
    {
      "slot": "Q",
      "param": "DamagePerRank",
      "value": 25.0
    }
  ]
}
```

이 파일은 임시 디자이너 preset이다. Apply는 서버 component overlay를 갱신할 뿐 generated C++ 또는 canonical gameplay JSON을 덮어쓰지 않는다.

1-14a. C:/Users/user/Desktop/Winters/.gitignore

대용량 Runtime Resource 기본 ignore는 유지하되 `Client/Bin/Resource/Config/Practice/practice_balance_overrides.json` 단일 authored config만 예외로 추적한다. config별 `Debug*/Resource`, `Release*/Resource` 복사본은 만들지 않는다.

1-15. C:/Users/user/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py 및 C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

공용 `as_float`는 Python의 bool을 숫자로 받지 않고 NaN/Infinity를 거절하도록 교체한다.

```python
def as_float(value: object, path: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{path} must be a number")
    result = float(value)
    if not math.isfinite(result):
        fail(f"{path} must be finite")
    return result
```

skill-effect validator는 `halfAngleCos`를 `[-1, 1]`, 나머지 scalar를 `[0, 1000000]`으로 제한한다. summon policy의 `roleType`/`lane`은 0~255 integer-like 값만 허용한다. 전체 검증이 끝나기 전에는 generated output을 갱신하지 않는다.

1-16. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`GameplayDefinitionQuery.h`를 include하고 `RunAuthoredNavGridProbe` 바로 위에 `RunPracticeDefinitionOverlayProbe`를 추가한다. 이 probe는 Annie Q의 canonical `BaseDamage`를 읽고, `PracticeSkillEffectOverrideComponent`의 777 값을 우선 조회하며, component 제거 후 canonical 값으로 복구되는지 확인한다. 이어 `BuildServerCommand`가 session/op/value/flags를 손실 없이 보존하는지 검사한다.

`main`의 probe 목록과 `bPass` 조건에 아래를 추가한다.

```cpp
    const bool_t bPracticeDefinitionOverlayProbePass =
        RunPracticeDefinitionOverlayProbe();
```

```cpp
        bPracticeDefinitionOverlayProbePass &&
```

1-17. 장기 협업·밸런스 패치 운영 계약

```text
Designer edits authoring JSON / sheet
  -> Validate (type, range, reference, target/timing compatibility)
  -> Cook immutable pack
  -> SimLab deterministic golden tests
  -> Practice Tool temporary patch and damage/CC capture
  -> Export/review diff
  -> Commit canonical JSON
  -> Cook/check/build/CI
  -> multiplayer smoke + replay/hash verification
```

기획자 테스트 matrix는 champion/skill/rank마다 최소 다음을 자동 생성한다.

- 대상 없음/자신/아군/적/미니언/중립/구조물, 사거리 경계, wall/nav/vision
- base damage와 AD/AP/체력 계수, 방어력/마저, item/rune 조합, level/rank 1~max
- resource, cooldown, stage window, cast/impact/recovery, 이동 정책
- stun/slow/airborne/forced motion stack와 cleanse/interrupt/death
- projectile/summon 수명, snapshot/reconnect/replay, client FX exactly-once
- 10인 + minion/structure 정상 F5 성능 budget

연습 패치는 source-of-truth가 아니며 `Apply`와 `Promote`를 분리한다. Release server에서는 practice command를 거절하고, 정식 내부 QA 빌드는 signed capability와 host/admin role을 Hello/Lobby에서 받아야 한다.

2. 검증

2-1. 스키마 및 데이터 정합성

```powershell
cmd /c Shared\Schemas\run_codegen.bat
python Tools\LoLData\Build-LoLDefinitionPack.py --root . --check
```

합격 기준:

- C++/Go generated schema가 `PracticeControl`과 신규 fields를 포함한다.
- gameplay pack freshness/hash 검사가 성공한다.
- 기존 CommandPacket은 append-only 변경으로 decode된다.

2-2. Shared/GameSim 경계 및 deterministic 검증

```powershell
powershell -ExecutionPolicy Bypass -File Tools\Harness\Check-SharedBoundary.ps1
msbuild Shared\GameSim\Include\GameSim.vcxproj /m /p:Configuration=Debug /p:Platform=x64
msbuild Tools\SimLab\SimLab.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

합격 기준:

- Shared/GameSim은 Engine/Client/Renderer/UI/ImGui/DX에 의존하지 않는다.
- 동일 slot/param override replace, 32-entry 상한, clear 후 canonical fallback이 deterministic하게 동작한다.
- 기존 champion/status tests가 회귀하지 않는다.

2-3. Server/Client 빌드

```powershell
msbuild Server\Server.vcxproj /m /p:Configuration=Debug /p:Platform=x64
msbuild Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64
git diff --check
```

합격 기준:

- Debug x64 Server/Client가 링크까지 성공한다.
- Release 경로는 `PracticeControl`을 consume/reject하고 gameplay state를 바꾸지 않는다.
- 기존 dirty 변경을 삭제하거나 덮어쓰지 않는다.

2-4. 사용자가 수행할 인게임 검증

```text
1. Debug Server 실행 후 host Client로 정상 경기에 입장한다.
2. F10 -> Practice Tool / Balance Lab -> Enable Practice Session.
3. Infinite Health/Mana/No Cooldowns/Infinite Gold를 적용하고 피격/연속 스킬/구매로 확인한다.
4. Restore, Level Up/18, Teleport, 각 team/role/lane minion spawn을 확인한다.
5. JSON Load -> 값 수정 -> Save -> Apply 후 해당 스킬을 동일 방어력 target에 반복 적중한다.
6. Clear Server Overrides 후 원래 damage로 돌아오는지 확인한다.
7. 두 번째 Client에서는 host 전용 operation이 거절되는지 Server Debug output으로 확인한다.
8. Client 재접속/Server 재시작 뒤 temporary options/overrides/spawns가 남지 않는지 확인한다.
```

자동 빌드만으로 command serialization, 서버 validation, definition query 경계는 확인할 수 있다. 실제 애니메이션·입력감·스킬 피해 체감과 두 PC 권한 검증은 정상 인게임에서 사용자가 최종 확인한다.
