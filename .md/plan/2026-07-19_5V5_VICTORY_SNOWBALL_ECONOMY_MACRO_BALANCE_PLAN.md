Session - 3클라이언트+2아군 봇이 5적 봇을 상대로 성장 차이를 승리로 환전하는 경제·명령 수직 슬라이스 완성
좌표: 신규 좌표 후보 · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-18_CHAMPION_AI_AGGRESSION_MID_GROUP_YONE_COMBO_MINION_STUCK_RESULT.md, 2026-07-18_CHAMPIONAI_MID_DEFENSE_LIFECYCLE_RESULT.md

## 1. 결정 기록

① 문제·제약: 일반 6기 웨이브의 solo XP는 276.45인데 현재 5명이 붙으면 총 1,382.25가 복제되고, 아군 슬롯 3·4가 모두 Bot에 배정되며, 적 미드 외곽 포탑 파괴 후 인원 열세만 보면 두 봇 모두 미드로 자동 합류한다.
② 순진한 해법의 실패: 적 봇 스탯을 낮추거나 시작 자원을 되돌리면 사용자가 의도한 4,000G/레벨 설정과 실력→성장→오브젝트 전환 루프를 가리고, 아군 전용 보상은 서버 규칙의 팀 대칭성을 깨뜨린다.
③ 메커니즘: 미니언은 1명일 때 soloXP, 2명 이상일 때 sharedXP 총풀을 인원수로 나누고, 포탑은 killer 1,500G + killer를 제외한 같은 팀 챔피언 각 1,000G를 지급하며, 핑은 서버 검증 TTL 팀 의도로만 AI 계획에 들어간다.
④ 대조: 3 human + 2 bot 팀에서 두 봇이 모두 Bot으로 설정된 기본 상태만 Bot/Top으로 정규화하고, 공격적 자동 미드 집결은 제거한다. 적의 정상적인 미드 수비 집결은 유지하며 Assist/Danger 핑만 단기 오버라이드한다.
⑤ 대가: 포탑 1기당 팀 총 5,500G는 공정 밸런스가 아니라 의도적인 승리 가속 장치이며 어느 팀이 파괴해도 대칭 적용된다. AD 계수 상한 10은 1,000% AD까지 저작 가능하게 해 원샷 밸런스를 허용하지만 실제 계수 값은 자동 변경하지 않는다.

## 사용자 의도 확정 및 제외

- 시작 골드/레벨은 사용자가 직접 설정한 값이므로 `SpawnObjectGameplayDefs.json`과 Release override를 변경하지 않는다.
- 현재 canonical Debug는 10,000G/레벨 6, Release Server factory override는 4,000G이다. 실전 QA에서 Server Configuration과 양 팀 실제 시작 Gold/Level을 기록해 “사용자 설정 불변”을 검증한다.
- BA는 이미 `StatComponent.ad = baseAd + bonusAd`를 사용한다. 별도 BA 배율을 추가하지 않고 F4 Champions의 `baseAd/adPerLevel` 및 아이템 `flatAd`를 기존 경로로 조절한다.
- F4 상한 10은 `totalAdRatioByRank`, `bonusAdRatioByRank`, runtime `totalAdRatio`, `bonusAdRatio`에만 적용한다. AP/HP/기타 Ratio 상한 5는 유지한다.
- 애쉬 R `stunDurationSec`는 3.0초에서 2.0초로 변경한다.
- 원격 클라이언트 사이의 핑 시각 복제, 억제기 슈퍼 미니언, 전체 챔피언 TTK 재밸런싱은 이번 수직 슬라이스 밖이다.

## AI 핑 행동 계약

```text
입력: 플레이어가 기존 핑 휠을 놓으면 로컬 마커를 표시하고 TeamPing GameCommand(kind=14, slot=핑 종류, groundPos=맵 좌표)를 전송한다.
검증: 서버는 살아 있는 챔피언 발신자, 유한 좌표, walkable 해석 가능 좌표, ping kind 1..4를 확인한다.
Assist(3): 같은 팀 ChampionAIComponent에 6초 TTL 집결 목표를 설정한다. 안전 후퇴·귀환·진행 중 combo/dive가 먼저이며, 자유 상태의 봇만 좌표로 이동하고 근처 적과는 기존 전투 루프로 싸운다.
Danger(2): 핑을 받은 순간 위험 좌표 반경 12m 안인 같은 팀 봇만 eligibility를 고정한다. 선정된 봇은 이후 반경 밖으로 빠져도 3초 TTL 동안 기존 safeAnchor 쪽 후퇴 결정을 유지한다. 비선정 봇의 기존 team-ping objective는 clear된다.
OnMyWay(1), Missing(4): 서버 입력은 유효하게 기록하되 이번 슬라이스에서는 AI 계획을 바꾸지 않는다.
만료/덮어쓰기: 새 Assist/Danger가 이전 팀 핑 목표를 덮어쓰며 tick 기반 TTL 만료 후 홈 라인 계획으로 돌아간다.
권위: TeamPing은 AI 계획 상태만 바꾼다. Bot AI는 여전히 GameCommand 생산자이고 Transform/HP/Gold/Structure 진실을 직접 변경하지 않는다.
```

## 구현 직전 최종 P2 처분 및 실제 적용 보정

- ACCEPT: 핑 TTL 만료 시 objective만 지우지 않고 `decisionTimer = 0.f`도 함께 설정해 해당 틱에 홈 라인 판단을 다시 열었다.
- ACCEPT: ReplayCommandContractProbe의 기존 ReorderItem/AuthoringMutation 사례는 교체하지 않고 보존했으며, TeamPing/PlayerInput을 두 번째 command record로 추가했다. 이 때문에 record count는 4, command count는 2가 된다.
- DEFER(P2): GameRoom 통합 프로브는 `EnqueueCommand`부터 ACK/journal/checkpoint/AI Move까지 검증하지만 FlatBuffers `AcceptCommandBatch` 실제 직렬화 왕복은 별도 wire probe로 남긴다. 클라이언트 serializer와 서버 generated schema 양쪽 컴파일은 이번 빌드에서 검증한다.
- 구현 중 Release에서 Debug 전용 `IsFinitePracticePosition`을 참조하지 않도록 TeamPing handler는 `std::isfinite` 3축 검사를 직접 사용한다.
- 현재 `run_codegen.bat` 사전 빌드 훅은 병렬 MSBuild에서 동시 실행 경쟁이 있으므로 검증 빌드는 `/m:1`로 수행한다. 생성 스크립트 단독 실행과 단일 빌드는 PASS이며, 이 인프라 경쟁 수정은 이번 게임 규칙 슬라이스 범위 밖이다.

## F4 Skills 사용자 작업 계약

```text
사용자 작업: F4 > Skills에서 Total AD Ratio 또는 Bonus AD Ratio를 5 초과 10 이하로 드래그/직접 입력하고 Save & Hot Load 한다.
성공 피드백: 값이 10에서 clamp되고 dirty 상태와 기존 Save & Hot Load 상태 피드백이 유지된다.
실패 피드백: 기존 draft 검증/저장 오류 영역을 사용하며 새 저장 버튼이나 mutation 경로를 만들지 않는다.
```

| 필드 | 최소 | 최대 | 단위/의미 | 비고 |
|---|---:|---:|---|---|
| Total AD Ratio | 0 | 10 | 1.0 = 100% total AD | 기존 5에서 확대 |
| Bonus AD Ratio | 0 | 10 | 1.0 = 100% bonus AD | 기존 5에서 확대 |
| AP/Max HP/Missing HP Ratio | 0 | 5 | 1.0 = 100% | 유지 |

```text
+---------------------------------------------------------------+
| F4 > Skills                                                   |
| Champion [ ... ]  Skill [ ... ]                               |
| Value              Rank1  Rank2  Rank3  Rank4  Rank5          |
| Total AD Ratio     [0..10][0..10][0..10][0..10][0..10]       |
| Bonus AD Ratio     [0..10][0..10][0..10][0..10][0..10]       |
| AP Ratio           [0..5] ...                                 |
| ...                                                           |
| [Reload JSON]                         [Save & Hot Load]         |
+---------------------------------------------------------------+
```

- 행동 예산: 기존 `Reload JSON`과 단일 `Save & Hot Load`만 유지한다. 버튼 추가 0개, 새 mutation 경로 0개다.
- owner chain: F4 draft JSON → 기존 검증/원자 저장 → 정의 팩 코드젠/서버 hot-load → Shared DamagePipeline. 클라이언트는 피해 진실을 만들지 않는다.
- 화면 QA 진입: `Client/Bin/Debug/Client.exe`의 network-authoritative 인게임 장면에서 F4 → Skills. 제품 공식 최소 해상도/DPI는 문서에 없으므로 `CONFIRM_NEEDED`; 일단 1920×1080, Windows 100% DPI를 기준 캡처로 사용한다.
- 캡처 artifact: `.md/build/artifacts/2026-07-19_F4_AD_RATIO_10_SUCCESS.png`, `2026-07-19_F4_AD_RATIO_SAVE_FAILURE.png`, `2026-07-19_F4_SKILL_EMPTY_STATE.png`. 실행 환경에서 캡처하지 못하면 RESULT에 `UNVERIFIED`로 남긴다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Experience/ExperienceSystem.cpp

기존 코드:

```cpp
    void GrantExperienceToRecipients(
        CWorld& world, const std::vector<EntityID>& recipients, f32_t amount)
    {
        for (EntityID recipient : recipients)
            CExperienceSystem::GrantExperience(world, recipient, amount);
    }
```

아래에 추가:

