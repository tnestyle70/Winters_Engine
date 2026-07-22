Session - F4를 서버 권위 통합 밸런스 튜너로 복구하고 이렐리아·비에고 피해 및 공식 부활 HUD를 완성한다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_BASIC_ATTACK_CADENCE_ANIMATION_TIMING_AUDIT_PLAN.md, 2026-07-18_RECALL_FX_RELEASE_GOLD_MANA_REGEN_RESOURCELESS_BAR_OFFSET_PLAN.md

## 1. 결정 기록

① 문제·제약: F4는 `StructureTuner` 1개만 열고, Claude의 통합 패널은 F10/숫자 9 뒤에 숨어 있다. 이렐리아·비에고 Q/W/E/R 쿨다운은 전부 3초 임시값이고 주요 AD/AP 계수는 0이며, 부활은 레벨과 무관하게 3초다.
② 순진한 해법의 실패: F4 구조물 창에 스킬 UI만 복제하면 기존 `ChampionTuner`와 서버 op가 이중화되고, 방어력을 전역 하향하면 정상인 저항 공식과 다른 17개 챔피언까지 훼손한다.
③ 메커니즘: F4는 기존 서버 권위 `ChampionTuner`를 단일 진입점으로 열고 카테고리 필터를 둔다. 피해는 정의 팩 공식, 부활은 레벨별 표+게임 시간 계수, HUD는 Snapshot→Client view state로만 전달한다.
④ 대조: Riot Data Dragon/게임 데이터의 이렐리아·비에고 기본 능력치·랭크별 쿨다운·피해 공식을 기준으로 삼는다. Winters의 월드 단위 이동속도/사거리는 원본 단위와 달라 이번에 강제 환산하지 않는다.
⑤ 대가: 라이브 스킬 오버라이드는 이번에도 슬롯별 단일 flat/cooldown 값이라 랭크별 영구값·계수는 JSON+리로드가 정본이다. 비에고 Q 패시브 2타/빙의 전체 재현은 별도 기능 슬라이스로 남긴다.

경계 선언: `Client ImGui -> Practice GameCommand -> Server GameRoom/GameSim -> Snapshot -> Client HUD`만 사용한다. Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.

## 예산

- 바닥 70%: F4 단일 진입점과 카테고리, 이렐리아·비에고 정의 수복, 레벨별 부활/연습 오버라이드, Snapshot 복제, 초상화 카운트다운, 런타임 HUD 골드 좌표, 자동 빌드/데이터 검증.
- 상한 30%: 장기 게임 시간 계수 경계값, 인게임 F4 조작성·초상화 가독성·골드 좌표의 수동 확인. 비에고 신규 패시브/빙의 기능과 전 챔피언 리밸런싱은 제외한다.

## 현재 코드 증거와 소유권

- `Scene_InGameImGui.cpp`: F4는 `m_bShowStructureTuner`와 `CStructureTunerPanel`만 토글한다. `CChampionTuner`는 F10/숫자 9의 레거시 그룹 안에서만 렌더된다.
- `ChampionTuner.cpp`: Champion/Item/Skill/Jungle/Structure/Minion 서버 op UI는 이미 한 창에 존재하므로 새 패널을 만들 이유가 없다.
- `DamageQueueSystem.cpp`: 커스텀 훅이 임시 flat 피해를 큐에 넣어도 일반 스킬은 `BuildSkillDamageRequest`로 정의 공식을 다시 해석한다. 따라서 이번 증상의 핵심은 공용 공식 우회가 아니라 F4 미연결과 0 계수/임시 정의다.
- `DamagePipeline.cpp`/`CombatFormula.cpp`: 방어력은 양수일 때 `100/(100+Armor)`로 정상 적용되고 레벨 성장식도 정상이다. 전역 Armor 하향은 하지 않는다.
- `GameRoom.cpp`: 사망 시 `spawnLoadout.respawnDelaySec` 3초를 레벨과 무관하게 복사한다.
- `hud_irelia_layout.json`: 실제 런타임 골드 텍스트는 `[695,137]`; `ActorHUDPanel.cpp`의 동일 좌표는 JSON 로드 실패 시 기본값이다. 상점 오버레이 숫자는 `UI_Manager.cpp`의 별도 경로다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

`m_bShowStructureTuner` 선언을 아래로 교체한다.

기존 코드:

```cpp
bool_t m_bShowStructureTuner = false;
```

아래로 교체:

```cpp
bool_t m_bShowBalanceTuner = false;
```

### 2-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

F4 토글과 렌더 블록을 기존 `ChampionTuner` 단일 창으로 교체한다. F10과 F4가 동시에 켜졌을 때 같은 ImGui 창을 두 번 렌더하지 않는다.

기존 코드:

```cpp
if (input.IsKeyPressed(VK_F4))
    m_bShowStructureTuner = !m_bShowStructureTuner;
```

아래로 교체:

```cpp
if (input.IsKeyPressed(VK_F4))
    m_bShowBalanceTuner = !m_bShowBalanceTuner;
```

기존 코드:

```cpp
if (m_bShowStructureTuner)
{
    WINTERS_PROFILE_SCOPE("UI::StructureTuner");
    UI::CStructureTunerPanel::Render(m_World, this);
}
```

아래로 교체:

```cpp
if (m_bShowBalanceTuner)
{
    WINTERS_PROFILE_SCOPE("UI::BalanceTuner");
    UI::CChampionTuner::Render(this);
}
```

레거시 그룹의 무조건 호출을 아래로 교체한다.

기존 코드:

```cpp
UI::CChampionTuner::Render(this);
```

아래로 교체:

```cpp
if (!m_bShowBalanceTuner)
    UI::CChampionTuner::Render(this);
```

### 2-3. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

`PracticeToolState`에 기본 `Skills` 카테고리와 18레벨 부활 초안을 추가한다.

```cpp
int balanceCategory = 0;
f32_t respawnSecondsByLevel[18] = {
    10.f, 10.f, 12.f, 12.f, 14.f, 16.f, 20.f, 25.f, 28.f,
    32.5f, 35.f, 37.5f, 40.f, 42.5f, 45.f, 47.5f, 50.f, 52.5f
};
```

창 상단에 `Skills / Champion / Items / Units & Structures / Respawn / Runtime / All` 콤보를 추가한다. 기존 CollapsingHeader는 각 카테고리 또는 All일 때만 렌더한다. F4의 기본 카테고리는 `Skills`다.

`Respawn` 카테고리에는 1~18 레벨 표, `Apply All`, `Restore Pack Values`를 추가한다. Apply는 레벨을 `slot`으로, 초를 `practiceValue`로 보내는 기존 `SendPracticeCommand`를 18회 재사용한다.

```cpp
SendPracticeCommand(
    pScene,
    state,
    ePracticeOperation::ApplyRespawnTimeOverride,
    state.respawnSecondsByLevel[level - 1],
    0u,
    static_cast<u8_t>(level));
```

### 2-4. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

이렐리아 기본 체력/성장과 MR을 현재 Riot 기준으로 교체한다.

```json
"baseHp": 630.0,
"hpPerLevel": 115.0,
"baseMr": 30.0
```

이렐리아 스킬의 `cooldownSec`/`manaCost` 단일값을 아래 랭크 배열로 교체하고 W 충전 피해 배율을 1→3으로 교체한다.

```json
"cooldownSecByRank": [10.0, 9.0, 8.0, 7.0, 6.0],
"manaCostByRank": [15.0, 15.0, 15.0, 15.0, 15.0]
```

```json
"cooldownSecByRank": [20.0, 18.0, 16.0, 14.0, 12.0],
"manaCostByRank": [70.0, 75.0, 80.0, 85.0, 90.0],
"damageScale": [1.0, 3.0]
```

```json
"cooldownSecByRank": [16.0, 14.5, 13.0, 11.5, 10.0],
"manaCostByRank": [50.0, 50.0, 50.0, 50.0, 50.0]
```

```json
"cooldownSecByRank": [125.0, 105.0, 85.0],
"manaCostByRank": [100.0, 100.0, 100.0]
```

비에고 성장 AD/AS와 Q/W/E/R 쿨다운 배열을 교체한다. Armor 34+4.6은 이미 기준과 일치하므로 유지한다.

```json
"adPerLevel": 3.5,
"attackSpeedPerLevel": 0.0275
```

```json
"cooldownSecByRank": [5.0, 4.5, 4.0, 3.5, 3.0]
```

```json
"cooldownSecByRank": [8.0, 8.0, 8.0, 8.0, 8.0]
```

```json
"cooldownSecByRank": [14.0, 12.0, 10.0, 8.0, 6.0]
```

```json
"cooldownSecByRank": [120.0, 100.0, 80.0]
```

### 2-5. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

이렐리아 피해 공식을 교체한다.

```json
"skill.irelia.q": {
  "type": "Physical",
  "flags": ["CanLifesteal", "OnHit"],
  "flatByRank": [5.0, 25.0, 45.0, 65.0, 85.0],
  "totalAdRatioByRank": [0.7, 0.7, 0.7, 0.7, 0.7]
}
```

```json
"skill.irelia.w": {
  "type": "Physical",
  "flatByRank": [10.0, 20.0, 30.0, 40.0, 50.0],
  "totalAdRatioByRank": [0.4, 0.4, 0.4, 0.4, 0.4],
  "apRatioByRank": [0.5, 0.5, 0.5, 0.5, 0.5]
}
```

```json
"skill.irelia.e": {
  "type": "Magic",
  "flatByRank": [70.0, 110.0, 150.0, 190.0, 230.0],
  "apRatioByRank": [1.0, 1.0, 1.0, 1.0, 1.0]
}
```

```json
"skill.irelia.r": {
  "type": "Magic",
  "flatByRank": [125.0, 200.0, 275.0],
  "apRatioByRank": [0.7, 0.7, 0.7]
}
```

비에고 피해 공식을 교체한다. Q active의 잘못된 `OnHit`는 제거한다.

```json
"skill.viego.q": {
  "type": "Physical",
  "flags": [],
  "flatByRank": [25.0, 40.0, 55.0, 70.0, 85.0],
  "totalAdRatioByRank": [0.7, 0.7, 0.7, 0.7, 0.7]
}
```

```json
"skill.viego.w": {
  "type": "Magic",
  "flatByRank": [80.0, 135.0, 190.0, 245.0, 300.0],
  "apRatioByRank": [1.0, 1.0, 1.0, 1.0, 1.0]
}
```

```json
"skill.viego.r": {
  "type": "Physical",
  "flatByRank": [0.0, 0.0, 0.0],
  "totalAdRatioByRank": [1.2, 1.2, 1.2],
  "targetMissingHpRatioByRank": [0.12, 0.16, 0.20]
}
```

`params.baseDamage`도 해당 flat 기준의 1랭크 값으로 맞춰, 파라미터 기반 보조 경로와 정의 공식이 서로 다른 숫자를 표시하지 않게 한다.

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h

레벨별 기본 표와 게임 시간 증가 계수를 해석하는 함수를 `SpawnLoadoutPolicyDef`에 추가한다. `respawnDelaySec`는 구버전 JSON 폴백으로 유지한다.

```cpp
static constexpr u8_t kRespawnLevelCount = 18u;
f32_t respawnDelaySecByLevel[kRespawnLevelCount]{};

f32_t ResolveRespawnDelaySec(u8_t level, f32_t gameTimeSec) const;
```

게임 시간 계수는 표준 Summoner's Rift 규칙을 사용한다: 15분 전 0%, 15~30분 30초마다 0.425%, 30~45분 30초마다 0.3%, 45~55분 30초마다 1.45%, 최대 50%.

### 2-7. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json

기존 3초 폴백 아래에 공식 base respawn 표를 추가한다.

```json
"respawnDelaySecByLevel": [
  10.0, 10.0, 12.0, 12.0, 14.0, 16.0, 20.0, 25.0, 28.0,
  32.5, 35.0, 37.5, 40.0, 42.5, 45.0, 47.5, 50.0, 52.5
]
```

### 2-8. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

`normalize_spawn_loadout`에서 18개 양수 배열을 검증하고 `append_spawn_object_cpp`에서 모든 원소를 생성한다. 배열이 없는 구버전 입력은 `respawnDelaySec`를 18회 복제한다.

```python
respawn_by_level = source.get("respawnDelaySecByLevel")
if respawn_by_level is None:
    respawn_by_level = [respawn_delay] * 18
if not isinstance(respawn_by_level, list) or len(respawn_by_level) != 18:
    fail("spawnLoadout.respawnDelaySecByLevel must contain 18 values")
```

### 2-9. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

`spawnLoadout` 알려진 키에 `respawnDelaySecByLevel`을 추가하고, Debug JSON 리로드 시 18개 숫자를 범위 검증하여 활성 Spawn pack에 복사한다.

### 2-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

기존 27 뒤에 append-only op를 추가한다.

```cpp
ApplyRespawnTimeOverride = 28,
ClearRespawnTimeOverrides = 29,
Count = 30,
```

### 2-11. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

동일한 숫자를 append한다.

```text
ApplyRespawnTimeOverride = 28,
ClearRespawnTimeOverrides = 29
```

### 2-12. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

연습 전용 레벨별 base override를 추가한다. 0은 팩 값을 의미한다.

```cpp
std::array<f32_t, 18> m_PracticeRespawnSecondsByLevel{};
```

### 2-13. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

op28은 `slot` 1~18, `practiceValue` 1~120초를 검증해 배열에 기록한다. op29는 배열을 0으로 초기화한다. 호스트/Debug/practice 게이트는 기존 `TryHandlePracticeControl` 공통 경계를 그대로 사용한다.

### 2-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

고정 3초 복사를 아래 의미로 교체한다.