```cpp
    void GrantMinionExperienceToRecipients(
        CWorld& world,
        const std::vector<EntityID>& recipients,
        const RewardDef& reward)
    {
        if (recipients.empty())
            return;

        const f32_t totalPool = recipients.size() == 1u
            ? reward.experience.nearbyXP
            : reward.experience.teamXP;
        const f32_t perRecipient =
            totalPool / static_cast<f32_t>(recipients.size());
        for (EntityID recipient : recipients)
            CExperienceSystem::GrantExperience(world, recipient, perRecipient);
    }
```

기존 코드:

```cpp
        const std::vector<EntityID> recipients = CollectNearbyExperienceRecipients(
            world, rewardTeam, killer, victim, ResolveExperienceShareRadius(*reward));
        GrantExperienceToRecipients(world, recipients, reward->experience.nearbyXP);

        return;
```

위 블록 중 첫 번째 미니언 분기만 아래로 교체:

```cpp
        const std::vector<EntityID> recipients = CollectNearbyExperienceRecipients(
            world, rewardTeam, killer, victim, ResolveExperienceShareRadius(*reward));
        GrantMinionExperienceToRecipients(world, recipients, *reward);

        return;
```

기존 코드:

```cpp
        if (world.HasComponent<ChampionComponent>(killer))
        {
            const u32_t goldAmount = GrantGold(world, killer, reward->gold.killerGold);
            (void)GameplayFeedback::EnqueueGoldRewardFeedback(world, tc, killer, victim, goldAmount);
        }
        return;
```

아래로 교체:

```cpp
        const bool_t bChampionKiller =
            world.HasComponent<ChampionComponent>(killer);
        if (bChampionKiller)
        {
            const u32_t goldAmount =
                GrantGold(world, killer, reward->gold.killerGold);
            (void)GameplayFeedback::EnqueueGoldRewardFeedback(
                world, tc, killer, victim, goldAmount);
        }

        world.ForEach<ChampionComponent>(
            [&](EntityID entity, ChampionComponent& champion)
            {
                if (champion.team != rewardTeam ||
                    (bChampionKiller && entity == killer))
                {
                    return;
                }

                const u32_t goldAmount =
                    GrantGold(world, entity, reward->gold.teamGold);
                (void)GameplayFeedback::EnqueueGoldRewardFeedback(
                    world, tc, entity, victim, goldAmount);
            });
        return;
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/EconomyGameplayDef.h

기존 코드:

```cpp
    f32_t turretGold = 250.f;
    EconomyJungleRewardDef jungle{};
```

아래로 교체:

```cpp
    f32_t turretGold = 1500.f;
    f32_t turretTeamGold = 1000.f;
    EconomyJungleRewardDef jungle{};
```

### 2-3. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json

기존 코드:

```json
  "turretGold": 250.0,
```

아래로 교체:

```json
  "turretGold": 1500.0,
  "turretTeamGold": 1000.0,
```

### 2-3a. C:/Users/user/Desktop/Winters/Data/LoL/Schemas/EconomyGameplayDefs.json.schema.json

`required` 배열의 기존 코드:

```json
    "minions",
    "turretGold",
    "jungle",
```

아래로 교체:

```json
    "minions",
    "turretGold",
    "turretTeamGold",
    "jungle",
```

`properties` 객체의 기존 코드:

```json
    "turretGold": {
      "type": "number"
    },
```

아래로 교체:

```json
    "turretGold": {
      "type": "number"
    },
    "turretTeamGold": {
      "type": "number"
    },
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

기존 코드:

```python
        "turretGold": validate_economy_number(
            legacy.as_float(root.get("turretGold", 0.0), "turretGold"), "turretGold"
        ),
        "jungle": normalized_group("jungle", ECONOMY_JUNGLE_FIELDS),
```

아래로 교체:

```python
        "turretGold": validate_economy_number(
            legacy.as_float(root.get("turretGold", 0.0), "turretGold"), "turretGold"
        ),
        "turretTeamGold": validate_economy_number(
            legacy.as_float(root.get("turretTeamGold", 0.0), "turretTeamGold"),
            "turretTeamGold",
        ),
        "jungle": normalized_group("jungle", ECONOMY_JUNGLE_FIELDS),
```

기존 코드:

```python
    lines.append(f"        def.turretGold = {cpp_float(economy_data['turretGold'])};")
    jungle = economy_data["jungle"]
```

아래로 교체:

```python
    lines.append(f"        def.turretGold = {cpp_float(economy_data['turretGold'])};")
    lines.append(
        f"        def.turretTeamGold = {cpp_float(economy_data['turretTeamGold'])};"
    )
    jungle = economy_data["jungle"]
```

### 2-4a. C:/Users/user/Desktop/Winters/Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp

`ApplyEconomyJson` known-root 목록의 기존 코드:

```cpp
            "xpCurve", "championKill", "minions", "turretGold",
            "jungle", "passiveGold", "timers",
```

아래로 교체:

```cpp
            "xpCurve", "championKill", "minions", "turretGold",
            "turretTeamGold", "jungle", "passiveGold", "timers",
```

기존 `turretGold` overlay 블록 바로 아래에 추가:

```cpp
        if (!TryOverlayNumber(
                root,
                "turretTeamGold",
                0.f,
                1000000.f,
                economy.turretTeamGold,
                fieldError))
        {
            outError = fieldError;
            return false;
        }
```

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Registries/Reward/RewardRegistry.cpp

기존 코드:

```cpp
    RewardDef MakeTurretReward(f32_t killerGold)
    {
        // 봇 가치판단(ChampionAIValuation::GetTurretGoldValue)이 이 레지스트리를
        // 조회하므로 실보상과 갈라지지 않는다.
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Turret;
        reward.gold.killerGold = killerGold;
        return reward;
    }
```

아래로 교체:

```cpp
    RewardDef MakeTurretReward(f32_t killerGold, f32_t teamGold)
    {
        // 봇 가치판단(ChampionAIValuation::GetTurretGoldValue)이 이 레지스트리를
        // 조회하므로 실보상과 갈라지지 않는다.
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Turret;
        reward.gold.killerGold = killerGold;
        reward.gold.teamGold = teamGold;
        return reward;
    }
```

기존 코드:

```cpp
    AddReward(MakeTurretReward(250.f));
```

아래로 교체:

```cpp
    AddReward(MakeTurretReward(1500.f, 1000.f));
```

기존 코드:

```cpp
    AddReward(MakeTurretReward(economy.turretGold));
```

아래로 교체:

```cpp
    AddReward(MakeTurretReward(economy.turretGold, economy.turretTeamGold));
```

### 2-6. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

`"key": "skill.ashe.r"` 객체의 기존 코드:

```json
        "stunDurationSec": 3.0
```

아래로 교체:

```json
        "stunDurationSec": 2.0
```

### 2-7. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

기존 코드:

```cpp
	constexpr f32_t kMinionAttackRangeMin = 0.1f;
	constexpr f32_t kMinionAttackRangeMax = 100.f;
	constexpr size_t kMaxPracticeOverrides = 32u;
```

아래로 교체:

```cpp
	constexpr f32_t kMinionAttackRangeMin = 0.1f;
	constexpr f32_t kMinionAttackRangeMax = 100.f;
	constexpr f32_t kAdRatioAuthoringMax = 10.f;
	constexpr size_t kMaxPracticeOverrides = 32u;
```

기존 코드:

```cpp
						DrawRankRow(
							"Total AD Ratio", damage, "totalAdRatioByRank",
							0.01f, 0.f, 5.f, "%.2f", false,
							draft.bSkillEffectDirty);
						DrawRankRow(
							"Bonus AD Ratio", damage, "bonusAdRatioByRank",
							0.01f, 0.f, 5.f, "%.2f", false,
							draft.bSkillEffectDirty);
```

아래로 교체:

```cpp
						DrawRankRow(
							"Total AD Ratio", damage, "totalAdRatioByRank",
							0.01f, 0.f, kAdRatioAuthoringMax, "%.2f", false,
							draft.bSkillEffectDirty);
						DrawRankRow(
							"Bonus AD Ratio", damage, "bonusAdRatioByRank",
							0.01f, 0.f, kAdRatioAuthoringMax, "%.2f", false,
							draft.bSkillEffectDirty);
```

기존 코드:

```cpp
								const bool_t bRatio = std::strstr(pParam, "Ratio") != nullptr;
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

아래로 교체:

```cpp
								const bool_t bRatio =
									std::strstr(pParam, "Ratio") != nullptr;
								const bool_t bAdRatio =
									std::strcmp(pParam, "totalAdRatio") == 0 ||
									std::strcmp(pParam, "bonusAdRatio") == 0;
								EditDragFloat(
									params,
									pParam,
									pParam,
									bRatio ? 0.01f : 1.f,
									0.f,
									bAdRatio
										? kAdRatioAuthoringMax
										: (bRatio ? 5.f : 2000.f),
									bRatio ? "%.2f" : "%.1f",
									draft.bSkillEffectDirty);
```

### 2-8. C:/Users/user/Desktop/Winters/Tools/LoLData/Test-F4BalanceContracts.py

기존 코드:

```python
    for call_contract in (
            '0.1f, 0.f, 300.f, "%.1f s"',
            '1.f, 0.f, 2000.f, "%.0f"',
            '0.01f, 0.f, 5.f, "%.2f"'):
        require(call_contract in skills_surface,
                f"rank DragFloat contract {call_contract}")
```

아래로 교체:

```python
    for call_contract in (
            '0.1f, 0.f, 300.f, "%.1f s"',
            '1.f, 0.f, 2000.f, "%.0f"',
            '0.01f, 0.f, 5.f, "%.2f"'):
        require(call_contract in skills_surface,
                f"rank DragFloat contract {call_contract}")
    require("kAdRatioAuthoringMax = 10.f" in tuner,
            "F4 AD ratio authoring maximum is 10")
    require(skills_surface.count("kAdRatioAuthoringMax") >= 3,
            "ranked and runtime Total/Bonus AD editors use the 10x maximum")
```

### 2-9. C:/Users/user/Desktop/Winters/Server/Private/Game/LobbyAuthority.cpp

익명 namespace의 `GetDefaultLobbyBotLane` 함수 바로 아래에 추가:

```cpp
    void ApplyThreeHumanTwoBotLanePreset(
        LobbySlotState* pSlots,
        u32_t slotCount)
    {
        if (!pSlots)
            return;

        for (u8_t team = 0u; team < 2u; ++team)
        {
            u32_t humanCount = 0u;
            u32_t botCount = 0u;
            LobbySlotState* pBots[2]{};
            for (u32_t i = 0u; i < slotCount; ++i)
            {
                LobbySlotState& slot = pSlots[i];
                if (slot.team != team)
                    continue;
                humanCount += slot.bHuman ? 1u : 0u;
                if (slot.bBot)
                {
                    if (botCount < 2u)
                        pBots[botCount] = &slot;
                    ++botCount;
                }
            }

            if (humanCount == 3u &&
                botCount == 2u &&
                pBots[0] &&
                pBots[1] &&
                pBots[0]->botLane == kGameSimLaneBot &&
                pBots[1]->botLane == kGameSimLaneBot)
            {
                pBots[0]->botLane = kGameSimLaneBot;
                pBots[1]->botLane = kGameSimLaneTop;
            }
        }
    }
```

`TryStartGame` special-roster 분기 바로 아래, slot lock loop 직전에 추가:

```cpp
    ApplyThreeHumanTwoBotLanePreset(m_slots, kGameRosterSlotCount);
```

이 규칙은 3 human + 2 bot인 팀의 두 봇이 여전히 모두 Bot일 때만 동작한다. 다른 로스터와 호스트가 이미 Top/Bot으로 나눈 설정은 보존하며, Red 5-bot 기본 라인은 변경하지 않는다.

### 2-9a. 새 파일: C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/TeamPingDef.h

전체 파일 본문:

```cpp
#pragma once

#include "WintersTypes.h"

enum class eTeamPingKind : u8_t
{
	None = 0,
	OnMyWay = 1,
	Danger = 2,
	Assist = 3,
	Missing = 4,
};

inline constexpr f32_t kTeamPingDangerRadius = 12.f;
```

### 2-9b. C:/Users/user/Desktop/Winters/Shared/GameSim/Include/GameSim.vcxproj

기존 코드:

```xml
    <ClInclude Include="..\Definitions\SummonerSpellGameplayDef.h" />
    <ClInclude Include="..\Definitions\WardDefinitions.h" />
```

아래로 교체:

```xml
    <ClInclude Include="..\Definitions\SummonerSpellGameplayDef.h" />
    <ClInclude Include="..\Definitions\TeamPingDef.h" />
    <ClInclude Include="..\Definitions\WardDefinitions.h" />
```

### 2-9c. C:/Users/user/Desktop/Winters/Shared/GameSim/Include/GameSim.vcxproj.filters

기존 코드:

```xml
    <ClInclude Include="..\Definitions\SummonerSpellGameplayDef.h">
      <Filter>Definitions</Filter>
    </ClInclude>
    <ClInclude Include="..\Definitions\WardDefinitions.h">
```

아래로 교체:

```xml
    <ClInclude Include="..\Definitions\SummonerSpellGameplayDef.h">
      <Filter>Definitions</Filter>
    </ClInclude>
    <ClInclude Include="..\Definitions\TeamPingDef.h">
      <Filter>Definitions</Filter>
    </ClInclude>
    <ClInclude Include="..\Definitions\WardDefinitions.h">
```

### 2-10. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

기존 코드:

```fbs
    PracticeControl = 12,
    ReorderItem = 13
}
```

아래로 교체:

```fbs
    PracticeControl = 12,
    ReorderItem = 13,
    TeamPing = 14
}
```

### 2-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

기존 코드:

```cpp
    PracticeControl = 12,
    ReorderItem = 13,
};
```

아래로 교체:

```cpp
    PracticeControl = 12,
    ReorderItem = 13,
    TeamPing = 14,
};
```

### 2-12. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

파일 상단의 기존 코드:

```cpp
#include "WintersMath.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
```

아래로 교체:

```cpp
#include "WintersMath.h"
#include "Shared/GameSim/Definitions/TeamPingDef.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
```

기존 코드:

```cpp
	u32_t SendBasicAttack(CClientNetwork& net, NetEntityId targetNet,
		const Vec3& groundPos = {}, const Vec3& direction = {});
```

아래에 추가:

```cpp
	u32_t SendTeamPing(CClientNetwork& net, eTeamPingKind kind,
		const Vec3& groundPos);
```

### 2-13. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

`GetCommandKindName`의 기존 코드:

```cpp
        case eCommandKind::ReorderItem:
            return "ReorderItem";
        default:
```

아래로 교체:

```cpp
        case eCommandKind::ReorderItem:
            return "ReorderItem";
        case eCommandKind::TeamPing:
            return "TeamPing";
        default:
```

기존 `SendBasicAttack` 함수 바로 아래에 추가:

```cpp
u32_t CCommandSerializer::SendTeamPing(
    CClientNetwork& net,
    eTeamPingKind kind,
    const Vec3& groundPos)
{
    if (kind <= eTeamPingKind::None ||
        kind > eTeamPingKind::Missing ||
        !IsValidMoveGroundPos(groundPos))
    {
        return 0u;
    }

    GameCommandWire wire{};
    wire.kind = eCommandKind::TeamPing;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.slot = static_cast<u8_t>(kind);
    wire.groundPos = groundPos;
    SendSingle(net, wire);
    return wire.sequenceNum;
}
```

### 2-14. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameInput.cpp

기존 코드:

```cpp
        CGameInstance::Get()->UI_Push_MapPing(m_vPingWheelWorldPos, iDirection);
        m_bPingWheelActive = false;
```

아래로 교체:

```cpp
        CGameInstance::Get()->UI_Push_MapPing(m_vPingWheelWorldPos, iDirection);
        if (iDirection != 0u && IsNetworkAuthoritativeGameplay())
        {
            CCommandSerializer* pCommandSerializer = GetCommandSerializer();
            CClientNetwork* pNetworkView = GetNetworkView();
            if (pCommandSerializer &&
                pNetworkView &&
                pNetworkView->IsConnected())
            {
                pCommandSerializer->SendTeamPing(
                    *pNetworkView,
                    static_cast<eTeamPingKind>(iDirection),
                    m_vPingWheelWorldPos);
            }
        }
        m_bPingWheelActive = false;
```

### 2-15. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIInfluenceMap.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/TeamPingDef.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIInfluenceMap.h"
```

기존 코드:

```cpp
	f32_t midDefenseThreatHoldTimer = 0.f;
	bool_t bMidTeamfightActive = false;
	bool_t bYonePostReturnUltimatePending = false;
	u8_t reservedMidDefenseAlignment[2]{};
};
```

아래로 교체:

```cpp
	f32_t midDefenseThreatHoldTimer = 0.f;
	bool_t bMidTeamfightActive = false;
	bool_t bYonePostReturnUltimatePending = false;
	u8_t reservedMidDefenseAlignment[2]{};

	u64_t teamPingExpireTick = 0u;
	Vec3 teamPingAnchor{};
	eTeamPingKind teamPingKind = eTeamPingKind::None;
	bool_t bTeamPingObjectiveActive = false;
	u8_t reservedTeamPingAlignment[2]{};
};
```

기존 코드:

```cpp
static_assert(sizeof(ChampionAIComponent) == 2992u);
```

아래로 교체:

```cpp
static_assert(offsetof(ChampionAIComponent, teamPingExpireTick) == 2992u);
static_assert(offsetof(ChampionAIComponent, teamPingAnchor) == 3000u);
static_assert(offsetof(ChampionAIComponent, teamPingKind) == 3012u);
static_assert(sizeof(ChampionAIComponent) == 3016u);
```

### 2-16. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    bool_t TryHandlePracticeControl(
        const TickContext& tc,
        const GameCommand& cmd,
        bool_t& outAccepted);
```

기존 `RecordPendingReplayCommand` 선언 블록 아래에 추가:

```cpp
    bool_t TryHandleTeamPing(
        const TickContext& tc,
        const GameCommand& cmd,
        bool_t& outAccepted);