```cpp
const u8_t level = champion.level > 0u ? champion.level : 1u;
const f32_t packDelay = spawnPolicy.ResolveRespawnDelaySec(
    level,
    static_cast<f32_t>(tc.tickIndex) / 30.f);
const f32_t practiceBase = m_PracticeRespawnSecondsByLevel[level - 1u];
const f32_t respawnDelay = practiceBase > 0.f
    ? spawnPolicy.ApplyGameTimeRespawnScale(practiceBase, gameTimeSec)
    : packDelay;
respawn.respawnDelay = respawnDelay;
respawn.respawnTimer = respawnDelay;
```

### 2-15. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`EntitySnapshot` 말미에 append한다.

```text
respawnRemainingSec:float;
respawnDurationSec:float;
```

### 2-16. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`RespawnComponent`가 있으면 pending 동안 남은 시간과 총 시간을 채우고 `CreateEntitySnapshot` 말미에 전달한다.

### 2-17. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

Champion snapshot에 `RespawnComponent`를 생성/갱신한다. 서버가 보낸 남은/총 시간만 복사하며 클라이언트가 자체 부활 판정을 만들지 않는다.

### 2-18. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/ActorHUDState.h

```cpp
bool_t bDead = false;
f32_t RespawnRemainingSec = 0.f;
f32_t RespawnDurationSec = 0.f;
```

### 2-19. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`HealthComponent`와 복제된 `RespawnComponent`를 읽어 HUD view state만 구성한다.

### 2-20. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/ActorHUDPanel.cpp

`TextForBind("respawn")`는 사망 중 `ceil(RespawnRemainingSec)`만 반환한다. 기본 레이아웃에 초상화 중앙 텍스트를 추가하고 골드 기본 좌표를 오른쪽 12, 아래 5 이동한다.

```cpp
addText("respawn.text", "respawn", 208.75f, 102.25f, 1.35f);
addText("gold.text", "gold", 707.f, 142.f, 0.96f);
```

### 2-21. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/hud_irelia_layout.json

실제 로드되는 authored layout에 동일한 respawn text를 추가하고 gold center를 `[707.00, 142.00]`로 바꾼다.

### 2-22. 생성 산출물

- `Shared/Schemas/Generated/cpp/Command_generated.h`
- `Shared/Schemas/Generated/go/Shared/Schema/PracticeOperation.go`
- `Shared/Schemas/Generated/cpp/Snapshot_generated.h`
- `Shared/Schemas/Generated/go/Shared/Schema/EntitySnapshot.go`
- `Shared/GameSim/Generated/ChampionGameData.generated.cpp`
- `Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json`
- `Data/LoL/ServerPrivate/Gameplay/ChampionGameplayDefs.json`
- `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`

수동 편집하지 않고 기존 코드젠으로 재생성한다. 새 소스 파일과 vcxproj 등록은 없다. Engine public header 변경 후 빌드 산출 SDK 동기화가 필요한지 `UpdateLib.bat` 경로를 확인한다.

## 3. 검증

예측:

- F4 한 번으로 `Practice Tool / Balance Lab`이 열리고 기본 `Skills` 카테고리에서 Q/W/E/R live flat/cooldown을 조절할 수 있다. F10 동시 활성화 시 창은 한 번만 그려진다.
- 코드젠 후 이렐리아 Q는 5~85 + 0.7 total AD, E/R은 Magic, 비에고 Q는 25~85 + 0.7 total AD, W는 Magic, R은 1.2 total AD + 12/16/20% missing HP로 생성된다.
- 레벨 6, 15분 전 사망은 16초; 레벨 18, 15분 전은 52.5초; 55분 이후 레벨 18은 78.75초다. F4 레벨 override는 같은 시간 계수를 적용한다.
- 죽은 로컬 챔피언의 원형 초상화 중앙에 남은 정수 초가 표시되고 부활 snapshot 뒤 사라진다.
- 실제 authored HUD의 gold 숫자는 reference 좌표 기준 `(695,137)`에서 `(707,142)`로 이동한다.
- Bot AI는 계속 GameCommand만 생산하며 이 작업은 AI truth를 직접 변경하지 않는다.
- schema/build codegen을 병렬 실행하면 충돌할 수 있으므로 모든 집계 빌드는 `/m:1`로 수행한다.

검증 명령:

```powershell
python Tools/ChampionData/build_champion_game_data.py --root . --check
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
git diff --check
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Server/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
msbuild Client/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
```

가능하면 기존 SimLab 실행 파일/타깃도 빌드·실행하여 damage/definition 회귀를 확인한다.

미검증:

- 계획 시점에는 실제 F5 서버 세션에서 F4 클릭, 사망 초상화, 골드 좌표를 눈으로 확인하지 않았다.
- 비에고 Q 패시브 2타·빙의 대상 스킬의 완전한 공식은 이번 범위 밖이다.

확인 필요:

- Engine public `ActorHUDState.h` 변경 후 Client가 어느 SDK 복사본을 include하는지 빌드 결과로 확인한다.
- 기존 다른 Codex의 Yone/AI/공속 변경이 dirty 상태이므로 관련 파일을 되돌리거나 재포맷하지 않는다.

## 서브 에이전트 비평

비평 주체: `Bernoulli` (`/root/plan_critic`), read-only 코드·데이터·빌드 경계 대조 완료.

### P0 판정

1. **수용 — 충전 피해 배율 소실**: `IreliaGameSim.cpp`/`ViegoGameSim.cpp`에서 임시 `damage *= damageScale`를 수행해도 `DamageQueueSystem.cpp`가 `request = resolved`로 공용 공식을 다시 만들면서 값이 사라진다. `DamageRequest`에 `skillDamageScale = 1.f`를 추가하고, 재해석 직전에 보존한 뒤 공식의 flat/AD/bonus AD/AP/target HP 계수에만 곱한다. 아이템·온힛 후속 피해는 배율 대상에서 제외한다.
2. **수용 — 부활 API 컴파일 불완전**: 초안의 `ResolveRespawnDelaySec`/`ApplyGameTimeRespawnScale` 혼합과 미정의 `gameTimeSec`를 폐기한다. `SpawnLoadoutPolicyDef`는 레벨별 base 선택만 책임지고, `GameRoom.cpp`의 익명 네임스페이스 함수가 게임 시간 증가율과 최종 시간을 계산한다.
3. **수용 — 연습용 override 누출**: 정상 부활 경로는 반드시 `m_bPracticeModeEnabled`일 때만 override를 읽는다. 연습 모드 비활성화와 `ResetMatchStateLocked()`에서 18개 배열을 모두 0으로 초기화한다.
4. **수용 — codegen 순서 누락**: 생성 명령을 먼저 실행하고, 이후 동일 명령의 `--check`를 실행한다. FlatBuffers는 GameSim 프로젝트 pre-build codegen으로 생성하되 schema 산출물 diff도 검증한다.

### P1 판정

1. **부분 수용 — 기준 데이터 고정**: 공개 근거는 Riot Data Dragon 16.14.1과 Riot 패치 노트를 기록한다. 단, Data Dragon의 Viego AD 성장 0은 원본 game-file 수치가 누락된 projection이므로 채택하지 않는다. 현재 CommunityDragon Riot raw character record의 `mAttackDamagePerLevel = 3.5`를 채택하고 이 예외를 결과서에 명시한다.
2. **수용 — deterministic 검증 부족**: 빌드만으로 닫지 않는다. SimLab의 `FormulaData` probe, 별도 F4 balance contract 검사, Irelia/Viego 랭크별 flat·계수, 충전 배율 보존, 부활 레벨/시간 경계, practice reset/guard, Snapshot append, F4 단일 렌더를 필수 검증한다.
3. **수용 — 카테고리 API 불완전**: `ChampionTuner.h`에 `eBalanceTunerCategory`와 `Open(category)`를 추가한다. F4가 열릴 때마다 `Skills`로 명시적으로 연다. 카테고리 매핑은 Skills(스킬 효과/밸런스), Champion(챔피언 스탯), Items(아이템/상점), Units & Structures(미니언/정글/구조물), Respawn(부활), Runtime(시뮬레이션/스폰/플레이어/텔레포트), All(전체)로 고정한다.

### P2 판정

1. **수용 — 잔재 제거**: F4 교체와 함께 `StructureTunerPanel.h` include, structure profiler scope, 오래된 F4 Structure 주석 및 `ChampionTuner.h`의 낡은 설명을 정리한다. `StructureTunerPanel` 소스 자체는 다른 호출 가능성을 위해 삭제하지 않는다.
2. **수용 — HUD 시각 QA**: 16:9 authored layout에서 생존/사망/골드 상태를 확인한다. 자동 실행 환경에서 캡처가 불가능하면 빌드·layout contract까지 완료하고 수동 F5 캡처만 `미검증`으로 결과서에 정확히 남긴다.

### 구현 후 재비평

동일 비평 주체 `Bernoulli`가 최종 구현을 다시 read-only 대조했다. P0는 없었고 아래 P1 두 건을 모두 수용해 RESULT 전에 추가 교정한다.

1. **수용 — 문자열 중심 계약 검사를 실제 C++ assertion으로 승격**: `SpawnLoadoutPolicyDef`가 base 선택·practice guard·게임 시간 증가까지 실제 서버 호출 API로 소유하게 하고 SimLab이 같은 함수를 15/30/45/55분 경계에서 실행한다. `FormulaData` probe는 `CDamageQueueSystem`을 통해 Irelia W의 flat/AD/AP 충전 배율과 Viego R의 missing-HP 계수 재해석을 실제 체력 감소로 검증한다. FlatBuffers respawn 필드도 생성→검증→읽기 round-trip을 실행한다.
2. **수용 — Chrono Break의 미래 override 누수**: `RoomKeyframe`에 18레벨 practice respawn 배열을 포함하고 capture/restore에서 복사한다. 문자열 계약은 두 방향 배선을 확인하며 Server Debug/Release 빌드로 실제 타입·접근 경계를 검증한다.

이 교정으로 초안의 “시간 계산은 GameRoom 익명 함수” 책임은 폐기하고, 서버와 SimLab이 동일한 `SpawnLoadoutPolicyDef::ResolveRespawnDelaySec` 구현을 호출하도록 변경한다.

## 비평 후 정정된 구현 계약

### 피해 배율 전달

`Shared/GameSim/Components/DamageRequestComponent.h`의 공식 계수 앞에 아래 필드를 추가한다.

```cpp
f32_t skillDamageScale = 1.f;
```

Irelia/Viego의 피해 enqueue helper는 이 값을 받으며, 충전 스킬은 로컬 flat을 미리 곱하지 않고 `request.skillDamageScale`에 저장한다. `DamageQueueSystem::ApplyDataDrivenSkillFormula`는 `request = resolved` 직전에 0 이상 유한값으로 정규화한 배율을 보존하고, 성공적으로 정의 공식을 만든 경우에만 아래 공식 항목에 곱한다.

```cpp
resolved.flatAmount *= skillDamageScale;
resolved.adRatioOverride *= skillDamageScale;
resolved.bonusAdRatioOverride *= skillDamageScale;
resolved.apRatioOverride *= skillDamageScale;
resolved.targetMaxHpRatioOverride *= skillDamageScale;
resolved.targetMissingHpRatioOverride *= skillDamageScale;
resolved.skillDamageScale = skillDamageScale;
request = resolved;
```

### 부활 계산 책임

`SpawnLoadoutPolicyDef`에는 `kRespawnLevelCount`, `respawnDelaySecByLevel[18]`, `ResolveBaseRespawnDelaySec(level)`, `ResolveRespawnTimeIncreaseFactor(gameTimeSec)`, `ResolveRespawnDelaySec(level, gameTimeSec, practiceEnabled, practiceSecondsByLevel)`를 둔다. 레벨은 1~18로 clamp하고 배열 값이 0 이하면 구버전 `respawnDelaySec` fallback을 사용한다.

`GameRoom.cpp`는 30 Hz `tc.tickIndex / 30.f`를 게임 초로 넘긴다. 표준 Summoner's Rift 증가율은 15분 미만 0, 15~30분 30초 tick당 0.425%, 30~45분은 12.75%에서 이어서 0.3%, 45~55분은 21.75%에서 이어서 1.45%, 이후 50% cap으로 계산한다. death latch 시점에만 최종 시간을 고정해 timer/duration이 같은 값을 갖게 한다.

### 카테고리와 단일 진입점

`Client/Public/UI/ChampionTuner.h`에 아래 API를 추가한다.

```cpp
enum class eBalanceTunerCategory : u8_t
{
    Skills = 0u,
    Champion,
    Items,
    UnitsAndStructures,
    Respawn,
    Runtime,
    All,
};

class CChampionTuner
{
public:
    static void Open(eBalanceTunerCategory category = eBalanceTunerCategory::Skills);
    static void Render(CScene_InGame* pScene);
};
```

F4 false→true 전환에서 `CChampionTuner::Open(eBalanceTunerCategory::Skills)`를 호출한다. F10 legacy 그룹은 F4 창이 닫혀 있을 때만 같은 패널을 그려 중복 ImGui window를 만들지 않는다.

### 생성·검증 순서

```powershell
python Tools/ChampionData/build_champion_game_data.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/ChampionData/build_champion_game_data.py --root . --check
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/LoLData/Test-F4BalanceContracts.py --root .
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'Tools/Bin/Debug/SimLab.exe' 1 42
```

이 정정 계약은 앞선 초안과 충돌할 경우 우선한다. 이제 비평 게이트가 닫혔으므로 위 범위에서만 구현을 시작한다.

### 새 파일: C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py