```

### 2-17. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`EnqueueCommand` 함수 전체 아래에 추가:

```cpp
bool_t CGameRoom::TryHandleTeamPing(
    const TickContext& tc,
    const GameCommand& cmd,
    bool_t& outAccepted)
{
    outAccepted = false;
    if (cmd.kind != eCommandKind::TeamPing)
        return false;

    if (cmd.issuerEntity == NULL_ENTITY ||
        !m_world.IsAlive(cmd.issuerEntity) ||
        !m_world.HasComponent<ChampionComponent>(cmd.issuerEntity) ||
        !m_world.HasComponent<HealthComponent>(cmd.issuerEntity) ||
        !m_world.HasComponent<TransformComponent>(cmd.issuerEntity) ||
        !std::isfinite(cmd.groundPos.x) ||
        !std::isfinite(cmd.groundPos.y) ||
        !std::isfinite(cmd.groundPos.z))
    {
        return true;
    }

    const auto& health =
        m_world.GetComponent<HealthComponent>(cmd.issuerEntity);
    if (health.bIsDead || health.fCurrent <= 0.f)
        return true;

    const eTeamPingKind kind = static_cast<eTeamPingKind>(cmd.slot);
    if (kind <= eTeamPingKind::None || kind > eTeamPingKind::Missing)
        return true;

    const auto& issuerChampion =
        m_world.GetComponent<ChampionComponent>(cmd.issuerEntity);
    const Vec3 issuerPos =
        m_world.GetComponent<TransformComponent>(cmd.issuerEntity).GetPosition();
    Vec3 resolvedGround{};
    if (!TryResolveMoveTarget(issuerPos, cmd.groundPos, resolvedGround))
        return true;

    outAccepted = true;
    if (kind != eTeamPingKind::Assist && kind != eTeamPingKind::Danger)
        return true;

    const u64_t ttlTicks = kind == eTeamPingKind::Assist
        ? 6ull * DeterministicTime::kTicksPerSecond
        : 3ull * DeterministicTime::kTicksPerSecond;
    m_world.ForEach<ChampionAIComponent, ChampionComponent, TransformComponent>(
        [&](EntityID,
            ChampionAIComponent& ai,
            ChampionComponent& champion,
            TransformComponent& transform)
        {
            if (champion.team != issuerChampion.team)
                return;

            if (kind == eTeamPingKind::Danger)
            {
                ai.teamPingExpireTick = 0u;
                ai.teamPingAnchor = Vec3{};
                ai.teamPingKind = eTeamPingKind::None;
                ai.bTeamPingObjectiveActive = false;
                ai.decisionTimer = 0.f;

                const f32_t dangerRadiusSq =
                    kTeamPingDangerRadius * kTeamPingDangerRadius;
                if (WintersMath::DistanceSqXZ(
                        transform.GetPosition(), resolvedGround) > dangerRadiusSq)
                {
                    return;
                }
            }

            ai.teamPingKind = kind;
            ai.teamPingAnchor = resolvedGround;
            ai.teamPingExpireTick = tc.tickIndex + ttlTicks;
            ai.bTeamPingObjectiveActive = true;
            ai.decisionTimer = 0.f;
        });
    return true;
}
```

### 2-18. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

`Phase_ExecuteCommands` 루프의 기존 코드:

```cpp
		bool_t bAccepted = false;
		if (TryHandlePracticeControl(tc, cmd, bAccepted))
```

아래로 교체:

```cpp
		bool_t bAccepted = false;
		if (TryHandleTeamPing(tc, cmd, bAccepted))
		{
			RecordPendingReplayCommand(
				tc.tickIndex,
				cmd,
				Winters::Replay::eReplayJournalOutcome::SubmittedPlayerInput);
			continue;
		}
		if (TryHandlePracticeControl(tc, cmd, bAccepted))
```

### 2-19. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

namespace 상수의 기존 코드:

```cpp
    constexpr f32_t kChampionAIMidTeamfightRadius = 18.f;
```

아래에 추가:

```cpp
    constexpr f32_t kChampionAITeamPingArrivalRadius = 2.5f;
```

기존 코드:

```cpp
    constexpr u8_t kChampionAIMidTeamfightEnemyMinimum = 2u;
```

삭제할 코드:

```cpp
    constexpr u8_t kChampionAIMidTeamfightEnemyMinimum = 2u;
```

`ExecuteLaneCombat` 함수 전체 아래에 추가:

```cpp
    void ClearTeamPingObjective(ChampionAIComponent& ai)
    {
        ai.teamPingExpireTick = 0u;
        ai.teamPingAnchor = Vec3{};
        ai.teamPingKind = eTeamPingKind::None;
        ai.bTeamPingObjectiveActive = false;
    }

    bool_t TryExecuteTeamPingObjective(
        CWorld& world,
        const TickContext& tc,
        EntityID self,
        ChampionAIComponent& ai,
        ChampionComponent& champion,
        const Vec3& selfPos,
        const ChampionAIContext& ctx,
        std::vector<GameCommand>& outCommands)
    {
        if (!ai.bTeamPingObjectiveActive)
            return false;
        if (tc.tickIndex >= ai.teamPingExpireTick)
        {
            ClearTeamPingObjective(ai);
            return false;
        }
        if (ai.comboTarget != NULL_ENTITY ||
            ai.divePhase != eChampionAIDivePhase::None)
        {
            return false;
        }

        if (ai.teamPingKind == eTeamPingKind::Danger)
        {
            (void)EmitMoveCommand(
                world,
                tc,
                self,
                ai,
                champion.id,
                selfPos,
                ai.safeAnchor,
                eChampionAIAction::Retreat,
                "team-ping-danger",
                outCommands);
            // 동일 safeAnchor 명령이 이미 있어 emit이 false여도
            // TTL/반경 내에서는 일반 macro가 후퇴를 덮지 못한다.
            return true;
        }

        if (ai.teamPingKind != eTeamPingKind::Assist)
            return false;
        if (ctx.enemyChampion != NULL_ENTITY)
        {
            ExecuteLaneCombat(
                world,
                tc,
                self,
                ai,
                champion,
                selfPos,
                ctx,
                outCommands);
            return true;
        }

        const f32_t arrivalRadiusSq =
            kChampionAITeamPingArrivalRadius *
            kChampionAITeamPingArrivalRadius;
        if (WintersMath::DistanceSqXZ(selfPos, ai.teamPingAnchor) <=
            arrivalRadiusSq)
        {
            return true;
        }
        (void)EmitMoveCommand(
            world,
            tc,
            self,
            ai,
            champion.id,
            selfPos,
            ai.teamPingAnchor,
            eChampionAIAction::FollowWave,
            "team-ping-assist",
            outCommands);
        // 동일 목표로 이미 이동 중이어도 일반 macro를 소비한다.
        return true;
    }
```

`CChampionAISystem::Execute` 내 timer 갱신의 기존 코드:

```cpp
            ai.midDefenseThreatHoldTimer =
                std::max(0.f, ai.midDefenseThreatHoldTimer - tc.fDt);

            const Vec3 selfPos = selfTf.GetPosition();
```

아래로 교체해 decisionTimer 조기 반환에 관계없이 tick-based TTL을 정확히 닫는다:

```cpp
            ai.midDefenseThreatHoldTimer =
                std::max(0.f, ai.midDefenseThreatHoldTimer - tc.fDt);
            if (ai.bTeamPingObjectiveActive &&
                tc.tickIndex >= ai.teamPingExpireTick)
            {
                ClearTeamPingObjective(ai);
            }

            const Vec3 selfPos = selfTf.GetPosition();
```

`TryEmitItemPurchase`와 기존 Retreat 연속 처리 뒤, 자동 mid macro 앞에 추가:

```cpp
            if (TryExecuteTeamPingObjective(
                world,
                tc,
                self,
                ai,
                champion,
                selfPos,
                ctx,
                outCommands))
            {
                return;
            }
```

기존 코드:

```cpp
                const bool_t bOffensiveMidGroupStillNeeded =
                    ctx.bEnemyMidOuterTurretLost &&
                    ctx.enemyMidChampionCount >=
                        kChampionAIMidTeamfightEnemyMinimum &&
                    ctx.alliedMidChampionCount <
                        ctx.enemyMidChampionCount;
                const bool_t bShouldRefreshMidGroup =
                    ai.bMidTeamfightActive
                        ? bOffensiveMidGroupStillNeeded
                        : HasMidDefenseThreat(
                            world,
                            champion.team,
                            ctx.midDefenseAnchor);
```

아래로 교체:

```cpp
                const bool_t bShouldRefreshMidGroup =
                    !ai.bMidTeamfightActive &&
                    HasMidDefenseThreat(
                        world,
                        champion.team,
                        ctx.midDefenseAnchor);
```

기존 코드:

```cpp
            const bool_t bOffensiveMidGroupNeeded =
                bMacroRefreshDue &&
                ctx.bEnemyMidOuterTurretLost &&
                ctx.enemyMidChampionCount >=
                    kChampionAIMidTeamfightEnemyMinimum &&
                ctx.alliedMidChampionCount < ctx.enemyMidChampionCount;
            const bool_t bDefensiveMidGroupNeeded =
```

아래로 교체:

```cpp
            const bool_t bDefensiveMidGroupNeeded =