```python
#!/usr/bin/env python3
import argparse
import json
import math
from pathlib import Path


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_close(actual: float, expected: float, message: str) -> None:
    require(math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-5),
            f"{message}: expected={expected} actual={actual}")


def champion_by_name(document, name: str):
    return next(row for row in document["champions"] if row["champion"] == name)


def skill_by_slot(champion, slot: int):
    return next(row for row in champion["skills"] if row["slot"] == slot)


def effect_by_key(document, key: str):
    return next(row for row in document["skillEffects"] if row["key"] == key)


def require_list(actual, expected, message: str) -> None:
    require(len(actual) == len(expected), f"{message}: length mismatch")
    for index, (actual_value, expected_value) in enumerate(zip(actual, expected)):
        require_close(float(actual_value), float(expected_value), f"{message}[{index}]")


def respawn_factor(game_time_sec: float) -> float:
    if game_time_sec <= 900.0:
        return 0.0
    if game_time_sec <= 1800.0:
        return min(0.1275, math.floor((game_time_sec - 900.0) / 30.0) * 0.00425)
    if game_time_sec <= 2700.0:
        return min(0.2175, 0.1275 + math.floor((game_time_sec - 1800.0) / 30.0) * 0.003)
    return min(0.5, 0.2175 + math.floor((game_time_sec - 2700.0) / 30.0) * 0.0145)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    champions = load_json(root / "Data/Gameplay/ChampionGameData/champions.json")
    effects = load_json(root / "Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json")
    spawn = load_json(root / "Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json")
    layout = load_json(root / "Client/Bin/Resource/UI/hud_irelia_layout.json")

    irelia = champion_by_name(champions, "IRELIA")
    viego = champion_by_name(champions, "VIEGO")
    require_close(irelia["stats"]["baseHp"], 630.0, "Irelia base HP")
    require_close(irelia["stats"]["hpPerLevel"], 115.0, "Irelia HP growth")
    require_close(irelia["stats"]["baseMr"], 30.0, "Irelia MR")
    require_close(viego["stats"]["adPerLevel"], 3.5, "Viego AD growth")
    require_close(viego["stats"]["attackSpeedPerLevel"], 0.0275, "Viego AS growth")

    expected_cooldowns = {
        ("IRELIA", 1): [10, 9, 8, 7, 6],
        ("IRELIA", 2): [20, 18, 16, 14, 12],
        ("IRELIA", 3): [16, 14.5, 13, 11.5, 10],
        ("IRELIA", 4): [125, 105, 85],
        ("VIEGO", 1): [5, 4.5, 4, 3.5, 3],
        ("VIEGO", 2): [8, 8, 8, 8, 8],
        ("VIEGO", 3): [14, 12, 10, 8, 6],
        ("VIEGO", 4): [120, 100, 80],
    }
    for (champion_name, slot), expected in expected_cooldowns.items():
        champion = irelia if champion_name == "IRELIA" else viego
        require_list(skill_by_slot(champion, slot)["cooldownSecByRank"], expected,
                     f"{champion_name} slot {slot} cooldown")

    expected_effects = {
        "skill.irelia.q": ("Physical", [5, 25, 45, 65, 85], [0.7] * 5, [0.0] * 5, [0.0] * 5),
        "skill.irelia.w": ("Physical", [10, 20, 30, 40, 50], [0.4] * 5, [0.5] * 5, [0.0] * 5),
        "skill.irelia.e": ("Magic", [70, 110, 150, 190, 230], [0.0] * 5, [1.0] * 5, [0.0] * 5),
        "skill.irelia.r": ("Magic", [125, 200, 275], [0.0] * 3, [0.7] * 3, [0.0] * 3),
        "skill.viego.q": ("Physical", [25, 40, 55, 70, 85], [0.7] * 5, [0.0] * 5, [0.0] * 5),
        "skill.viego.w": ("Magic", [80, 135, 190, 245, 300], [0.0] * 5, [1.0] * 5, [0.0] * 5),
        "skill.viego.r": ("Physical", [0, 0, 0], [1.2] * 3, [0.0] * 3, [0.12, 0.16, 0.20]),
    }
    for key, (damage_type, flat, total_ad, ap, missing_hp) in expected_effects.items():
        damage = effect_by_key(effects, key)["damage"]
        require(damage["type"] == damage_type, f"{key} damage type")
        require_list(damage["flatByRank"], flat, f"{key} flat")
        require_list(damage["totalAdRatioByRank"], total_ad, f"{key} total AD")
        require_list(damage["apRatioByRank"], ap, f"{key} AP")
        require_list(damage["targetMissingHpRatioByRank"], missing_hp, f"{key} missing HP")

    respawn = spawn["spawnLoadout"]["respawnDelaySecByLevel"]
    require_list(respawn, [10, 10, 12, 12, 14, 16, 20, 25, 28, 32.5, 35, 37.5,
                           40, 42.5, 45, 47.5, 50, 52.5], "respawn table")
    for second, expected in ((900, 0.0), (930, 0.00425), (1800, 0.1275),
                             (1830, 0.1305), (2700, 0.2175), (2730, 0.232),
                             (3300, 0.5)):
        require_close(respawn_factor(second), expected, f"respawn factor {second}s")
    require_close(respawn[17] * (1.0 + respawn_factor(3300)), 78.75,
                  "level 18 capped respawn")

    text_rows = {row["ID"]: row for row in layout["texts"]}
    require_list(text_rows["gold.text"]["center"], [707, 142], "gold center")
    require_list(text_rows["respawn.text"]["center"], [208.75, 102.25], "respawn center")
    require(text_rows["respawn.text"]["bind"] == "respawn", "respawn bind")

    scene = (root / "Client/Private/Scene/Scene_InGameImGui.cpp").read_text(encoding="utf-8")
    require("m_bShowBalanceTuner" in scene, "F4 balance tuner state")
    require("CChampionTuner::Open(UI::eBalanceTunerCategory::Skills)" in scene,
            "F4 defaults to Skills")
    require("CStructureTunerPanel::Render" not in scene, "legacy F4 structure renderer removed")
    require(scene.count("CChampionTuner::Render(this)") == 2,
            "exactly guarded F4 and legacy render call sites")

    damage_queue = (root / "Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp").read_text(encoding="utf-8")
    require("resolved.flatAmount *= skillDamageScale" in damage_queue,
            "charge scale applies after formula resolution")
    require("resolved.targetMissingHpRatioOverride *= skillDamageScale" in damage_queue,
            "charge scale covers target HP formula")
    spawn_policy = (root / "Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h").read_text(
        encoding="utf-8")
    require("bPracticeModeEnabled && pPracticeSecondsByLevel" in spawn_policy,
            "practice respawn guard")
    game_room = (root / "Server/Private/Game/GameRoom.cpp").read_text(encoding="utf-8")
    require("m_PracticeRespawnSecondsByLevel.data()" in game_room,
            "server uses shared respawn policy")
    require("m_PracticeRespawnSecondsByLevel.fill(0.f)" in game_room,
            "match reset clears respawn overrides")
    room_commands = (root / "Server/Private/Game/GameRoomCommands.cpp").read_text(
        encoding="utf-8")
    require(
        "keyframe.practiceRespawnSecondsByLevel = m_PracticeRespawnSecondsByLevel" in
        room_commands,
        "chrono capture preserves respawn overrides")
    require(
        "pKeyframe->practiceRespawnSecondsByLevel" in room_commands,
        "chrono restore preserves respawn overrides")

    snapshot_schema = (root / "Shared/Schemas/Snapshot.fbs").read_text(encoding="utf-8")
    require("respawnRemainingSec:float" in snapshot_schema, "snapshot remaining field")
    require("respawnDurationSec:float" in snapshot_schema, "snapshot duration field")

    print("[F4BalanceContracts] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

---

## 후속 세션 — 본질형 F4 / AI Debug / UI Manager

Session - 2026-07-19 F4 Essentials UX

목표: F4를 이렐리아 BA·Q·W·E·R, 성장 수치, 미니언 공격력만 남긴 3면 편집기로 교체하고 AI Debug/UI Manager도 한 번에 한 작업만 보이게 축소한다.

범위: Client/Engine ImGui와 역할별 미니언 공격력 practice 명령·서버 상태·저장/복원·계약 검증. WFX와 release 정의 팩 수치는 건드리지 않는다.

검증: 읽기 전용 서브 에이전트 비평 후 구현하고, schema codegen·계약 테스트·diff check·Debug/Release Engine/GameSim/Server/Client 빌드로 닫는다.

### ① 요청 해석

- F4 첫 화면은 `Champion Damage / Growth / Minions` 세 탭만 노출한다. 연결 설명, 세션 on/off, target netId, raw slot/parameter 표, Items/Runtime/Respawn/Jungle/Structure는 기본 F4에서 제거한다.
- `Champion Damage`의 BA는 현재 조종 챔피언 `BaseAd`, Q/W/E/R은 self 대상 `DamageFlatOverride`다. ServerPrivate 원본 QWER 수치는 Client가 소유하지 않으므로 비활성 행은 숫자를 꾸며내지 않고 `Pack value`로 표시한다.
- `Growth`는 HP/Mana/AD/AP/Armor/MR/Attack Speed의 Base와 Per Level만 2열로 편집한다.
- `Minions`는 Melee/Ranged/Siege/Super 선택 + Attack Damage 하나만 편집·저장한다. 역할별 override는 서버 권위이며 기존/신규 미니언과 Chrono에 동일하게 반영한다.
- F9는 AI Debug만, F8은 UI Manager 한 창만 연다. WFX(F7)는 변경하지 않는다.

### ② 현재 코드 증거와 책임 경계

- `Client/Private/UI/ChampionTuner.cpp::Render`가 7개 카테고리와 raw override/runtime/shop/respawn을 한 창에 함께 소유해 스크린샷의 중복·과밀을 만든다.
- `Client/Private/Scene/Scene_InGameImGui.cpp`의 F9가 `m_bShowAIDebug`와 `m_bShowUITuner`를 함께 바꾸고, F8 UI 경로도 UI Manager·Status Panel·Minimap 세 창을 동시에 그린다.
- `DamageQueueSystem.cpp`는 BasicAttack에 skill flat override를 적용하지 않는다. 따라서 BA는 기존 `ApplyChampionStatOverride(BaseAd)` 경로를 재사용한다.
- `GameRoomSpawn.cpp::SpawnServerMinion`이 역할 정의와 시간 성장으로 `MinionStateComponent::attackDamage`를 결정하므로, 미니언 override의 진실은 `GameRoom`에 있어야 한다.
- 권위 흐름은 `F4 Client -> PracticeControl command -> Server GameRoom -> existing/new MinionStateComponent -> snapshot/gameplay`이며 Client ECS 직접 변경은 금지한다.

### ③ 구현 결정과 예산

- 바닥 작업 70%: F4 3탭, AI/UI Manager 축소, 미니언 서버 명령, JSON draft 저장, 계약/빌드.
- 천장 작업 30%: 범용 편집기 확장 대신 “한 창·한 목적·한 저장” 규칙을 UI 진입 구조에 고정하고, 이후 항목은 별도 focused lab으로만 추가한다.
- 기존 대형 helper/backend는 이번 세션에서 동작을 지우지 않고 기본 Render에서만 숨긴다. 새 추상화나 두 번째 데이터 owner는 만들지 않는다.

### ④ 파일별 적용 계획

#### `Client/Public/UI/ChampionTuner.h`

기존 `eBalanceTunerCategory` 전체를 아래로 교체한다.

```cpp
enum class eBalanceTunerCategory : u8_t
{
    ChampionDamage = 0u,
    Growth,
    Minions,
};
```

#### `Client/Private/Scene/Scene_InGameImGui.cpp`

기존 F9 블록과 F8 UI render 블록을 아래로 교체한다.

```cpp
if (input.IsKeyPressed(VK_F9))
    m_bShowAIDebug = !m_bShowAIDebug;
if (input.IsKeyPressed(VK_F8))
    m_bShowUITuner = !m_bShowUITuner;
```

```cpp
if (m_bShowUITuner)
{
    WINTERS_PROFILE_SCOPE("UI::Tuner");
    CGameInstance::Get()->UI_OnImGui_Tuner();
}
```

F4 open anchor는 아래 한 카테고리만 지정한다.

```cpp
UI::CChampionTuner::Open(UI::eBalanceTunerCategory::ChampionDamage);
```

#### `Client/Private/UI/ChampionTuner.cpp`

`PracticeToolState`의 constructor seed 두 행을 삭제하고 아래 draft를 추가한다.

```cpp
f32_t skillDamageDraft[4] = { -1.f, -1.f, -1.f, -1.f };
int minionRole = 0;
f32_t minionAttackDamageDraft[4] = { 40.f, 60.f, 40.f, 100.f };
bool_t minionAttackDamageEnabled[4] = { false, false, false, false };
int balanceCategory = static_cast<int>(UI::eBalanceTunerCategory::ChampionDamage);
```

`LoadOverrides`/`SaveOverrides`의 version 1 optional field로 아래 배열을 읽고 쓴다. 기존 JSON 호환은 유지한다.

```json
"minionAttackDamage": [
  { "role": 0, "value": 40.0 }
]
```

`CChampionTuner::Render` 전체 body는 다음 화면 계약으로 교체한다.

```cpp
// 640x520 한 창. 연결 불가일 때만 한 줄 경고.
// 탭: Champion Damage / Growth / Minions.
// Champion Damage: BA(BaseAd) + Q/W/E/R(DamageFlatOverride) 5행.
// Growth: HP/Mana/AD/AP/Armor/MR/Attack Speed x Base/Per Level 7행.
// Minions: role combo + Use custom value + Attack Damage 1행.
// 하단: Save & Apply / Restore This Category / 짧은 status 한 줄.
// Save & Apply는 SetEnabled(true), JSON save, self skill/stat clear+apply,
// role minion clear+apply를 순서대로 server command로 보낸다.
```

구현 시 위 주석을 남기는 것이 아니라 기존 `SendPracticeCommand`, `UpsertStatOverrideRow`, `BuildDefaultChampionStatsDef`, `SaveOverrides`를 직접 호출하는 실제 ImGui code로 교체한다. raw `RenderOverrideTable`과 cooldown/runtime/shop/spawn/respawn/jungle/structure 섹션은 새 body에서 호출하지 않는다.

#### `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`

`ePracticeOperation` 말미를 append-only로 아래처럼 교체하고 역할별 계약을 바로 아래 추가한다.

```cpp
ApplyRespawnTimeOverride = 28,
ClearRespawnTimeOverrides = 29,
ApplyMinionStatOverride = 30,
ClearMinionStatOverrides = 31,
Count = 32,
```

```cpp
enum class eMinionStatOverrideId : uint8_t
{
    None = 0,
    AttackDamage = 1,
    Count = 2,
};
```

`ApplyMinionStatOverride`는 `flags=role(0..3)`, `slot=AttackDamage`, `value=최종 공격력`이다.

#### `Shared/Schemas/Command.fbs` 및 생성물

`PracticeOperation` 말미에 동일한 30/31 값을 append하고 `Shared/Schemas/run_codegen.bat`로 C++/Go 생성물을 동기화한다.

```fbs
ApplyRespawnTimeOverride = 28,
ClearRespawnTimeOverrides = 29,
ApplyMinionStatOverride = 30,
ClearMinionStatOverrides = 31
```

#### `Server/Public/Game/GameRoom.h`

practice room state와 `RoomKeyframe`에 같은 배열을 각각 추가한다.

```cpp
static constexpr u32_t kPracticeMinionRoleCount = 4u;
std::array<f32_t, kPracticeMinionRoleCount> m_PracticeMinionAttackDamageByRole{};
```

```cpp
std::array<f32_t, kPracticeMinionRoleCount>
    practiceMinionAttackDamageByRole{};
```

#### `Server/Private/Game/GameRoomSpawn.cpp`

기존 `state.attackDamage = combat.attackDamage * timeGrowth;`를 아래로 교체한다.

```cpp
const f32_t practiceAttackDamage = roleType < kPracticeMinionRoleCount
    ? m_PracticeMinionAttackDamageByRole[roleType]
    : 0.f;
state.attackDamage = m_bPracticeModeEnabled && practiceAttackDamage > 0.f
    ? practiceAttackDamage
    : combat.attackDamage * timeGrowth;
```

#### `Server/Private/Game/GameRoom.cpp`, `Server/Private/Game/GameRoomCommands.cpp`

- match reset와 practice disable에서 배열을 `fill(0.f)`한다.
- keyframe capture/restore에 배열을 복사한다.
- `ApplyMinionStatOverride`는 role/stat/value를 검증하고 같은 role의 모든 기존 `MinionStateComponent::attackDamage`를 즉시 최종값으로 바꾼다.
- `ClearMinionStatOverrides`는 배열을 비우고 active spawn pack의 role attackDamage × 현재 time growth로 기존 미니언을 복원한다.

#### `Client/Private/UI/AIDebugPanel.cpp`

기존 `CAIDebugPanel::Render` body를 아래 정보 구조만 남긴 540x430 창으로 교체한다.

```text
Bot selector
Now: State | Intent | Action | Target, HP
Scores: Fight | Farm | Structure | Retreat
Why: Block reason | Last command/slot | Executor state/reason
Force: Safe | Farm | Fight | Structure | Retreat
Core Tuning (collapsed): Retreat HP, Reengage HP, Champion Margin,
                         Turret Danger, Skill Cast Interval, Reset
Latest Decision (collapsed): 최신 trace 한 건
```

All Bots 표, shadow logits, skill rule 표, 전체 decision trace 표, Chrono 표, Server Minions 표는 새 body에서 렌더하지 않는다. 서버 debug component/trace 수집 자체는 삭제하지 않는다.

#### `Engine/Private/Manager/UI/UI_Manager.cpp`

`OnImGui_Tuner` body를 `HUD / Health Bars / Cursor` 탭으로 교체한다.

```text
HUD: Show HUD, Reference, Reference Alpha, Actor HUD Layout
Health Bars: Show, Target(Champion/Minion/Structure), Width, Height, Y Offset,
             Structure일 때만 Screen X/Y
Cursor: Show Cursor, Size
```

Lua host, texture loaded/fallback 상태, shop/status 설명, damage floater 시험 버튼은 기본 창에서 제거한다.

#### `Engine/Private/Manager/UI/ActorHUDPanel.cpp`

`DrawLayoutTunerImGui` body는 `Image / Text` 선택, 대상 combo, Position, Size 또는 Font Scale, `Save Layout`만 남긴다. raw sprite/image/bind/shape/UV와 copy JSON 버튼은 숨긴다.

#### `Tools/LoLData/Test-F4BalanceContracts.py`

다음을 정적 계약으로 추가한다.

```python
require('"Champion Damage"' in tuner, "essential damage tab")
require('"Growth"' in tuner and '"Minions"' in tuner, "three essential tabs")
require("RenderOverrideTable(pScene, state)" not in render_body, "raw table hidden")
require("m_bShowUITuner = m_bShowAIDebug" not in scene, "F9 is AI only")
require("UI_OnImGui_StatusPanelLayoutTuner" not in ui_tuner_block, "F8 one window")
require("ApplyMinionStatOverride = 30" in command_schema, "schema operation")
require("practiceMinionAttackDamageByRole" in room_commands, "chrono preservation")
```

### ⑤ 검증·핸드오프

각 명령 전 예측을 기록한다: codegen은 operation 32개와 Go enum을 만들고, 계약 테스트는 본질형 UI/권위 경로를 PASS, `git diff --check`는 새 whitespace error 0, 각 빌드는 error 0이어야 한다.

```powershell
cmd /c Shared\Schemas\run_codegen.bat
python Tools/LoLData/Test-F4BalanceContracts.py --root .
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine/Include/Engine.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1
```

수동 확인은 F4 한 창/세 탭, 이렐리아 BA·QWER 반영, 역할별 기존/신규 미니언 공격력, F9 한 창, F8 한 창, HUD gold text 이동 저장이다. 실행 환경에서 직접 F5 플레이를 못 하면 RESULT에 자동 검증과 수동 미확인 경계를 분리해 적는다.

### 서브 에이전트 비평 게이트

구현 전 Bernoulli에게 이 후속 세션만 읽기 전용으로 검토시킨다. P0/P1은 계획에 반영한 뒤에만 source edit을 시작하며, 수용/기각 근거를 이 절 아래에 기록한다.

#### Bernoulli 비평 결과와 처분

- P0 수용: `CChampionTuner::Open` 기본 인자도 `ChampionDamage`로 교체한다.
- P0 수용: 기존 `Test-F4BalanceContracts.py`의 `Open(...Skills)` 계약은 추가가 아니라 `Open(...ChampionDamage)`로 교체한다.
- P1 수용: `ClearPracticeMinionAttackDamageOverrides()` private helper가 배열 초기화와 기존 미니언의 pack×현재 성장 복원을 함께 담당한다. `ClearMinionStatOverrides`와 practice disable이 모두 이 helper를 호출한다.
- P1 수용: 새 Save & Apply allowlist는 `BaseAd + 14개 Growth stat + QWER DamageFlatOverride + 활성 미니언 role AD`뿐이다. legacy cooldown/item/move/range/raw effect row는 JSON에서 읽혀도 새 화면에서 전송하지 않는다.
- P1 수용: 화면 라벨은 `Base AD`, `Q/W/E/R Flat Damage`로 쓴다. BA와 Growth의 Base AD는 동일한 `StatOverrideRow(BaseAd)` 하나를 읽고 갱신한다.
- P1 수용: Render 교체는 구현 직전 최신 dirty anchor를 다시 확인했고, source에서는 전체 function body를 실제 ImGui 코드로 교체한다. 이 문서의 이전 주석형 블록은 방향 요약이며, 아래 실행 계약을 빠뜨릴 수 없다.

```cpp
// ChampionTuner 실행 계약
// 1) BeginTabBar + 정확히 3 BeginTabItem.
// 2) Base AD input -> UpsertStatOverrideRow(BaseAd index, value, baseline).
// 3) QWER checkbox off=-1, on>=0; InputFloat label은 Flat Damage.
// 4) Growth 7행은 kStatOptions의 exact Base/PerLevel id pair만 사용.
// 5) Minion input edit은 해당 role enabled=true.
// 6) Save & Apply는 SyncEssentialSkillRows -> SaveOverrides 성공 후에만
//    SetEnabled, clear/apply allowlisted skill/stat/minion 명령을 self로 전송.
// 7) Restore This Category는 현재 탭 allowlist만 local/server에서 복원.
```

- P1 수용: 정적 문자열 검사 외에 `PracticeMinionAttackDamagePolicy` 결정적 probe를 SimLab에 추가해 valid/invalid apply, practice on/off resolve, clear, copy capture/restore를 실행한다. GameRoom 기존/신규 미니언 wiring은 계약 검사와 Server 빌드로 함께 고정하고, 실제 F5 타격 QA는 RESULT에 별도 경계로 남긴다.
- P1 수용: 전체 `run_codegen.bat` 대신 아래 Command 전용 codegen만 직렬 실행해 dirty Snapshot 생성물을 덮지 않는다.

```powershell
Tools/Bin/flatc.exe --cpp --scoped-enums --no-warnings -o Shared/Schemas/Generated/cpp Shared/Schemas/Command.fbs
Tools/Bin/flatc.exe --go --no-warnings -o Shared/Schemas/Generated/go Shared/Schemas/Command.fbs
```

- P2 수용: spawn/clear가 공유할 최종 공격력 계산은 아래 새 pure policy 한 곳으로 둔다.

새 파일: `Shared/GameSim/Definitions/PracticeMinionAttackDamagePolicy.h`

```cpp
#pragma once

#include "WintersTypes.h"

#include <array>
#include <cmath>

struct PracticeMinionAttackDamagePolicy
{
    static constexpr u8_t kRoleCount = 4u;
    static constexpr f32_t kMaximumValue = 1000000.f;

    std::array<f32_t, kRoleCount> values{};

    bool_t Apply(u8_t role, f32_t value)
    {
        if (role >= kRoleCount || !std::isfinite(value) || value < 0.f ||
            value > kMaximumValue)
        {
            return false;
        }
        values[role] = value;
        return true;
    }

    void Clear()
    {
        values.fill(0.f);
    }