```

기존 코드:

```cpp
            if (!ai.bMidDefenseActive &&
                (bOffensiveMidGroupNeeded || bDefensiveMidGroupNeeded) &&
```

아래로 교체:

```cpp
            if (!ai.bMidDefenseActive &&
                bDefensiveMidGroupNeeded &&
```

기존 코드:

```cpp
                ai.bMidTeamfightActive = bOffensiveMidGroupNeeded;
```

아래로 교체:

```cpp
                ai.bMidTeamfightActive = false;
```

### 2-19a. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp — 양 팀 macro fixture 업데이트

`RunChampionAIMidDefenseDeterminismProbe` 내 lambda 이름의 기존 코드:

```cpp
        const auto RunOffensiveMidGroupCase = [&](u64_t seed) -> u64_t
```

아래로 교체:

```cpp
        const auto RunSideFarmWhileEnemyGroupsMidCase = [&](u64_t seed) -> u64_t
```

같은 lambda의 기존 검증 블록:

```cpp
            const bool_t bMoveToWave =
                groupCommands.size() == 1u &&
                groupCommands[0].kind == eCommandKind::Move &&
                WintersMath::DistanceSqXZ(
                    groupCommands[0].groundPos,
                    Vec3{ 18.f, 0.f, 10.f }) <= 0.0001f;
            if (!currentAI.bMidDefenseActive ||
                !currentAI.bMidTeamfightActive ||
                currentAI.lane !=
                    static_cast<u8_t>(Winters::Map::eLane::Bot) ||
                currentAI.activeLane !=
                    static_cast<u8_t>(Winters::Map::eLane::Mid) ||
                currentAI.state != eChampionAIState::GroupMidDefense ||
                !bMoveToWave)
```

아래로 교체:

```cpp
            const bool_t bMoveToMidWave =
                groupCommands.size() == 1u &&
                groupCommands[0].kind == eCommandKind::Move &&
                WintersMath::DistanceSqXZ(
                    groupCommands[0].groundPos,
                    Vec3{ 18.f, 0.f, 10.f }) <= 0.0001f;
            if (currentAI.bMidDefenseActive ||
                currentAI.bMidTeamfightActive ||
                currentAI.lane !=
                    static_cast<u8_t>(Winters::Map::eLane::Bot) ||
                currentAI.activeLane !=
                    static_cast<u8_t>(Winters::Map::eLane::Bot) ||
                currentAI.state == eChampionAIState::GroupMidDefense ||
                bMoveToMidWave)
```

실패 로그의 `offensive group` 문구는 `side farm while enemy groups mid`로, `moveToWave`는 `moveToMidWave`로 교체한다. hash tail의 기존 코드:

```cpp
            HashF32(hash, groupCommands[0].groundPos.x);
            HashF32(hash, groupCommands[0].groundPos.z);
```

아래로 교체:

```cpp
            HashU64(hash, static_cast<u64_t>(groupCommands.size()));
            if (!groupCommands.empty())
            {
                HashF32(hash, groupCommands[0].groundPos.x);
                HashF32(hash, groupCommands[0].groundPos.z);
            }
```

lambda 호출/검증의 기존 코드:

```cpp
        const u64_t offensiveGroupHashA =
            RunOffensiveMidGroupCase(20260728ull);
        const u64_t offensiveGroupHashB =
            RunOffensiveMidGroupCase(20260728ull);
        if (offensiveGroupHashA == 0ull ||
            offensiveGroupHashA != offensiveGroupHashB)
```

아래로 교체:

```cpp
        const u64_t sideFarmHashA =
            RunSideFarmWhileEnemyGroupsMidCase(20260728ull);
        const u64_t sideFarmHashB =
            RunSideFarmWhileEnemyGroupsMidCase(20260728ull);
        if (sideFarmHashA == 0ull || sideFarmHashA != sideFarmHashB)
```

실패 로그의 두 hash 인자도 `sideFarmHashA/B`로 교체한다. 이 fixture는 “Red 미드 외곽 파괴 + Red 3명 미드”에서 Blue Bot side bot이 라인을 유지함을 검증한다. 같은 probe의 기존 defensive fixture는 자기 팀 포탑 손실+미드 위협 시 수비 집결이 계속 동작함을 보장한다.

### 2-20. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunPracticeMinionAttackDamagePolicyProbe` 함수 바로 아래에 `RunVictoryEconomyAndPingProbe`를 추가한다. 함수 전문:

```cpp
    bool_t RunVictoryEconomyAndPingProbe()
    {
        const auto NearlyEqual = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.001f;
        };

        constexpr eMinionRewardKind kRewardKinds[] = {
            eMinionRewardKind::Melee,
            eMinionRewardKind::Ranged,
            eMinionRewardKind::Siege,
            eMinionRewardKind::Super,
        };
        for (u8_t roleType = 0u; roleType < 4u; ++roleType)
        {
            for (u8_t recipientCount : { 1u, 2u, 5u })
            {
                CWorld world;
                DeterministicRng rng(
                    2026071900ull + roleType * 10ull + recipientCount);
                EntityIdMap entityMap;
                FlatWalkable walkable;
                std::vector<EntityID> recipients;
                std::vector<u32_t> goldBefore;
                for (u8_t slot = 0u; slot < recipientCount; ++slot)
                {
                    const EntityID recipient = SpawnChampion(
                        world,
                        entityMap,
                        eChampion::GAREN,
                        static_cast<u8_t>(eTeam::Blue),
                        slot);
                    world.GetComponent<TransformComponent>(recipient).SetPosition(
                        Vec3{ static_cast<f32_t>(slot), 0.f, 0.f });
                    recipients.push_back(recipient);
                    goldBefore.push_back(
                        world.GetComponent<GoldComponent>(recipient).amount);
                }

                const EntityID farAlly = SpawnChampion(
                    world, entityMap, eChampion::JAX,
                    static_cast<u8_t>(eTeam::Blue), 6u);
                world.GetComponent<TransformComponent>(farAlly).SetPosition(
                    Vec3{ 100.f, 0.f, 0.f });
                const EntityID deadAlly = SpawnChampion(
                    world, entityMap, eChampion::YASUO,
                    static_cast<u8_t>(eTeam::Blue), 7u);
                world.GetComponent<TransformComponent>(deadAlly).SetPosition(
                    Vec3{});
                HealthComponent& deadHealth =
                    world.GetComponent<HealthComponent>(deadAlly);
                deadHealth.fCurrent = 0.f;
                deadHealth.bIsDead = true;
                const EntityID nearEnemy = SpawnChampion(
                    world, entityMap, eChampion::RIVEN,
                    static_cast<u8_t>(eTeam::Red), 8u);
                world.GetComponent<TransformComponent>(nearEnemy).SetPosition(
                    Vec3{});

                const EntityID victim = world.CreateEntity();
                TransformComponent victimTransform{};
                victimTransform.SetPosition(Vec3{});
                world.AddComponent<TransformComponent>(victim, victimTransform);
                MinionComponent minion{};
                minion.team = eTeam::Red;
                minion.roleType = roleType;
                world.AddComponent<MinionComponent>(victim, minion);

                TickContext tc = MakeProbeTickContext(
                    1ull, rng, entityMap, walkable);
                CExperienceSystem::GrantKillRewards(
                    world, tc, recipients.front(), victim);
                const RewardDef* reward = CRewardRegistry::Instance().FindReward(
                    eRewardSourceKind::Minion,
                    static_cast<u8_t>(kRewardKinds[roleType]));
                if (!reward)
                    return false;
                const f32_t expectedPool = recipientCount == 1u
                    ? reward->experience.nearbyXP
                    : reward->experience.teamXP;
                const f32_t expectedPerRecipient =
                    expectedPool / static_cast<f32_t>(recipientCount);
                for (u32_t i = 0u; i < recipients.size(); ++i)
                {
                    const EntityID recipient = recipients[i];
                    if (!NearlyEqual(
                            world.GetComponent<ExperienceComponent>(recipient).total,
                            expectedPerRecipient) ||
                        (i == 0u &&
                            world.GetComponent<GoldComponent>(recipient).amount <=
                                goldBefore[i]) ||
                        (i != 0u &&
                            world.GetComponent<GoldComponent>(recipient).amount !=
                                goldBefore[i]))
                    {
                        return false;
                    }
                }
                if (!NearlyEqual(
                        world.GetComponent<ExperienceComponent>(farAlly).total,
                        0.f) ||
                    !NearlyEqual(
                        world.GetComponent<ExperienceComponent>(deadAlly).total,
                        0.f) ||
                    !NearlyEqual(
                        world.GetComponent<ExperienceComponent>(nearEnemy).total,
                        0.f))
                {
                    return false;
                }
            }
        }

        {
            CWorld world;
            DeterministicRng rng(2026071955ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            std::vector<EntityID> blue;
            std::vector<u32_t> blueGoldBefore;
            for (u8_t slot = 0u; slot < 5u; ++slot)
            {
                const EntityID champion = SpawnChampion(
                    world,
                    entityMap,
                    eChampion::GAREN,
                    static_cast<u8_t>(eTeam::Blue),
                    slot);
                blue.push_back(champion);
                blueGoldBefore.push_back(
                    world.GetComponent<GoldComponent>(champion).amount);
            }
            const EntityID red = SpawnChampion(
                world,
                entityMap,
                eChampion::JAX,
                static_cast<u8_t>(eTeam::Red),
                5u);
            const u32_t redGoldBefore =
                world.GetComponent<GoldComponent>(red).amount;
            const EntityID turret = world.CreateEntity();
            world.AddComponent<TurretAIComponent>(turret, TurretAIComponent{});
            TickContext tc = MakeProbeTickContext(
                1ull, rng, entityMap, walkable);
            CExperienceSystem::GrantKillRewards(
                world, tc, blue.front(), turret);
            for (u32_t i = 0u; i < blue.size(); ++i)
            {
                const u32_t expected = blueGoldBefore[i] +
                    (i == 0u ? 1500u : 1000u);
                if (world.GetComponent<GoldComponent>(blue[i]).amount != expected)
                    return false;
            }
            if (world.GetComponent<GoldComponent>(red).amount != redGoldBefore)
                return false;
        }

        {
            CWorld world;
            DeterministicRng rng(2026071966ull);
            EntityIdMap entityMap;
            FlatWalkable walkable;
            const EntityID bot = SpawnChampion(
                world,
                entityMap,
                eChampion::GAREN,
                static_cast<u8_t>(eTeam::Blue),
                0u);
            world.GetComponent<GoldComponent>(bot).amount = 0u;
            world.GetComponent<SkillRankComponent>(bot).pointsAvailable = 0u;
            world.GetComponent<TransformComponent>(bot).SetPosition(Vec3{});
            ChampionAIComponent ai{};
            ai.champion = eChampion::GAREN;
            ai.team = eTeam::Blue;
            ai.lane = static_cast<u8_t>(Winters::Map::eLane::Bot);
            ai.activeLane = ai.lane;
            ai.laneGoal = Vec3{ 0.f, 0.f, 20.f };
            ai.safeAnchor = Vec3{ -10.f, 0.f, 0.f };
            ai.teamPingExpireTick = 180u;
            ai.teamPingAnchor = Vec3{ 20.f, 0.f, 0.f };
            ai.teamPingKind = eTeamPingKind::Assist;
            ai.bTeamPingObjectiveActive = true;
            world.AddComponent<ChampionAIComponent>(bot, ai);
            TickContext tc = MakeProbeTickContext(
                1ull, rng, entityMap, walkable);
            std::vector<GameCommand> commands;
            CChampionAISystem::Execute(world, tc, commands);
            if (commands.empty() ||
                commands.front().kind != eCommandKind::Move ||
                WintersMath::DistanceSqXZ(
                    commands.front().groundPos,
                    Vec3{ 20.f, 0.f, 0.f }) > 0.001f)
            {
                return false;
            }
        }

        std::printf(
            "[SimLab][VictoryEconomyPing] PASS: minion pool 1/2/5 x4, turret 1500+4x1000, assist command\n");
        return true;
    }
```

`main`의 첫 special-mode 분기 앞에 추가:

```cpp
    if (argc > 1 && std::strcmp(argv[1], "--victory-economy-only") == 0)
    {
        std::printf("[SimLab] victory economy/ping probes only\n");
        RegisterAllChampionHooks();
        const bool_t bPass = RunVictoryEconomyAndPingProbe();
        std::printf("[SimLab] %s\n", bPass ? "PASS" : "FAIL");
        return bPass ? 0 : 1;
    }
```

정상 전체 probe 목록에서 `RunPracticeMinionAttackDamagePolicyProbe()` 바로 뒤에 다음 변수를 추가하고 최종 `bPass` 체인에도 포함:

```cpp
    const bool_t bVictoryEconomyAndPingProbePass =
        RunVictoryEconomyAndPingProbe();
```

```cpp
        bPracticeMinionAttackDamagePolicyProbePass &&
        bVictoryEconomyAndPingProbePass &&
```

### 2-20a. C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomBotMatchSoak.cpp

`PracticeControlProbeEvidence` 바로 아래에 추가:

```cpp
    struct TeamPingProbeEvidence
    {
        u32_t blueBotCount = 0u;
        u32_t replayCommandCount = 0u;
        u64_t checkpointBytes = 0u;
        bool_t bThreeHumanLanePreset = false;
        bool_t bAssistProducedNextTickMove = false;
        bool_t bDangerConsumedEquivalentMove = false;
        bool_t bExpired = false;
    };
```

`CGameRoomIntegrationProbeAccess` public 영역에 `RunPracticeControlProbe` 바로 아래 함수 전체를 추가:

```cpp
    static bool_t RunTeamPingCommandProbe(
        CGameRoom& room,
        u64_t seed,
        TeamPingProbeEvidence& outEvidence,
        std::string& outError)
    {
        outEvidence = {};
        outError.clear();

#if !defined(_DEBUG)
        (void)room;
        (void)seed;
        return true;
#else
        constexpr u32_t kSessionId = 7002u;
        if (!room.m_pLobbyAuthority)
        {
            outError = "team ping probe lobby authority is missing";
            return false;
        }

        CLobbyAuthority& laneAuthority = *room.m_pLobbyAuthority;
        laneAuthority.InitializeSlots();
        laneAuthority.m_hostSessionId = kSessionId;
        laneAuthority.m_phase = eRoomPhase::ChampionSelect;
        LobbySlotState* pLaneSlots = laneAuthority.GetSlots();
        u8_t redLaneBefore[5]{};
        for (u32_t i = 0u; i < kGameRosterSlotCount; ++i)
        {
            LobbySlotState& slot = pLaneSlots[i];
            slot.bHuman = i < 3u;
            slot.bBot = i >= 3u;
            slot.sessionId = i < 3u ? kSessionId + i : 0u;
            slot.champion = eChampion::ASHE;
            if (i >= 5u)
                redLaneBefore[i - 5u] = slot.botLane;
        }
        if (!laneAuthority.TryStartGame(kSessionId) ||
            pLaneSlots[3].botLane != kGameSimLaneBot ||
            pLaneSlots[4].botLane != kGameSimLaneTop)
        {
            outError = "3-human + 2-bot lane preset did not produce Bot/Top";
            return false;
        }
        for (u32_t i = 5u; i < kGameRosterSlotCount; ++i)
        {
            if (pLaneSlots[i].botLane != redLaneBefore[i - 5u])
            {
                outError = "3-human lane preset changed the enemy five-bot lanes";
                return false;
            }
        }
        outEvidence.bThreeHumanLanePreset = true;

        if (!PreparePracticeControlMatch(room, seed, kSessionId, outError))
            return false;

        LobbySlotState* pSlots = room.m_pLobbyAuthority->GetSlots();
        const EntityID issuer = room.m_entityMap.FromNet(pSlots[0].netId);
        std::vector<EntityID> blueBots;
        std::vector<EntityID> redBots;
        for (u32_t i = 1u; i < kGameRosterSlotCount; ++i)
        {
            const EntityID entity = room.m_entityMap.FromNet(pSlots[i].netId);
            if (entity == NULL_ENTITY || !room.m_world.IsAlive(entity))
            {
                outError = "team ping probe roster entity is invalid";
                return false;
            }
            (pSlots[i].team == 0u ? blueBots : redBots).push_back(entity);
        }
        if (issuer == NULL_ENTITY || blueBots.size() != 4u || redBots.size() != 5u)
        {
            outError = "team ping probe expected one blue human, four blue bots, five red bots";
            return false;
        }

        const Vec3 issuerPos =
            room.m_world.GetComponent<TransformComponent>(issuer).GetPosition();
        for (u32_t index = 0u; index < blueBots.size(); ++index)
        {
            const EntityID bot = blueBots[index];
            room.m_world.GetComponent<TransformComponent>(bot).SetPosition(
                issuerPos + Vec3{ static_cast<f32_t>(index + 1u), 0.f, 0.f });
            room.m_world.GetComponent<GoldComponent>(bot).amount = 0u;
        }
        for (u32_t index = 0u; index < redBots.size(); ++index)
        {
            room.m_world.GetComponent<TransformComponent>(redBots[index]).SetPosition(
                Vec3{ 300.f + static_cast<f32_t>(index), 0.f, 300.f });
        }

        const auto SubmitPing = [&](const GameCommandWire& wire) -> bool_t
        {
            const u64_t tickBefore = room.m_tickIndex;
            const u32_t replayBefore = room.m_pReplayRecorder->GetCommandCount();
            room.EnqueueCommand(kSessionId, wire, room.m_tickIndex, 0u, 0u);
            room.Tick();
            const auto ack = room.m_lastSimCommandSeqBySession.find(kSessionId);
            if (room.m_tickIndex != tickBefore + 1u ||
                ack == room.m_lastSimCommandSeqBySession.end() ||
                ack->second != wire.sequenceNum ||
                room.m_pReplayRecorder->GetCommandCount() != replayBefore + 1u ||
                !room.m_pendingExecCommands.empty() ||
                !room.m_pendingReplayCommands.empty())
            {
                outError = "team ping was not ACKed/journaled exactly once";
                return false;
            }
            return true;
        };

        GameCommandWire assist{};
        assist.kind = eCommandKind::TeamPing;
        assist.sequenceNum = 1u;
        assist.slot = static_cast<u8_t>(eTeamPingKind::Assist);
        assist.groundPos = issuerPos + Vec3{ 10.f, 0.f, 0.f };
        if (!SubmitPing(assist))
            return false;

        for (EntityID bot : blueBots)
        {
            const ChampionAIComponent& ai =
                room.m_world.GetComponent<ChampionAIComponent>(bot);
            if (!ai.bTeamPingObjectiveActive ||
                ai.teamPingKind != eTeamPingKind::Assist ||
                ai.teamPingExpireTick <= room.m_tickIndex)
            {
                outError = "Assist did not reach every same-team bot";
                return false;
            }
        }
        for (EntityID bot : redBots)
        {
            if (room.m_world.GetComponent<ChampionAIComponent>(bot)
                    .bTeamPingObjectiveActive)
            {
                outError = "Assist leaked to enemy bot";
                return false;
            }
        }

        std::vector<u8_t> checkpoint;
        if (!SimCheckpoint::SaveWorldKeyframe(
                room.m_world,
                room.m_rng,
                room.m_entityMap,
                room.m_tickIndex,
                checkpoint))
        {
            outError = "team ping checkpoint save failed";
            return false;
        }
        CWorld restoredWorld;
        DeterministicRng restoredRng(1ull);
        EntityIdMap restoredEntityMap;
        u64_t restoredTick = 0u;
        if (!SimCheckpoint::RestoreWorldKeyframe(
                restoredWorld,
                restoredRng,
                restoredEntityMap,
                restoredTick,
                checkpoint) ||
            restoredTick != room.m_tickIndex ||
            !restoredWorld.GetComponent<ChampionAIComponent>(blueBots.front())
                .bTeamPingObjectiveActive ||
            restoredWorld.GetComponent<ChampionAIComponent>(blueBots.front())
                .teamPingKind != eTeamPingKind::Assist)
        {
            outError = "team ping checkpoint restore lost Assist state";
            return false;
        }

        const u32_t assistSequenceBefore =
            room.m_world.GetComponent<ChampionAIComponent>(blueBots.front())
                .nextCommandSequence;
        room.Tick();
        const ChampionAIComponent& assistAI =
            room.m_world.GetComponent<ChampionAIComponent>(blueBots.front());
        outEvidence.bAssistProducedNextTickMove =
            assistAI.nextCommandSequence > assistSequenceBefore &&
            assistAI.debugLastCommandKind == static_cast<u8_t>(eCommandKind::Move);
        if (!outEvidence.bAssistProducedNextTickMove)
        {
            outError = "Assist did not produce a next-tick bot Move command";
            return false;
        }

        const EntityID dangerBot = blueBots.front();
        const Vec3 dangerAnchor =
            room.m_world.GetComponent<TransformComponent>(dangerBot).GetPosition();
        room.m_world.GetComponent<TransformComponent>(blueBots.back()).SetPosition(
            dangerAnchor + Vec3{ kTeamPingDangerRadius + 5.f, 0.f, 0.f });
        GameCommandWire danger{};
        danger.kind = eCommandKind::TeamPing;
        danger.sequenceNum = 2u;
        danger.slot = static_cast<u8_t>(eTeamPingKind::Danger);
        danger.groundPos = dangerAnchor;
        if (!SubmitPing(danger))
            return false;

        ChampionAIComponent& dangerState =
            room.m_world.GetComponent<ChampionAIComponent>(dangerBot);
        const Vec3 safeAnchor = dangerState.safeAnchor;
        const u64_t dangerExpireTick = dangerState.teamPingExpireTick;
        if (dangerState.teamPingKind != eTeamPingKind::Danger ||
            dangerExpireTick <= room.m_tickIndex)
        {
            outError = "Danger did not overwrite Assist with a fresh TTL";
            return false;
        }
        const ChampionAIComponent& ineligibleDangerAI =
            room.m_world.GetComponent<ChampionAIComponent>(blueBots.back());
        if (ineligibleDangerAI.bTeamPingObjectiveActive ||
            ineligibleDangerAI.teamPingKind != eTeamPingKind::None)
        {
            outError = "Danger ping-time radius eligibility was not fixed";
            return false;
        }

        MoveTargetComponent& equivalentMove =
            room.m_world.GetComponent<MoveTargetComponent>(dangerBot);
        equivalentMove.target = safeAnchor;
        equivalentMove.bHasTarget = true;
        dangerState.decisionTimer = 0.f;
        room.Tick();
        const MoveTargetComponent& dangerMove =
            room.m_world.GetComponent<MoveTargetComponent>(dangerBot);
        outEvidence.bDangerConsumedEquivalentMove =
            dangerMove.bHasTarget &&
            WintersMath::DistanceSqXZ(dangerMove.target, safeAnchor) <= 0.25f;
        if (!outEvidence.bDangerConsumedEquivalentMove)
        {
            outError = "Danger equivalent Move was overwritten by normal macro";
            return false;
        }

        room.m_world.GetComponent<TransformComponent>(dangerBot).SetPosition(
            dangerAnchor + Vec3{ kTeamPingDangerRadius + 5.f, 0.f, 0.f });
        room.m_world.GetComponent<ChampionAIComponent>(dangerBot).decisionTimer = 0.f;
        room.Tick();
        const ChampionAIComponent& outsideRadiusAI =
            room.m_world.GetComponent<ChampionAIComponent>(dangerBot);
        const MoveTargetComponent& outsideRadiusMove =
            room.m_world.GetComponent<MoveTargetComponent>(dangerBot);
        if (!outsideRadiusAI.bTeamPingObjectiveActive ||
            outsideRadiusAI.teamPingKind != eTeamPingKind::Danger ||
            !outsideRadiusMove.bHasTarget ||
            WintersMath::DistanceSqXZ(
                outsideRadiusMove.target, safeAnchor) > 0.25f)
        {
            outError = "eligible Danger bot stopped retreating after leaving radius";
            return false;
        }

        while (room.m_tickIndex <= dangerExpireTick)
            room.Tick();
        const ChampionAIComponent& expiredAI =
            room.m_world.GetComponent<ChampionAIComponent>(dangerBot);
        outEvidence.bExpired =
            !expiredAI.bTeamPingObjectiveActive &&
            expiredAI.teamPingKind == eTeamPingKind::None;
        if (!outEvidence.bExpired)
        {
            outError = "Danger TTL did not expire back to normal macro";
            return false;
        }

        outEvidence.blueBotCount = static_cast<u32_t>(blueBots.size());
        outEvidence.replayCommandCount =
            room.m_pReplayRecorder->GetCommandCount();
        outEvidence.checkpointBytes = static_cast<u64_t>(checkpoint.size());
        return true;
#endif
    }
```

`RunPracticeControlProbe` runtime reload revision 검증 바로 아래에 추가:

```cpp
        const GameplayDefinitionPack& reloadedPack =
            ServerData::GetActiveLoLGameplayDefinitionPack();
        if (!reloadedPack.economy ||
            std::fabs(reloadedPack.economy->turretGold - 1500.f) > 0.001f ||
            std::fabs(reloadedPack.economy->turretTeamGold - 1000.f) > 0.001f)
        {
            outError = "runtime reload did not overlay turret team gold";
            return false;
        }
```

`main` Debug control-probe room 블록 직후에 별도 room을 만들어 아래를 실행하고, 실패 시 즉시 1을 반환한다:

```cpp
    auto teamPingProbeRoom = CGameRoom::Create(
        options.roomId ^ 0x40000000u);
    TeamPingProbeEvidence teamPingEvidence{};
    if (!teamPingProbeRoom ||
        !CGameRoomIntegrationProbeAccess::RunTeamPingCommandProbe(
            *teamPingProbeRoom,
            options.seed,
            teamPingEvidence,
            error))
    {
        if (teamPingProbeRoom)
            teamPingProbeRoom->Stop();
        std::cerr << "RESULT status=FAIL reason=team_ping_probe_failed detail=\""
            << error << "\"\n";
        return 1;
    }
    std::cout << "TEAM_PING_PROBE status=PASS"
        << " blue_bots=" << teamPingEvidence.blueBotCount
        << " replay_commands=" << teamPingEvidence.replayCommandCount
        << " checkpoint_bytes=" << teamPingEvidence.checkpointBytes
        << " lane_preset=1 assist_next_tick_move=1"
        << " danger_equivalent_move=1 expired=1\n";
    teamPingProbeRoom->Stop();
    teamPingProbeRoom.reset();
```

### 2-20b. C:/Users/user/Desktop/Winters/Tools/Harness/ReplayCommandContractProbe.cpp

`explicitCommand` 설정의 기존 코드:

```cpp
    explicitCommand.kind = 13u;
    explicitCommand.slot = 2u;
    explicitCommand.itemId = 3157u;
    explicitCommand.practiceFlags = 5u;
    explicitCommand.clientTick = 91u;
    SetReplayCommandDomain(
        explicitCommand,
        eReplayCommandDomain::AuthoringMutation);
```

아래로 교체:

```cpp
    explicitCommand.kind = static_cast<u8_t>(eCommandKind::TeamPing);
    explicitCommand.slot = static_cast<u8_t>(eTeamPingKind::Assist);
    explicitCommand.itemId = 3157u;
    explicitCommand.groundPos[0] = 10.f;
    explicitCommand.groundPos[1] = 0.f;
    explicitCommand.groundPos[2] = -4.f;
    explicitCommand.practiceFlags = 5u;
    explicitCommand.clientTick = 91u;
    SetReplayCommandDomain(
        explicitCommand,
        eReplayCommandDomain::PlayerInput);
```

기존 persisted assertion:

```cpp
        GetReplayCommandDomain(persistedCommand) ==
            eReplayCommandDomain::AuthoringMutation &&
        persistedCommand.kind == 13u &&
        persistedCommand.slot == 2u &&
        persistedCommand.itemId == 3157u &&
        persistedCommand.practiceFlags == 5u;
```

아래로 교체:

```cpp
        GetReplayCommandDomain(persistedCommand) ==
            eReplayCommandDomain::PlayerInput &&
        persistedCommand.kind == static_cast<u8_t>(eCommandKind::TeamPing) &&
        persistedCommand.slot == static_cast<u8_t>(eTeamPingKind::Assist) &&
        persistedCommand.itemId == 3157u &&
        persistedCommand.groundPos[0] == 10.f &&
        persistedCommand.groundPos[1] == 0.f &&
        persistedCommand.groundPos[2] == -4.f &&
        persistedCommand.practiceFlags == 5u;
```

파일 상단에 다음 정의 헤더를 추가:

```cpp
#include "Shared/GameSim/Definitions/TeamPingDef.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

### 2-21. 코드젠 산출물

다음 생성 명령으로 `Command_generated.h`, Go command schema, `Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp`를 갱신한다. 산출물은 수동 편집하지 않는다.

```powershell
cmd /c Shared\Schemas\run_codegen.bat
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
```

## 3. 검증

예측:

- 일반 웨이브 XP는 1명 276.45, 2명 각 180.42(총 360.84), 5명 각 72.168(총 360.84)가 되고, 먼 아군·죽은 아군·근처 적은 0 XP, 미니언 골드는 막타 챔피언만 받는다.
- 포탑을 챔피언이 파괴하면 그 챔피언 +1,500G, 같은 팀 다른 네 챔피언 각 +1,000G, 상대 팀 +0G다. Red가 파괴해도 같은 규칙이 대칭 적용된다.
- 3 human + 2 bot인 팀에서 두 봇이 모두 Bot으로 남아 있을 때만 Bot/Top으로 정규화된다. 적 5-bot 라인 배치는 불변이다. 적 미드 외곽 포탑이 파괴되고 적이 미드에 모여도 side bot은 자동 전원 미드 집결하지 않고, 아군 방어 위협 집결은 유지된다.
- Assist 핑은 자유 상태 아군 봇이 6초 동안 목표로 이동하게 하고, Danger는 반경 12m 자유 상태 봇을 3초간 safeAnchor로 이동시킨다. combo/dive/저체력 안전 처리는 핑보다 우선한다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다. TeamPing handler는 checkpoint 대상 AI 계획 상태만 설정하고 실제 이동은 다음 AI GameCommand가 수행한다.
- F4 Total/Bonus AD ranked 및 runtime scalar 편집은 10까지 가능하고 AP/HP Ratio는 5에서 clamp된다. F4 실제 화면에서 드래그와 직접 입력 10.00, dirty 표시, 단일 Save & Hot Load 버튼을 확인한다.
- 애쉬 R 정의 팩의 `StunDurationSec`는 2.0이며 기존 DamagePipeline/상태이상 경로가 이를 소비한다.
- 이 변경은 XP·보상·AI command hash를 의도적으로 바꾸므로 과거 SimLab 최종 골든 해시 불변을 기대하지 않는다. 같은 seed 두 실행끼리의 결정론 일치만 기대한다.

검증 명령:

```powershell
cmd /c Shared\Schemas\run_codegen.bat
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
python Tools/LoLData/Test-F4BalanceContracts.py --root .
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
Tools/Bin/Debug/SimLab.exe --victory-economy-only
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File Tools/Harness/RunReplayCommandContractProbe.ps1
powershell -ExecutionPolicy Bypass -File Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 120 -HeartbeatTicks 120 -Configuration Debug
msbuild Server/Include/Server.vcxproj /p:Configuration=Release /p:Platform=x64 /m
msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
git diff --check
```

실전/화면 QA:

```text
1. Release Server+백엔드+Debug 클라이언트 3개로 3 human + 2 bot 대 5 bot을 시작하고, 양 팀 시작 Gold/Level을 기록해 4,000G 사용자 설정이 불변임을 확인한다.
2. 로비에서 기본 아군 봇이 Bot/Top으로 갈리는지 확인한다.
3. 적 mid outer 파괴 뒤 적이 mid에 모일 때 두 봇이 자동으로 전부 mid를 향하지 않고 Top/Bot 파밍을 유지하는지 본다.
4. mid에 Assist 핑을 찍어 필요할 때만 아군 봇이 집결하고, Danger 핑 반경 안 봇이 safeAnchor 쪽으로 빠지는지 본다.
5. 포탑 파괴 전후 5명 골드를 기록해 +1500/+1000x4를 확인한다.
6. 1920×1080/100% DPI에서 F4 Skills의 Total/Bonus AD Ratio 10.00, 저장/hot-load 성공, 저장 실패, skill 비선택 상태를 위에 정의한 3개 artifact 경로로 캡처한다.
7. 애쉬 R 적중 후 스턴 해제까지 서버 tick 기준 60tick(2초)인지 관찰한다.
```

미검증:

- 원격 클라이언트에 다른 플레이어의 핑 마커/사운드를 복제하는 presentation event.
- 포탑 총 5,500G가 장기적으로 재미있는 공정 밸런스인지 여부. 이번 목표는 의도적 승리 가속이며 수치 평가는 후속 실전 ledger 대상이다.
- 전체 17챔피언의 계수/TTK 매트릭스. AD 상한만 확장하며 실제 계수는 변경하지 않는다.

확인 필요:

- 제품 공식 최소 해상도/DPI가 문서화되지 않아 1920×1080/100%를 이번 기준으로 사용한다. 후속에 공식 기준을 확정해야 한다.

## 실행 예산과 외부 마감

- 바닥 70%: XP/포탑/애쉬/F4/기본 라인/핑 command 수직 슬라이스, 코드젠, headless probe, Server·Client 빌드.
- 천장 30%: 실제 3-client 경기와 F4 화면 QA, 골드/스턴 tick 기록. 새 AI utility 체계나 전체 밸런스 딥다이브는 열지 않는다.
- 외부 마감: 2026-07-21까지 치트/연습 기능 없이 정상 5v5 첫 승 1회와 포탑 보상 ledger를 남긴다.

## 프롬프트 냉정 비평

- 좋은 점: 승리라는 최종 목표, 반복 재현 장면(타워 진입 시 적이 몰려와 사망), 핵심 경제 후보(XP 복제), 원하는 수치(1500/1000, Ashe 2초)가 명확하다.
- 부족한 점: 첫 설명의 “적 봇 3명”과 문제 정의의 “적 봇 5명”이 충돌한다. 이 세션은 최종 승리 목표와 5v5 흐름을 우선해 3 human + 2 allied bot 대 5 enemy bot으로 고정한다.
- 부족한 점: “적은 포탑을 못 민다”는 가정은 테스트 전 사실이 아니며 이를 코드에 넣으면 아군 전용 치트가 된다. 그래서 보상은 팀 대칭으로 구현한다.
- 부족한 점: “AD Ratio 10”은 실제 계수를 10으로 만들라는 뜻과 편집 상한을 10으로 열라는 뜻이 다르다. 이번 계획은 안전하게 저작 상한만 연다.
- 부족한 점: 포탑 1기 총 5,500G는 성장 보상이라기보다 승패를 급격히 고정하는 규칙이다. 이것으로 이기면 “실력 차이를 정상 성장으로 전환했다”는 증거가 아니라 “강한 오브젝트 가속 규칙이 작동했다”는 증거다.
- 부족한 점: 맵 핑의 종류별 의미·TTL·우선순위가 없었다. 이번 계약은 Assist=6초 집결, Danger=3초/12m 이탈, hard safety와 active commitment 우선으로 sharp하게 고정한다.
- 핵심 본질: 지금 문제는 적 봇 전투력이 아니라 **우위를 벌리는 경제 보존식과 우위를 구조물로 바꾸는 팀 의사결정 계약이 없거나 잘못되어 있다는 것**이다. XP 복제는 격차 생성을 막고, 자동 과잉 합류는 사이드 기회비용을 지우며, 포탑 무보상은 우위의 현금화를 막는다.

## 서브 에이전트 비평

```text
1차 독립 read-only 비평: P0 3개, P1 8개.

- ACCEPT P0 Economy schema 누락: required/properties에 turretTeamGold를 추가했다.
- ACCEPT P0 runtime overlay 누락: known-root + TryOverlayNumber + 실제 reload probe를 추가했다.
- ACCEPT P0 ChampionAI ABI 오계산: 3016B와 2992/3000/3012 offset assert로 고정했다.
- ACCEPT P1 issuer 생존 검증: Champion/Transform/Health 필수, bIsDead/current HP gate를 추가했다.
- ACCEPT P1 검증 경로 부족: GameRoom wire/session/team/checkpoint/next-tick/Danger/TTL probe, replay payload probe, 실행 명령을 추가했다.
- ACCEPT P1 enum 소유권: Systems가 아닌 새 `Definitions/TeamPingDef.h`로 분리했다.
- ACCEPT P1 equivalent Move TTL 덮어쓰기: Assist/Danger 모두 emit 성공 여부와 관계없이 해당 tick을 소비하고 Danger 2-tick probe로 고정했다.
- ACCEPT P1 global slot 4 변경: 전역 기본값 변경을 폐기하고 3 human + 2 bot인 팀의 duplicate-Bot 상태만 정규화하며 적 5-bot 불변 probe를 추가했다.
- ACCEPT P1 macro 원인 미구분: 적 defensive group은 유지하고, 아군 offensive auto-group만 제거하는 양 팀 fixture로 문제를 재정의했다.
- ACCEPT P1 F4 화면 QA: 실행 장면, 1920x1080/100% 임시 기준, success/failure/empty artifact를 명시했다. 공식 최소 사양 미문서는 후속 확인 사항으로 유지한다.
- ACCEPT P1 시작 자원 구성 차이: Debug 10,000G/6, Release 4,000G를 인지하고 실전 Configuration/양 팀 시작 ledger를 검증에 추가했다.

2차 재비평: P0 0개, P1 2개.

- ACCEPT P1 Danger 반경/TTL 의미 불일치: 서버 handler가 핑 시점 12m eligibility를 고정하고 비선정 봇의 기존 objective를 clear한다. 선정 봇은 AI에서 반경을 재검사하지 않고 TTL 동안 후퇴한다.
- ACCEPT P1 equivalent Move probe 미실행: `MoveTargetComponent` target을 safeAnchor로 미리 고정하고 decisionTimer=0으로 actual false-emit 분기를 즉시 실행한다. 이후 반경 밖으로 Transform을 옮겨도 TTL/objective/MoveTarget이 유지되는지 검증한다.

3차 재비평: P0 0개, P1 1개.

- ACCEPT P1 TTL이 decision cadence에 막힘: `CChampionAISystem::Execute` timer 갱신 직후, `decisionTimer > 0` 조기 반환 전에 expired team-ping objective를 clear한다. 따라서 90/180 tick 만료가 7-tick 판단 cadence와 무관하게 정확한 tick에 닫힌다.

재비평 게이트: P0/P1 residual 0을 확인하기 전에는 소스 구현을 시작하지 않는다.
최종 독립 read-only 재비평: P0 0개 / P1 0개. 구현 게이트 통과.
```