    f32_t Resolve(
        u8_t role,
        f32_t packAttackDamage,
        f32_t timeGrowth,
        bool_t bPracticeEnabled) const
    {
        if (bPracticeEnabled && role < kRoleCount && values[role] > 0.f)
            return values[role];
        return packAttackDamage * timeGrowth;
    }
};
```

`GameRoom`은 이 policy를 단일 owner로 보유하고 keyframe에는 `values`만 복사한다. SimLab은 이 실제 header를 include해 검증한다.

- P2 수용: Release GameSim 빌드와 `Services`의 `go test ./...`를 검증 목록에 추가한다.
- P2 수용: AI Force 영역에는 명령 불가 시 `Server-authoritative practice session required` 한 줄만 표시한다. F4 Save & Apply가 practice를 자동 활성화한다.

#### 비평 후 최종 소스 편집 게이트

P0/P1은 모두 위와 같이 계획에 반영했다. Command 전용 codegen을 사용하고, 각 source patch 직전 해당 function/enum anchor를 다시 읽으며, unrelated dirty diff는 보존한 상태에서 구현을 시작한다.

## 교정 세션 — 2026-07-19 JSON 밸런스 에디터 / 즉시 Hot Load

### ① 실패 판정과 교정 목표

직전 후속 구현은 `본질만`을 UI 노이즈 제거가 아니라 편집 필드 제거로 잘못 해석했다. 현재 F4가 `Current champion`, Base AD, QWER 단일 flat override, 성장치, 미니언 공격력만 노출하므로 사용자의 실제 목적을 충족하지 못한다. 에이전트 비평은 이 축소를 요구하지 않았으며, 범위 판단 책임은 구현 세션에 있다.

교정 완료 조건은 다음 한 문장이다.

> F4에서 지원 챔피언을 이름으로 선택하고 챔피언 기본/성장 스탯, QWER 랭크별 피해 계수와 쿨타임, 역할별 미니언 체력/공격력, 타워 체력/공격력을 수정한 뒤 `Save & Hot Load` 한 번으로 진실 JSON 저장과 Debug 서버 재로딩까지 완료한다.

### ② 현재 코드 증거와 진실 데이터 소유권

- `Data/Gameplay/ChampionGameData/champions.json`: `champions[]`의 이름, 기본/성장 스탯, QWER `cooldownSecByRank`를 소유한다. `RuntimeGameplayDefinitionOverlay::ApplyChampionsJson`이 이미 이를 재파싱한다.
- `Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json`: 스킬 key별 `flatByRank`, `totalAdRatioByRank`, `bonusAdRatioByRank`, `apRatioByRank`를 소유한다. `ApplySkillEffectsJson`이 이미 이를 재파싱한다.
- `Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json`: `minions[]`의 `maxHp/attackDamage`와 `structure.turretMaxHp/turretAI.attackDamage`를 소유한다. `ApplySpawnObjectJson`이 이미 이를 재파싱한다.
- `ReloadGameplayDefinitions`는 Debug 서버에서 세 파일을 다시 읽고 모든 챔피언 `StatComponent`를 dirty로 만든다. 스킬 정의는 다음 query/cast부터 활성 팩을 사용한다. SpawnObject 값은 기본적으로 다음 스폰/웨이브부터 적용되므로 현재 미니언/타워에는 live practice 명령을 함께 보내야 한다.
- 현재 `ApplyStructureStatOverride`는 타워 HP/AD를 즉시 적용할 수 있다. `ApplyMinionStatOverride`는 AD만 지원하므로 Max HP를 추가해야 한다.

### ③ 최소 구현 범위

#### `Client/Private/UI/ChampionTuner.cpp`, `Client/Public/UI/ChampionTuner.h`

- F4 표면을 `Champions / Skills / Minions / Towers` 네 탭으로 교체한다.
- 상단 공용 Champion combo는 `champions.json`의 모든 `champion` 이름을 나열하며 `Current champion`에 종속되지 않는다.
- `Champions`: Health, Mana, Attack Damage, Ability Power, Armor, Magic Resist, Attack Speed의 `Base / Per Level` 7행을 직접 편집한다.
- `Skills`: Q/W/E/R combo와 랭크 열을 제공하고 `Cooldown`, `Flat Damage`, `Total AD Ratio`, `Bonus AD Ratio`, `AP Ratio`를 편집한다. Q/W/E는 5랭크, R은 3랭크이며 비활성 랭크는 표시하지 않는다.
- `Minions`: Melee/Ranged/Siege/Super 역할 combo와 `Max Health / Attack Damage`를 편집한다.
- `Towers`: `Max Health / Attack Damage`를 편집한다.
- `WintersResolveContentPath`로 위 세 Data JSON의 워크스페이스 절대 경로를 찾고 원본 JSON object를 메모리에 유지한다. 편집 시 해당 선택 항목만 바꾸며 다른 key/배열은 보존한다.
- `Save & Hot Load`는 모든 노출 수치의 finite/non-negative 검사를 통과한 경우에만 세 JSON을 임시 파일에 기록하고 같은 볼륨에서 replace한 뒤 `ReloadGameplayDefinitions`를 전송한다. 이어 역할별 Minion HP/AD와 Turret HP/AD live 명령을 전송해 현재 엔티티도 갱신한다.
- `Reload Draft`는 디스크의 세 JSON을 다시 읽어 화면을 원본으로 되돌린다. 이 두 버튼 외 raw override/target net/practice session 제어는 새 표면에 노출하지 않는다.
- 저장 파일은 runtime scratch JSON이 아니라 위 진실 Data JSON 세 개다. 별도 중복 balance schema는 만들지 않는다.

#### `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h`, `Shared/Schemas/Command.fbs`

- 새 operation을 만들지 않는다. `eMinionStatOverrideId`에 `MaxHp`를 추가하고 `Count`만 확장한다. wire의 operation 번호는 유지한다.

#### `Shared/GameSim/Definitions/PracticeMinionAttackDamagePolicy.h`, `Server/Public/Game/GameRoom.h`, `Server/Private/Game/GameRoomCommands.cpp`, `Server/Private/Game/GameRoomSpawn.cpp`

- 기존 역할별 practice policy를 HP와 AD를 함께 보유하도록 확장한다. 이름은 현 파일을 유지하되 구조체 내부 계약은 `maxHpValues/attackDamageValues` 두 배열로 명확히 한다.
- `ApplyMinionStatOverride(MaxHp)`는 해당 역할의 살아 있는 기존 미니언 `HealthComponent`와 `MinionStateComponent`의 최대/현재 HP를 즉시 갱신한다.
- 신규 미니언은 활성 override가 있으면 HP와 AD를 모두 적용한다.
- clear/practice disable은 활성 SpawnObject pack의 역할별 HP와 시간 성장 AD로 기존 미니언을 복구한다.
- keyframe은 두 배열을 모두 capture/restore한다.

### ④ 적용 순서와 실패 경계

1. JSON draft loader/editor를 추가한다. 세 파일 중 하나라도 없거나 schema anchor를 못 찾으면 저장 버튼을 비활성화하고 정확한 파일/필드 오류를 표시한다.
2. F4 네 탭을 새 draft에 연결한다. 챔피언 변경 시 같은 JSON object에서 즉시 다른 champion/skill entry를 선택한다.
3. Minion MaxHp wire enum과 서버 policy를 확장한다.
4. Save 성공 뒤에만 server reload/live apply 명령을 전송한다. 파일 저장 실패 시 서버 명령은 보내지 않는다.
5. 기존 WFX/F5/F8/F9 및 아이템·정글·부활 경로는 이번 교정 범위에서 수정하지 않는다.

`Save & Hot Load`가 서버에서 실패하면 파일은 저장되어 있으므로 상태 문자열에 서버 command 결과 확인 필요를 표시한다. Debug 전용이라는 경계는 창 상단에 한 줄로 유지한다. Release 서버에서 practice/hot reload를 우회 활성화하지 않는다.

### ⑤ 검증 계약

- 정적 계약: `Current champion` 문구 제거, 네 탭, champion combo, 다섯 skill 행, JSON 세 경로, `Save & Hot Load`, `ReloadGameplayDefinitions`, minion MaxHp enum/server branch를 검사한다.
- JSON round-trip probe: 임시 fixture의 한 champion/skill/minion/turret만 수정하고 다른 key가 보존되는 helper 계약을 실행 가능한 테스트로 둔다. 실제 Data JSON은 테스트가 수정하지 않는다.
- SimLab: minion policy의 역할/값 검증, HP/AD resolve, clear, keyframe copy를 실행한다.
- 빌드: GameSim/Server/Client Debug 및 Release를 `/m:1 /nr:false`로 직렬 검증한다. 다른 MSBuild/cl/link가 같은 출력 경로를 사용하면 종료를 기다리고 중복 빌드하지 않는다.
- 수동 QA 미실행 시 RESULT에 `자동/빌드 통과, F5 화면 미확인`을 분리한다.

### 서브 에이전트 교정 비평 게이트

Bernoulli에게 이 교정 세션을 읽기 전용으로 비평시켜 다음을 확인한다: 데이터 진실 파일 선택이 맞는지, 선택 챔피언과 skill key 매핑이 전 챔피언에 안전한지, save/reload/live apply 순서가 부분 적용을 만들지 않는지, 미니언 HP 복구/keyframe 누락이 없는지, 사용자가 요구한 핵심 필드가 다시 누락되지 않았는지. P0/P1을 계획에 반영하기 전에는 source edit을 시작하지 않는다.

#### Bernoulli 교정 비평 P0/P1 처분

- **P0 수용 — 단일 서버 reload transaction.** JSON 저장 뒤 Minion/Structure practice 명령을 연달아 보내는 계획을 폐기한다. `ReloadGameplayDefinitions`가 새 pack 구성에 성공한 뒤 같은 handler 안에서 기존 챔피언 dirty, 기존 미니언의 HP/AD, 기존 타워/넥서스 타워의 HP/AD를 모두 새 pack으로 갱신한다. 클라이언트는 reload 명령 하나만 전송한다. 따라서 `eMinionStatOverrideId::MaxHp`, Command schema, practice minion policy/keyframe 확장은 이번 교정에서 만들지 않는다.
- **P0 수용 — 3파일 저장 롤백.** 클라이언트는 세 JSON을 모두 memory parse/도메인 검증한 뒤 세 temp 파일을 먼저 완성한다. 원본 backup을 만든 다음 replace하며, 하나라도 실패하면 이미 바뀐 파일을 backup으로 복구한다. 모든 replace 성공 뒤에만 reload 명령을 보낸다. 서버 reload 실패 시 active pack은 교체 전 상태를 유지하고 별도 live override가 없으므로 혼합 상태를 만들지 않는다.
- **P0 수용 — HP mirror 정정.** 기존 미니언 HP 갱신 owner는 `HealthComponent.fCurrent/fMaximum`과 `MinionComponent.hp/maxHp`다. `MinionStateComponent`는 AD만 갱신한다. 새 pack HP/AD에는 spawn과 동일한 `timeGrowth`를 적용한다.
- **P1 수용 — champion/skill bijection gate.** Save 활성화 전에 `champions[]` 이름 중복, QWER slot 중복/누락, `skill.<lower champion>.<qwer>` key 중복/누락을 전수 검증한다. 현재 17명×QWER는 모두 존재함을 실측했으며, 테스트 fixture가 미래 누락을 막는다.
- **P1 수용 — cooldown canonicalization.** scalar `cooldownSec`도 읽어 5/3칸 draft로 확장하고, 저장 시 `cooldownSecByRank` 배열로 기록한다. 기존 scalar는 rank 1 값으로 함께 갱신해 구 소비자와도 모순되지 않게 한다.
- **P1 수용 — param-driven damage 경계.** Yasuo Q, Kalista E, Lee Sin Q, Ezreal R은 custom flat 계산 때문에 일반 formula flat을 그대로 소비하지 않는다. 이 네 슬롯은 `Flat Damage` 랭크 행을 비활성화하고 실제 runtime `params`의 `baseDamage`, `damagePerRank`, `damagePerSpear` 등 존재하는 피해 필드를 `Special Damage Params`로 노출한다. AD/AP ratio 랭크 행은 `DamageQueueSystem`의 param-driven branch가 custom flat은 유지하면서 활성 formula ratio만 병합하도록 교정해 실제로 반영한다.
- **P1 수용 — tower tier 구분.** Towers 탭은 Turret Max Health, Turret Attack Damage, Nexus Turret Attack Damage를 구분한다. 서버 reload refresh도 nexus tier에는 `nexusAttackDamage`, 나머지 turret에는 `attackDamage`를 적용한다.
- **P1 수용 — Release cook 경계.** Debug hot load는 source JSON을 즉시 반영하지만 Release의 generated pack 영속화는 `Build-LoLDefinitionPack.py` recook 소관이다. 검증에 pack generator와 `--check`를 포함하고, 창 하단에 `Hot Load = Debug, Release 반영 = data cook/build` 한 줄을 표시한다.

#### 교정 후 최종 소스 편집 게이트

P0/P1을 위와 같이 모두 반영했다. 최종 source scope는 `ChampionTuner UI/JSON transaction`, `ReloadGameplayDefinitions 성공 handler의 기존 엔티티 refresh`, `param-driven ratio merge`, 계약/SimLab 테스트다. 새 command operation과 새 중복 balance schema는 만들지 않는다.

#### Bernoulli 최종 비평 추가 처분

- **P0 확장 수용 — 실제 소비 capability.** explicit 4-slot 예외만 보지 않고, `params.baseDamage` 등을 basic-attack empower나 별도 pending hit가 직접 소비하는 스킬도 고려한다. Skills 탭은 canonical formula 7행(`Flat/Total AD/Bonus AD/AP/Target Max HP/Target Missing HP` + Cooldown)을 유지하되, 선택 effect의 `params`에 존재하는 피해 관련 필드를 같은 화면의 `Runtime Damage Params`에 추가한다. formula flat 비소비가 확인된 슬롯은 flat 행을 비활성화하고 사유를 표시한다. 정적 계약은 현재 known param-driven/custom-hit key 목록과 runtime param 노출을 함께 검사한다.
- **P1 수용 — stale draft 차단.** 로드 시 세 파일의 원문을 보존하고 Save 직전 다시 읽어 byte compare한다. 외부 Codex/에디터 변경이 있으면 전체 저장을 중단하고 파일명과 `Reload Draft` 요구를 표시한다. `ordered_json`으로 key 순서를 보존하고 dirty 문서만 temp/backup/replace한다.
- **P1 수용 — 입력 도메인.** Champion base HP/Mana는 0 이상(무자원 허용), Minion/Turret HP는 1 이상, AD/ratio/cooldown은 0 이상이며 서버 overlay 상한을 따른다. rank 배열 길이는 QWE 5/R 3을 강제한다. 저장 전 전체 draft preflight가 같은 조건을 재검증한다.
- **P1 수용 — lane minion role 경계.** F4는 role 0..3만 편집하고 reload live refresh도 role<4만 갱신한다. role 4 Tibbers entry는 저장·런타임 갱신 모두 보존한다.
- **P1 수용 — 누락 핵심 필드.** Champions 탭에 `Attack Speed Ratio`와 `Resource Regen / Sec`를 추가한다. Skills 탭에 `Target Max HP Ratio`와 `Target Missing HP Ratio`를 추가해 Viego R 같은 공식을 조절할 수 있게 한다. 이동속도/사거리/윈드업은 이번 사용자가 다시 특정한 공격력·방어력·성장·쿨타임 범위 밖이므로 노출하지 않는다.
- **P1 수용 — 적용 확인.** 새 snapshot schema를 추가하지 않고 기존 generic command ack와 `toolRevision`을 사용한다. Save 전 revision을 기록하고 `SetEnabled + ReloadGameplayDefinitions` 두 command가 ack된 뒤 revision이 2 이상 증가하면 성공으로 표시한다. ack되었으나 증가가 부족하면 server reject로 표시한다.
- **P1 수용 — Debug/Release 경계.** Release client에서는 Save & Hot Load를 컴파일 조건으로 비활성화한다. 창에 data cook/build 필요 문구를 표시하고 검증에 generator 실행, `--check`, data-driven pipeline 검사를 포함한다.
- **P2 수용 — rewind epoch.** reload 성공 시 기존 keyframe과 pending rewind를 폐기하고 timeline epoch/branch를 증가시켜 이전 definition generation으로 되감는 경로를 차단한다.

위 처분까지 반영했으므로 source edit gate를 통과한다.

### 교정 구현 완료 상태 — 2026-07-19

- `ChampionTuner`의 실행 표면을 `Champions / Skills / Minions / Towers` 네 탭으로 교체했다. 챔피언 선택기는 챔피언 데이터가 필요한 앞의 두 탭에만 표시한다.
- `Champions`는 HP/Mana/AD/AP/Armor/MR/Attack Speed의 Base/Per Level, Attack Speed Ratio, Resource Regen/Sec를 편집한다.
- `Skills`는 Q/W/E/R의 랭크별 Cooldown, Flat, Total AD, Bonus AD, AP, Target Max HP, Missing HP 계수를 편집한다. scalar cooldown은 rank 배열로 확장하고 저장 시 rank 1과 동기화한다.
- `Minions`는 lane role 0..3의 Max HP/AD, `Towers`는 일반/넥서스 타워 HP/AD를 편집한다. role 4 소환물은 보존한다.
- 저장은 세 진실 JSON의 로드 원문 stale check, 전체 도메인 검증, dirty 문서 temp/backup/replace, 실패 rollback을 거친다. 성공 뒤 Debug 방 호스트가 `SetEnabled + ReloadGameplayDefinitions`를 전송한다.
- 서버 reload 성공 handler는 같은 authoritative tick에 챔피언 stat dirty, 살아 있는 lane minion HP/AD mirror, 살아 있는 일반/넥서스 타워 HP/AD를 새 pack으로 갱신한다. 이전 definition generation의 keyframe/pending rewind는 폐기한다.
- param-driven/custom-hit 스킬은 custom flat을 보존하면서 canonical AD/AP/HP ratio를 병합한다. UI는 known custom-flat 슬롯의 Flat 행을 비활성화하고 실제 runtime damage params를 노출한다.
- 자동 검증은 F4 계약, definition pack freshness/round-trip, 전체 SimLab, GameSim/Server/Client/SimLab Debug·Release 빌드까지 통과했다. 실제 F5 화면 클릭·배치는 수동 시각 QA 경계로 남긴다.

## 후속 교정 계획 — 2026-07-19 Reload Draft 설명 / Hot Load 차단 진단 / 수치 슬라이더

Session - F4의 저장·재로드 의미를 화면에서 분리하고 스킬 수치 입력을 실사용 가능한 슬라이더로 바꾼다
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md, 2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_RESULT.md

### 1. 결정 기록

① 문제·제약: 사용자가 Release Client에서 실행했음을 확인해 `Save & Hot Load` 비활성 자체는 정상이다. 다만 `Reload Draft`는 미저장 편집을 버린다는 설명이 없고, 스킬 7행×최대 5랭크는 숫자 입력칸이라 반복 조절 비용이 크다(수동 클릭 실측 없음).
② 순진한 해법의 실패: Release Hot Load 제한을 풀면 서버 권위/배포 경계를 깨며, 버튼 이름만 바꾸면 Debug Server·방장·연결 조건을 계속 숨긴다.
③ 메커니즘: 기존 단일 `Save & Hot Load`와 Debug gate는 유지하고 첫 차단 조건을 정확히 표시한다. 보조 `Reload JSON`은 dirty draft 폐기 확인을 거치며, 스킬 공식과 runtime damage param만 bounded slider로 바꾼다.
④ 대조: ImGui slider는 드래그 조절과 Ctrl+클릭 정확 입력을 함께 제공한다. JSON validator/server overlay 범위는 안전 게이트로 유지하되 일상 튜닝 범위는 Cooldown 0..300, Flat 0..2000, ratio 0..5로 좁힌다.
⑤ 대가: 슬라이더 범위 밖 실험은 F4에서 할 수 없다. 300초 초과 cooldown, 2000 초과 flat, 5.0 초과/음수 ratio가 실제 요구가 되면 표시 범위를 데이터 메타로 승격해야 한다.

경계 선언: JSON 저장은 Client authoring 동작이고, 실제 게임 적용은 계속 `Client PracticeCommand -> Debug room host -> Server reload -> authoritative pack/entity refresh`만 사용한다. Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.

### 2. 반영해야 하는 코드

#### 2-1. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

활성 `CChampionTuner::Render`의 스킬 랭크 입력 블록을 아래로 교체한다.

기존 코드:

```cpp
if (ImGui::InputFloat("##Value", &value, 0.f, 0.f, "%.3f") &&
	std::isfinite(value) && value >= minValue && value <= maxValue)
```

아래로 교체:

```cpp
if (ImGui::SliderFloat(
	"##Value", &value, minValue, maxValue, pFormat,
	ImGuiSliderFlags_AlwaysClamp) && std::isfinite(value))
```

`DrawRankRow`에 `pFormat` 인자를 추가하고 호출 범위를 다음으로 고정한다.

```cpp
DrawRankRow("Cooldown (sec)", *pSkill, "cooldownSecByRank",
	0.f, 300.f, "%.1f s", false, draft.bChampionDirty);
DrawRankRow("Flat Damage", damage, "flatByRank",
	0.f, 2000.f, "%.0f", bCustomFlat, draft.bSkillEffectDirty);
DrawRankRow("Total AD Ratio", damage, "totalAdRatioByRank",
	0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("Bonus AD Ratio", damage, "bonusAdRatioByRank",
	0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("AP Ratio", damage, "apRatioByRank",
	0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("Target Max HP Ratio", damage, "targetMaxHpRatioByRank",
	0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("Missing HP Ratio", damage, "targetMissingHpRatioByRank",
	0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
```

슬라이더가 기존 105px 열에서 지나치게 거칠지 않도록 최초 창 너비를 1180px, 각 rank 열을 155px로 넓힌다. 표 아래에 `Drag to tune. Ctrl+click to type an exact value.`를 표시한다.

`EditFloat` 아래에 damage runtime param용 bounded slider helper를 추가한다.

```cpp
const auto EditSliderFloat = [](
	ordered_json& owner, const char* pField, const char* pLabel,
	f32_t minValue, f32_t maxValue, const char* pFormat, bool_t& bDirty)
{
	f32_t value = owner.value(pField, 0.f);
	ImGui::SetNextItemWidth(-1.f);
	ImGui::PushID(pField);
	if (ImGui::SliderFloat(
		pLabel, &value, minValue, maxValue, pFormat,
		ImGuiSliderFlags_AlwaysClamp) && std::isfinite(value))
	{
		owner[pField] = value;
		bDirty = true;
	}
	ImGui::PopID();
};
```

`Runtime Damage Params`의 `EditFloat` 호출을 아래로 교체한다.

```cpp
const bool_t bRatio = std::strstr(pParam, "Ratio") != nullptr;
EditSliderFloat(
	params, pParam, pParam, 0.f, bRatio ? 5.f : 2000.f,
	bRatio ? "%.2f" : "%.1f", draft.bSkillEffectDirty);
```

버튼 블록은 기존 Debug 전용 Primary action을 유지하고, reload가 dirty draft를 조용히 버리지 않도록 아래 의미로 교체한다.

```cpp
const bool_t bHotLoadPending = draft.pendingHotLoadSequence != 0u;
ImGui::BeginDisabled(!bCanHotLoad || !draft.bLoaded || bHotLoadPending);
if (ImGui::Button("Save & Hot Load", ImVec2(160.f, 0.f)) &&
	SaveBalanceData(state))
{
	// 기존 SetEnabled + ReloadGameplayDefinitions 전송과 revision 확인을 유지한다.
}
ImGui::EndDisabled();
ImGui::SameLine();
ImGui::BeginDisabled(bHotLoadPending);
if (ImGui::Button("Reload JSON", ImVec2(130.f, 0.f)))
{
	if (draft.bChampionDirty || draft.bSkillEffectDirty || draft.bSpawnObjectDirty)
		ImGui::OpenPopup("Discard unsaved F4 edits?");
	else
		LoadBalanceData(state);
}
ImGui::EndDisabled();
if (ImGui::BeginPopupModal("Discard unsaved F4 edits?", nullptr,
	ImGuiWindowFlags_AlwaysAutoResize))
{
	ImGui::TextUnformatted(
		"Reload JSON discards unsaved F4 values and rereads the three data files.");
	if (ImGui::Button("Discard & Reload"))
	{
		LoadBalanceData(state);
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}
```

단일 availability helper가 `bCanHotLoad`와 한 줄 진단을 함께 만든다. Client Release, non-authoritative scene, command serializer 없음, network object 없음, network disconnected, snapshot/ack tracker 없음을 서로 구분한다. Client가 사전 판별할 수 없는 `Debug Server / room host / server JSON parse` 조건은 전송 후 서버 reject 안내로만 말한다. Release Client에서는 JSON authoring까지 수행하지 않으며 cook/build 경계를 유지한다.

Primary tooltip은 세 JSON 검증·저장 뒤 authoritative Debug server reload를 요청하며 room host만 승인된다고 설명한다. `Reload JSON` tooltip은 세 JSON을 다시 읽고 미저장 F4 값은 폐기하지만 서버 값은 자체 변경하지 않는다고 설명한다. 초기 load 실패 버튼과 stale-source 오류의 `Reload Draft` 문구도 `Reload JSON`으로 통일한다.

#### 2-2. C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py

활성 F4 surface 계약에 다음 검사를 추가한다.

```python
require('"Save & Hot Load"' in tuner_surface, "Debug authoritative hot-load action")
require('"Reload JSON"' in tuner_surface, "disk draft reload action")
require('"Discard unsaved F4 edits?"' in tuner_surface,
        "dirty draft reload confirmation")
require("ImGui::SliderFloat" in tuner_surface, "F4 skill slider controls")
require('ImGui::InputFloat("##Value"' not in tuner_surface,
        "active skill rank values are not raw input boxes")
require("ImGuiSliderFlags_AlwaysClamp" in tuner_surface,
        "slider direct input respects displayed bounds")
require(tuner_surface.count('"Save & Hot Load"') == 1,
        "one primary save and hot-load action")
require('"Reload Draft"' not in tuner_surface, "old ambiguous reload label removed")
runtime_params = tuner_surface.split('SeparatorText("Runtime Damage Params")', 1)[1].split(
    "if (bCustomFlat)", 1)[0]
require("EditSliderFloat" in runtime_params, "runtime damage params use sliders")
require("EditFloat(" not in runtime_params, "runtime damage params have no raw inputs")
require("bHotLoadPending" in tuner_surface,
        "save and reload lock while authoritative ack is pending")
```

기존 `single save and hot-load action`/`Reload Draft` 문자열 계약은 위 계약으로 교체한다.

### 3. 검증

예측:
- Release Client에서는 `Save & Hot Load`가 계속 비활성이고 `Debug Client + Debug Server + room host` 필요 문구가 표시된다. authoritative scene/serializer/network가 없을 때는 각각의 첫 차단 조건이 표시된다.
- Debug 서버 권위 연결에서는 `Save & Hot Load`가 활성화된다. 방장이 아니거나 Server가 Release면 전송 뒤 revision 증가 부족으로 reject가 표시된다.
- Skills의 cooldown/flat/ratio와 Runtime Damage Params는 slider로 보이고, Ctrl+클릭 정확 입력도 표시 범위 안에서 동작한다.
- `Reload JSON`은 clean 상태에서 세 JSON을 즉시 다시 읽고, dirty 상태에서는 미저장 메모리 편집을 버리기 전에 확인한다. 어느 경우에도 서버 값은 바꾸지 않는다.

검증 명령:

```powershell
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
git diff --check -- Client/Private/UI/ChampionTuner.cpp Tools/LoLData/Test-F4BalanceContracts.py .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_RESULT.md
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
```

미검증:
- 실제 F5 창에서 mouse drag/Ctrl+클릭과 room-host Hot Load를 클릭하는 수동 화면·네트워크 QA.

확인 필요:
- 없음. Client는 room host와 Server configuration을 사전 식별하는 정보가 없으므로, 그 두 조건은 기존 authoritative command ack/revision으로만 확정한다.

### 서브 에이전트 비평

비평 주체: `/root/f4_slider_hotload_critic`, read-only. 판정은 P0 없음, P1 4건, P2 3건이다.

- **P1 수용 — pending ack 보존.** Save 성공 직후 dirty가 지워져도 `pendingHotLoadSequence`가 남는다. 이 동안 Primary/Reload를 모두 비활성화해 pending sequence와 expected revision을 `LoadBalanceData`가 지우거나 새 전송이 덮지 못하게 한다.
- **P1 수용 — Client가 아는 차단 원인만 표시.** Release/scene/serializer/network/connected/snapshot은 단일 helper로 사전 진단한다. host, Server Release, JSON parse 실패는 server-side 조건이므로 reject 뒤 공통 다음 행동만 표시한다.
- **P1 수용 — reload 의미와 전체 문자열 통일.** 초기 load 실패 버튼, stale-source 안내, 정상 action을 모두 `Reload JSON`으로 바꾸고 dirty 폐기 확인/tooltip을 추가한다.
- **P1 수용 — runtime slider 계약 보강.** 활성 `#if 0` 이전 slice가 rank와 runtime damage param slider를 각각 검사하고, Primary 1개, 구 `Reload Draft` 부재, pending lock을 고정한다.
- **P2 수용 — 조작 폭.** 최초 창을 1180px, rank 열을 155px로 넓히고 Ctrl+click 정확 입력을 화면에 적는다. logarithmic slider는 0 경계의 감각이 달라 이번에는 쓰지 않는다.
- **P2 수용 — 의도적 authoring 범위 축소.** validator의 -1e6..1e6 허용과 별개로 F4 slider는 현재 데이터 실측 최대 cooldown 125, flat 750, ratio 1.3을 포함하는 nonnegative 0..300/2000/5 범위다. 범위 밖 실험은 ⑤의 메타데이터 승격 조건으로 남긴다.
- **P2 보류 — global tool revision 동시 명령 오탐.** 기존 ack protocol의 범위이며 이번 UI 교정에서 schema를 넓히지 않는다. 별도 command-result correlation 작업에서 다룬다.

위 처분을 계획에 반영했으므로 source edit gate를 통과한다.

## 후속 재교정 계획 — 2026-07-19 WFX형 DragFloat / 더블클릭 입력

Session - F4 스킬 수치 조작을 WFX Effect Tool과 같은 DragFloat UX로 교체한다.

좌표: `Client/Private/UI/ChampionTuner.cpp`의 활성 F4 `Skills` rank 표와 `Runtime Damage Params`.

관련: 이 계획서와 같은 이름의 `*_RESULT.md`에 구현·검증 결과를 이어서 기록한다.

### 1. 실패 인정과 현재 코드 증거

- 직전 요구를 `slider widget`으로 문자 그대로 해석해 파란 thumb가 보이는 `ImGui::SliderFloat`를 넣은 것은 잘못이다. 사용자가 제시한 왼쪽 WFX 필드는 트랙/thumb slider가 아니라 숫자 영역 전체를 좌우로 드래그하는 `ImGui::DragFloat` 계열이다.
- 기준 구현은 `Client/Private/UI/WfxEffectToolPanel.cpp`의 `Attach Offset`, `Velocity`, `Scale`, `World Yaw Spin Speed` 등이며 각각 `ImGui::DragFloat3`/`ImGui::DragFloat`를 사용한다.
- 현재 포함된 Dear ImGui의 `DragScalar` 활성화 경로는 `Engine/External/imgui/imgui_widgets.cpp`에서 double-click을 temp text input 활성 조건으로 직접 처리한다. 따라서 전역 `ImGuiIO::ConfigDragClickToInputText`를 바꾸거나 별도 팝업 입력기를 만들 필요가 없다.
- 이번 교정은 수치 위젯과 계약 테스트만 바꾼다. 기존 JSON 저장, `Save & Hot Load`, `Reload JSON`, authoritative ack, 범위 제한은 유지한다.

### 2. 성공 조건

1. Skills rank 수치는 WFX처럼 회색 숫자 필드 전체를 좌우 드래그해 변경된다.
2. 같은 필드를 더블클릭하면 키보드로 정확한 수치를 입력할 수 있다.
3. cooldown/flat damage/ratio별 드래그 감도는 각각 `0.1f`/`1.0f`/`0.01f`이고, 직접 입력도 기존 `0..300`/`0..2000`/`0..5` 범위로 clamp된다.
4. Runtime Damage Params도 같은 조작을 사용하며 ratio는 `0.01f`, damage scalar는 `1.0f` 감도를 쓴다.
5. 활성 F4 Skills surface에는 `ImGui::SliderFloat`와 파란 slider thumb가 남지 않는다.

### 3. 반영 코드

#### 3-1. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

기존 `EditSliderFloat` 람다를 아래로 교체한다.

```cpp
const auto EditDragFloat = [](
	ordered_json& owner,
	const char* pField,
	const char* pLabel,
	f32_t dragSpeed,
	f32_t minValue,
	f32_t maxValue,
	const char* pFormat,
	bool_t& bDirty)
{
	f32_t value = owner.value(pField, 0.f);
	ImGui::SetNextItemWidth(-1.f);
	ImGui::PushID(pField);
	if (ImGui::DragFloat(
		pLabel,
		&value,
		dragSpeed,
		minValue,
		maxValue,
		pFormat,
		ImGuiSliderFlags_AlwaysClamp) && std::isfinite(value))
	{
		owner[pField] = value;
		bDirty = true;
	}
	ImGui::PopID();
};
```

`DrawRankRow`에 `dragSpeed` 인자를 추가하고 내부 `ImGui::SliderFloat` 호출을 아래로 교체한다.

```cpp
if (ImGui::DragFloat(
	"##Value",
	&value,
	dragSpeed,
	minValue,
	maxValue,
	pFormat,
	ImGuiSliderFlags_AlwaysClamp) && std::isfinite(value))
```

rank별 호출은 아래 감도로 고정한다.

```cpp
DrawRankRow("Cooldown (sec)", *pSkill, "cooldownSecByRank",
	0.1f, 0.f, 300.f, "%.1f s", false, draft.bChampionDirty);
DrawRankRow("Flat Damage", damage, "flatByRank",
	1.f, 0.f, 2000.f, "%.0f", bCustomFlat, draft.bSkillEffectDirty);
DrawRankRow("Total AD Ratio", damage, "totalAdRatioByRank",
	0.01f, 0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("Bonus AD Ratio", damage, "bonusAdRatioByRank",
	0.01f, 0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("AP Ratio", damage, "apRatioByRank",
	0.01f, 0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("Target Max HP Ratio", damage, "targetMaxHpRatioByRank",
	0.01f, 0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
DrawRankRow("Missing HP Ratio", damage, "targetMissingHpRatioByRank",
	0.01f, 0.f, 5.f, "%.2f", false, draft.bSkillEffectDirty);
```

조작 안내는 아래로 교체한다.

```cpp
ImGui::TextDisabled(
	"Drag horizontally to tune. Double-click to type an exact value. Ratio 1.0 = 100%%.");
```

Runtime Damage Params 호출은 아래로 교체한다.

```cpp
EditDragFloat(
	params,
	pParam,
	pParam,
	bRatio ? 0.01f : 1.f,
	0.f,
	bRatio ? 5.f : 2000.f,
	bRatio ? "%.2f" : "%.1f",
	draft.bSkillEffectDirty);
```

#### 3-2. C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py

기존 slider 계약을 아래처럼 Skills와 Runtime Damage Params slice를 분리하는 DragFloat 계약으로 교체한다.

```python
skills_surface = tuner_surface.split('BeginTabItem("Skills")', 1)[1].split(
    'BeginTabItem("Minions")', 1)[0]
rank_controls = skills_surface.split("const auto DrawRankRow", 1)[1].split(
    'if (ImGui::BeginTable(', 1)[0]
require("ImGui::DragFloat" in rank_controls, "F4 rank drag controls")
require("dragSpeed" in rank_controls and
        "ImGuiSliderFlags_AlwaysClamp" in rank_controls,
        "rank drag speed and direct-input clamp")
require("ImGui::SliderFloat" not in skills_surface,
        "active F4 skill surface has no slider-thumb controls")
require('ImGui::InputFloat("##Value"' not in skills_surface,
        "active skill rank values are not raw input boxes")
for call_contract in (
        '0.1f, 0.f, 300.f, "%.1f s"',
        '1.f, 0.f, 2000.f, "%.0f"',
        '0.01f, 0.f, 5.f, "%.2f"'):
    require(call_contract in skills_surface,
            f"rank DragFloat contract {call_contract}")
require("Double-click to type an exact value." in skills_surface,
        "DragFloat exact-input gesture is explained")
runtime_params = skills_surface.split(
    'SeparatorText("Runtime Damage Params")', 1)[1].split(
        "if (bCustomFlat)", 1)[0]
require("EditDragFloat(" in runtime_params,
        "runtime damage params use DragFloat")
require("bRatio ? 0.01f : 1.f" in runtime_params,
        "runtime ratio and damage drag speeds")
require("EditSliderFloat(" not in runtime_params and
        "EditFloat(" not in runtime_params,
        "runtime damage params have no slider-thumb or raw input path")
```

WFX 소스의 존재를 영구 결합하는 대신, 활성 F4 자체의 rank/runtime 조작 계약을 고정하고 WFX와의 시각·조작 일치는 수동 F5 항목으로 검증한다.

### 4. 검증

```powershell
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
git diff --check -- Client/Private/UI/ChampionTuner.cpp Tools/LoLData/Test-F4BalanceContracts.py .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_RESULT.md
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
msbuild Client/Include/Client.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
```

수동 확인 항목은 F5 Debug에서 숫자 영역 좌우 드래그, 더블클릭 텍스트 입력, clamp, `Save & Hot Load` 후 실제 피해 반영이다.

### 5. 독립 비평 게이트

비평 주체: `/root/f4_slider_hotload_critic`, read-only. 판정은 P0 없음, P1 1건, P2 3건이다.

- **기술 방향 수용.** WFX는 실제로 `DragFloat3`/`DragFloat`를 사용하며, `DragScalar`는 X축 드래그와 독립적인 double-click temp input을 제공한다. `AlwaysClamp`는 double-click 직접 입력에도 min/max를 적용한다.
- **P1 수용 — 계약 테스트 구체화.** 최초 초안은 rank/runtime 양쪽의 DragFloat·AlwaysClamp·감도 `0.1/1.0/0.01`을 각각 보장하지 못했다. 위 §3-2처럼 slice와 정확한 토큰을 분리해 보강했다.
- **P2 수용 — SliderFloat 부재 범위 축소.** 전체 F4가 아니라 `Skills` slice만 검사해 다른 탭의 장래 적법한 slider까지 금지하지 않는다.
- **P2 수용 — WFX 영구 결합 제거.** WFX는 구현 판단의 근거로만 남기고 계약 테스트는 F4 자체를 고정한다.
- **P2 수용 — 수동 QA 유지.** 정적 테스트는 실제 double-click 제스처를 증명하지 못하므로 F5 Debug 수동 항목을 완료 조건에 남긴다.

위 처분을 계획에 반영했으므로 DragFloat source edit gate를 통과한다.

## 후속 결함 교정 계획 — 2026-07-19 Save & Hot Load 현재 쿨다운 반영

Session - 이렐리아 Q를 기준으로 JSON 저장부터 서버 정의와 살아 있는 스킬 쿨다운까지 한 번의 authoritative hot load로 반영한다.

좌표: `Shared/GameSim/Systems/SkillCooldown`, `Server/Private/Game/GameRoomCommands.cpp`, `Tools/SimLab/main.cpp`, F4 계약 테스트.

관련: 같은 계획/결과 문서에 DragFloat 교정과 함께 기록한다.

### 1. 현재 증거와 실패 경계

- 현재 `Data/Gameplay/ChampionGameData/champions.json`의 IRELIA Q `cooldownSecByRank`는 사용자가 F4에서 수정한 값으로 실제 저장되어 있다. 따라서 이번 제보의 첫 실패점은 디스크 write가 아니다.
- `TryReloadRuntimeGameplayDefinitions`는 이 배열을 새 runtime pack에 overlay하고, 서버 tick은 매 tick `GetActiveLoLGameplayDefinitionPack()`을 다시 바인딩한다. 다음 시전의 쿨다운은 `GameplayDefinitionQuery::ResolveSkillCooldown`에서 active pack을 조회한다.
- 그러나 `ReloadGameplayDefinitions` 성공 분기는 살아 있는 미니언·포탑·챔피언 stat만 갱신하고, 이미 진행 중인 `SkillStateComponent::cooldownRemaining/cooldownDuration`은 전혀 갱신하지 않는다. 즉 리로드 당시 돌고 있던 이렐리아 Q는 예전 정의의 시간이 계속 보인다.
- hot load는 현재 진행률을 보존한 채 old/new definition cooldown 비율로 진행 중 타이머를 재매핑한다. ready skill은 ready 상태를 유지하며, 다음 시전은 이미 새 pack을 사용한다.

### 2. 성공 조건

1. IRELIA Q rank 값이 JSON에 저장되고 reload command가 성공하면 다음 시전은 새 rank 값을 사용한다.
2. reload 시점에 Q가 cooldown 중이면 `remaining / duration` 진행률을 유지하면서 새 정의 시간으로 즉시 재매핑된다.
3. 이 정책은 Q/W/E/R 전체와 모든 챔피언에 동일하게 적용되며 basic attack은 건드리지 않는다. 죽은 챔피언도 기존 `SkillCooldownSystem`이 계속 tick하므로 old-generation 타이머를 남기지 않게 함께 remap한다.
4. NaN/음수/ready 상태는 0으로 정규화되고 cooldown을 새로 시작시키지 않는다.
5. SimLab `--f4-balance-only`가 실제 remap 수치와 ready/invalid 경계를 검증한다.

### 3. 반영 코드

#### 3-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h

`Execute` 아래에 다음 public 정책 함수를 추가한다.

```cpp
static void RemapDefinitionCooldown(
	SkillSlotRuntime& slot,
	f32_t previousDefinitionCooldown,
	f32_t reloadedDefinitionCooldown);
```

이를 위해 `SkillSlotRuntime`을 전방 선언하고 `WintersTypes.h`를 include한다.

#### 3-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp

`Execute` 앞에 아래 구현을 추가한다.

```cpp
void CSkillCooldownSystem::RemapDefinitionCooldown(
	SkillSlotRuntime& slot,
	f32_t previousDefinitionCooldown,
	f32_t reloadedDefinitionCooldown)
{
	if (!std::isfinite(slot.cooldownRemaining) ||
		!std::isfinite(slot.cooldownDuration) ||
		slot.cooldownRemaining <= kCooldownReadyEpsilon ||
		slot.cooldownDuration <= kCooldownReadyEpsilon)
	{
		slot.cooldownRemaining = 0.f;
		slot.cooldownDuration = 0.f;
		return;
	}

	const f32_t previous = std::isfinite(previousDefinitionCooldown)
		? std::max(0.f, previousDefinitionCooldown) : 0.f;
	const f32_t reloaded = std::isfinite(reloadedDefinitionCooldown)
		? std::max(0.f, reloadedDefinitionCooldown) : 0.f;
	const f32_t progressRemaining = std::clamp(
		slot.cooldownRemaining / slot.cooldownDuration, 0.f, 1.f);
	const f32_t reloadedDuration = previous > kCooldownReadyEpsilon
		? slot.cooldownDuration * (reloaded / previous)
		: reloaded;
	slot.cooldownDuration = reloadedDuration;
	slot.cooldownRemaining = reloadedDuration * progressRemaining;
	if (slot.cooldownRemaining <= kCooldownReadyEpsilon)
	{
		slot.cooldownRemaining = 0.f;
		slot.cooldownDuration = 0.f;
	}
}
```

`<algorithm>`과 `<cmath>`를 직접 include한다.

#### 3-3. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`GameplayDefinitionQuery.h`와 `SkillCooldownSystem.h`를 직접 include한다. reload 전에 기존 `tc.pDefinitions` 포인터를 보존하고, stale practice override가 아직 존재할 때 실제 old baseline을 entity/slot별로 캐시한다. runtime pack publish 뒤 override를 제거하고 new cooldown을 조회해 아래 갱신을 실행한다.

```cpp
const GameplayDefinitionPack* pPreviousDefinitions = tc.pDefinitions;
struct PreviousSkillCooldowns
{
	EntityID entity = NULL_ENTITY;
	f32_t values[4]{};
};
std::vector<PreviousSkillCooldowns> previousSkillCooldowns;
m_world.ForEach<ChampionComponent, SkillStateComponent>(
	[&](EntityID entity, ChampionComponent& champion, SkillStateComponent&)
	{
		PreviousSkillCooldowns sample{};
		sample.entity = entity;
		for (u8_t slotIndex = static_cast<u8_t>(eSkillSlot::Q);
			slotIndex <= static_cast<u8_t>(eSkillSlot::R); ++slotIndex)
		{
			sample.values[slotIndex - static_cast<u8_t>(eSkillSlot::Q)] =
				GameplayDefinitionQuery::ResolveSkillCooldown(
					m_world, entity, tc,
					static_cast<eChampion>(champion.id), slotIndex);
		}
		previousSkillCooldowns.push_back(sample);
	});

// TryReloadRuntimeGameplayDefinitions succeeds, then stale practice
// override components are removed before new values are resolved.
const GameplayDefinitionPack& reloadedDefinitions =
	ServerData::GetActiveLoLGameplayDefinitionPack();
TickContext reloadedTick = tc;
	reloadedTick.pDefinitions = &reloadedDefinitions;
u32_t refreshedSkillCooldowns = 0u;
m_world.ForEach<ChampionComponent, SkillStateComponent>(
	[&](EntityID entity, ChampionComponent& champion, SkillStateComponent& skills)
	{
		const auto previousIt = std::find_if(
			previousSkillCooldowns.begin(), previousSkillCooldowns.end(),
			[&](const PreviousSkillCooldowns& sample)
			{
				return sample.entity == entity;
			});
		if (previousIt == previousSkillCooldowns.end())
			return;
		for (u8_t slotIndex = static_cast<u8_t>(eSkillSlot::Q);
			slotIndex <= static_cast<u8_t>(eSkillSlot::R); ++slotIndex)
		{
			const f32_t previousCooldown = previousIt->values[
				slotIndex - static_cast<u8_t>(eSkillSlot::Q)];
			const f32_t reloadedCooldown =
				GameplayDefinitionQuery::ResolveSkillCooldown(
					m_world, entity, reloadedTick,
					static_cast<eChampion>(champion.id), slotIndex);
			CSkillCooldownSystem::RemapDefinitionCooldown(
				skills.slots[slotIndex], previousCooldown, reloadedCooldown);
			++refreshedSkillCooldowns;
		}
	});
```

서버 성공 로그에 `skillCooldownRefresh=`를 추가한다. old 값은 overlay 제거 전에, new 값은 제거 뒤에 해석해 실제 진행 중 타이머를 만든 baseline과 새 JSON definition을 정확히 비교한다. `StorageGenerations()`가 이전 runtime pack 세대를 계속 소유하므로 `tc.pDefinitions`의 수명은 publish 뒤에도 안전하다.

#### 3-4. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunSkillCooldownDefinitionReloadProbe`를 추가한다. 순수 산술뿐 아니라 old/new pack, rank, `PracticeSkillEffectOverrideComponent` 우선순위와 제거 순서를 둔 world-level probe로 구성한다. 핵심 수치 계약은 다음과 같다.

```cpp
SkillSlotRuntime active{};
active.cooldownRemaining = 6.f;
active.cooldownDuration = 8.f;
const f32_t previousCooldown = GameplayDefinitionQuery::ResolveSkillCooldown(
	world, irelia, previousTick, eChampion::IRELIA,
	static_cast<u8_t>(eSkillSlot::Q));
world.RemoveComponent<PracticeSkillEffectOverrideComponent>(irelia);
const f32_t reloadedCooldown = GameplayDefinitionQuery::ResolveSkillCooldown(
	world, irelia, reloadedTick, eChampion::IRELIA,
	static_cast<u8_t>(eSkillSlot::Q));
CSkillCooldownSystem::RemapDefinitionCooldown(
	active, previousCooldown, reloadedCooldown);
if (std::fabs(active.cooldownDuration - 3.2f) > 0.0001f ||
	std::fabs(active.cooldownRemaining - 2.4f) > 0.0001f)
	return false;

SkillSlotRuntime ready{};
CSkillCooldownSystem::RemapDefinitionCooldown(ready, 10.f, 4.f);
if (ready.cooldownRemaining != 0.f || ready.cooldownDuration != 0.f)
	return false;
```

probe는 old override 10초, 선택 rank의 new pack 4초를 명시해 위 결과를 만든다. NaN/음수 입력도 0으로 정규화되는지 검사하고 `--f4-balance-only`의 최종 `bPass`에 포함한다. 서버 command 자체는 이 headless probe가 실행하지 못하므로 정적 Q..R loop 계약과 F5 수동 QA를 함께 완료 조건으로 둔다.

#### 3-5. C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py

서버 reload slice가 `RemapDefinitionCooldown`, `skillCooldownRefresh=`, Q..R loop를 포함하는지, SimLab f4-only path가 `RunSkillCooldownDefinitionReloadProbe`를 포함하는지 고정한다.

### 4. 검증

```powershell
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
msbuild Shared/GameSim/Include/GameSim.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
Tools/SimLab/Bin/Debug/SimLab.exe --f4-balance-only
msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
git diff --check -- Client/Private/UI/ChampionTuner.cpp Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.h Shared/GameSim/Systems/SkillCooldown/SkillCooldownSystem.cpp Server/Private/Game/GameRoomCommands.cpp Tools/SimLab/main.cpp Tools/LoLData/Test-F4BalanceContracts.py .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_RESULT.md
```

수동 확인은 Debug Client + Debug Server 방장 세션에서 IRELIA Q를 사용해 cooldown이 도는 중 값을 낮추고 `Save & Hot Load`한 뒤 HUD remaining이 즉시 비례 축소되는지, 다음 Q가 새 rank 시간으로 시작하는지 확인한다.

### 5. 독립 비평 게이트

비평 주체: `/root/f4_slider_hotload_critic`, read-only. 최초 판정은 P0 1건, P1 2건, P2 1건으로 계획 수정 전 reject였다.

- **P0 수용 — 존재하지 않는 death field 제거.** `ChampionComponent::bIsDead`는 존재하지 않는다. 더 근본적으로 dead entity도 cooldown tick 대상이므로 생존 필터 자체를 없애 모든 챔피언 Q..R을 remap한다.
- **P1 수용 — old override baseline 선캐시.** 최초 계획처럼 override 제거 뒤 old 값을 조회하면 `CooldownSecOverride`가 타이머를 만든 사실을 잃는다. old 값은 reload/제거 전에 캐시하고, new 값은 성공 publish 및 제거 뒤 조회하도록 순서를 교정했다.
- **P1 수용 — world-level probe.** 순수 helper 산술만으로는 rank/pack/override 순서를 검증할 수 없다. SimLab에 old/new pack과 override component를 쓰는 probe를 계획했고, 실제 server command 자동 실행 부재는 수동 F5 미검증으로 정직하게 남긴다.
- **P2 수용 — dead generation 일관성.** 죽은 챔피언을 건너뛰지 않아 부활 뒤 old-generation timer가 남지 않게 한다.
- **수용 — 수명과 수학.** `StorageGenerations()` 때문에 old pack 포인터는 안전하고, `old/new` 비율은 현재 `cooldownDuration`에 이미 들어간 ability haste/CDR 배율과 `remaining/duration` 진행률을 보존한다.

위 처분을 계획에 반영했으므로 Save & Hot Load cooldown source edit gate를 통과한다.

## 2026-07-19 재개 — Item damage metadata 때문에 전체 Hot Load가 차단되는 결함

### 1. 실패 증거와 범위

- 실제 Debug 서버 로그는 `ItemGameplayDefs.json: items[3153].onHitDamage unknown field: cppFlags`로, F4가 저장한 챔피언 JSON을 읽은 뒤 전체 runtime pack transaction이 Item 단계에서 롤백됨을 보여 준다.
- 현재 tracked `ItemGameplayDefs.json`의 3153 `onHitDamage`에는 authoring 필드와 함께 codegen 정규화 metadata인 `cppType`, `cppFlags`, `rankCount`가 존재한다.
- `Build-LoLDefinitionPack.py::normalize_damage_formula`는 이 세 값을 생성하거나 입력에서 무시하고 `type`, `flags`, rank array를 기준으로 다시 계산한다. 반면 `RuntimeGameplayDefinitionOverlay.cpp::TryParseDamageFormula`의 strict allowlist는 세 필드를 허용하지 않아 동일 canonical JSON에 대해 cook과 Debug reload의 판정이 갈라졌다.
- 수정 소유자는 Server runtime overlay parser다. F4 Client, Shared damage truth, JSON 수치, Release 생성팩은 바꾸지 않는다.

### 2. 구현 계획

#### 2-1. `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp`

`TryParseDamageFormula`의 기존 `kKnownKeys`를 아래로 교체한다.

```cpp
static constexpr const char* kKnownKeys[] =
{
    "type", "flags", "flatByRank", "totalAdRatioByRank",
    "bonusAdRatioByRank", "apRatioByRank", "targetMaxHpRatioByRank",
    "targetMissingHpRatioByRank",
    // 현재 canonical ItemGameplayDefs의 codegen 파생 metadata다.
    // runtime truth는 계속 type/flags/rank array만 사용한다.
    "cppType", "cppFlags", "rankCount",
};
```

임의 unknown field 허용으로 넓히지 않는다. 현재 canonical 파일과 cook이 이미 받아들이는 파생 metadata 세 개만 호환 입력으로 허용하고 값은 authoritative input으로 소비하지 않는다.

#### 2-2. `Tools/LoLData/Test-F4BalanceContracts.py`

runtime overlay source에서 `TryParseDamageFormula` slice를 잡아 `cppType`, `cppFlags`, `rankCount` 세 compatibility key가 각각 allowlist에 정확히 한 번 있고, `type`/`flags` 파싱은 유지되는지 고정한다. 세 metadata에 대한 `node[...]`, `node.value(...)`, `node.contains(...)` 접근은 없어야 하므로 runtime truth로 소비하지 않고 무시한다는 계약도 고정한다. actual item 3153 fixture는 세 metadata의 존재뿐 아니라 `type == "Physical"`, `flags == []`, 여섯 rank 배열 길이 `1`, `rankCount == 1`을 함께 검증한다. 이 테스트는 향후 strict runtime allowlist가 canonical/cook 입력보다 뒤처지거나 파생 metadata가 권위값으로 승격되는 회귀를 막는다.

#### 2-3. `Tools/Harness/GameRoomBotMatchSoak.cpp`

정적 문자열 검사만으로 실제 parser acceptance를 주장하지 않도록 기존 Debug `RunPracticeControlProbe`의 첫 명령으로 `ReloadGameplayDefinitions`를 실행한다. sequence는 reload/take/replace = `1/2/3`으로 조정하고, 기존 exact tool revision/replay command 기대값을 `3`으로 올린다. 기존 `ExecuteAcceptedPracticeControlCommand`가 command ACK, accepted tool revision +1, replay journal +1을 동시에 요구하므로 실제 runtime JSON parser가 하나라도 실패하면 probe가 fail-closed 된다. Release에서는 기존처럼 probe가 컴파일상 no-op이다.

### 3. 검증

```powershell
python Tools/LoLData/Test-F4BalanceContracts.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomBotMatchSoak.ps1 -Configuration Debug -TickCount 1 -Seed 42 -Runs 1 -SkipServerBuild
git diff --check -- Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp Tools/LoLData/Test-F4BalanceContracts.py .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_PLAN.md .md/plan/2026-07-18_F4_BALANCE_TUNER_DAMAGE_RESPAWN_HUD_RESULT.md
```

현재 사용자의 라이브 이렐리아 Q 값 때문에 full baseline contract가 먼저 실패하면, 동일 source-contract assertion을 별도 read-only 검사로 실행하고 그 차이를 결과서에 명시한다. 최종 수동 게이트는 새 Debug Server에서 같은 `Save & Hot Load`를 눌러 `definitions-reloaded`와 revision 증가를 확인하는 것이다.

### 4. 독립 비평 게이트

비평 주체: `/root/f4_slider_hotload_critic`, read-only. 판정은 구현 방향 Accept, 테스트 보강 조건부 Accept였다.

- **P0 없음 — 세 필드 allow/ignore 수용.** cook과 runtime 모두 실제 `type`, `flags`, 여섯 rank 배열로 값을 재구성하고 세 metadata를 truth로 신뢰하지 않는다. actual Item JSON의 non-null damage object도 3153 하나이며 추가 필드는 정확히 이 셋뿐이므로 strict unknown rejection을 유지한 채 호환할 수 있다.
- **P1 수용 — “무시” 계약 보강.** 단순 문자열 존재 검사를 넘어서 parser slice에서 세 key가 정확히 한 번 allowlist에 있고 어떠한 `node` 접근으로도 소비되지 않는지 검사한다. fixture의 authoring 값과 rankCount 일치도 함께 고정한다.
- **P1 수용 — 실제 parser acceptance 경계.** 정적 계약만으로 runtime 실행 성공을 대신 주장하지 않는다. 새 Debug Server에서 `definitions-reloaded`, revision 증가, reload failure 부재를 확인하지 못하면 수동 미검증으로 결과서에 남긴다.
- **P2 수용 — cook parity 명령.** `Build-LoLDefinitionPack.py --check`를 검증에 추가한다. 사용자 live champion JSON 때문에 stale이면 그 실패를 별도 기록하고 runtime parser 계약으로 cook parity 통과를 대체하지 않는다.

위 disposition을 계획에 반영했으므로 Item metadata Hot Load source edit gate를 통과한다.

#### 4-1. 실제 Debug reload probe 추가 비평 disposition

동일 비평 주체 `/root/f4_slider_hotload_critic`에게 2-3의 실제 command probe 확장을 다시 read-only 검토받았다. 판정은 P0 없음, 조건부 Accept이다.

- **Accept — reload/take/replace sequence `1/2/3` 및 exact tool/replay count `3`.** `ExecuteAcceptedPracticeControlCommand`가 ACK, tool revision +1, replay command +1을 함께 요구하므로 reload parser가 reject되면 즉시 fail-close한다.
- **P1 수용 — item 3153 실행 전제 고정.** compiled base pack의 item 배열이 비어 있으면 `ApplyItemsJson`이 parser를 건너뛸 수 있으므로, reload 전에 `FindItems` 결과가 비어 있지 않고 `itemId == 3153`이 실제 존재하는지 검사한다.
- **P1 수용 — runtime publish 직접 고정.** reload 전후 `GetRuntimeGameplayDefinitionRevision()`이 정확히 `+1`인지 검사해 tool revision 간접 증거뿐 아니라 runtime pack publish 자체도 고정한다.
- **P2 수용 — 진단 문구 갱신.** 최종 exact outcome 오류 문구를 `Reload/Take/Fresh`로 바꾼다.

위 조건을 2-3 구현에 반영했으므로 실제 Debug parser acceptance probe source edit gate를 통과한다.
