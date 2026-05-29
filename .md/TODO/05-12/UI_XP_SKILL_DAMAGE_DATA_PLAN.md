# UI / XP / Skill Rank / Damage Data Migration Plan

- 작성일: 2026-05-12
- 기준 워크스페이스: `C:\Users\user\Desktop\Winters`
- UI 기준 자산 루트: `Client/Bin/Resource/Texture/UI`
- 서버 권위 기준 문서: `.md/TODO/05-09/ServerAICompletion.md`
- 연관 런타임 체크포인트: S10 BotAIStage1 smoke pass

---

## 0. 현재 순서

```text
S10_KEEP    Death / TargetInvalid / Respawn 기준선 보존
U1          UI asset inventory + LoL HUD shell
X1          서버 권위 Experience / Level / SkillPoint
K1          Skill Level-Up command 정식화
U2          HUD data binding: HP / MP / XP / rank / cooldown / level-up
D1          Damage formula JSON migration
L1          Lua formula authoring layer
```

이번 목표는 "예쁘게 보이는 UI"만 먼저 붙이는 것이 아니라, 서버 GameSim 값이 Snapshot/Event를 타고 내려와 HUD에 보이는 구조까지 닫는 것이다.

---

## 1. 목표

### 1.1 UI 완성

- `Client/Bin/Resource/Texture/UI`를 LoL HUD의 기준 에셋 루트로 사용한다.
- 우선 사용 자산:
  - `HUD_Irelia.png`
  - `HUD_Irelia_2.png`
  - `Champion_HP_Bar.png`
  - `Champion_MP_Bar.png`
  - `single_bar_blue.png`
  - `single_bar_red.png`
  - `Minimap.png`
  - `HUD/clarity_hudatlas.png`
  - `HUD/clarity_abilityatlas.png`
  - `HUD/clarity_levelupatlas.png`
  - `HUD/clarity_resourceatlas.png`
  - `HUD/statspanel_atlas.png`
- 현 `CUI_Manager`의 정적 Irelia HUD 렌더를 실제 HUD 레이어로 확장한다.
- 화면 하단 HUD에는 HP/MP/XP, 챔피언 레벨, 스킬 랭크, 쿨다운, 레벨업 버튼, 미니맵 프레임을 표시한다.
- 월드 오버레이에는 팀별 체력바와 데미지 플로터를 표시한다.

### 1.2 경험치와 레벨

- XP/Level/SkillPoint는 서버 GameSim이 소유한다.
- 미니언/챔피언/정글몹 사망 시 서버가 XP를 부여한다.
- Level 증가 시 `SkillRankComponent::pointsAvailable`을 증가 또는 재동기화한다.
- Snapshot에 XP/다음 레벨 XP/스킬 포인트를 포함한다.

### 1.3 스킬 레벨 업그레이드

- `CommandKind::LevelSkill`은 이미 FlatBuffers에 있으므로 기존 커맨드를 살린다.
- 클라이언트는 Ctrl+Q/W/E/R 또는 HUD level-up 버튼으로 `LevelSkill`을 보낸다.
- 서버는 `CSkillRankSystem::TryLevelSkill`을 통해 슬롯별 최대 랭크, 포인트, R 레벨 제한을 검증한다.
- 서버 승인 결과는 Snapshot의 `skillRanks`와 `skillPoints`로 반영한다.

### 1.4 데미지 계산 공식 JSON/Lua 이전

- 1차는 JSON 데이터 + C++ deterministic evaluator로 이전한다.
- 2차에서 Lua authoring layer를 얹는다.
- 이유: 현재 소스 런타임에는 Lua 통합이 없고, JSON 사용 패턴은 Client의 `GameModeCatalog`에 이미 있다.
- 데미지 적용의 최종 권위는 계속 서버 `DamagePipeline` / `DamageQueueSystem`에 둔다.

---

## 2. 왜 이 순서인가

1. S10 smoke 기준선을 먼저 지킨다.
   - `.md/TODO/05-09/ServerAICompletion.md`의 다음 1순위는 Death / TargetInvalid / Respawn이다.
   - 죽은 entity가 target이 되는 문제를 막지 않으면 XP와 데미지 공식 검증도 흔들린다.

2. UI 에셋 로딩은 먼저 해도 안전하지만, UI 값은 서버 값으로 묶어야 한다.
   - 현재 `CUI_Manager`는 정적 HUD 이미지를 그릴 수 있다.
   - HP/MP/Level/Cooldown/Rank는 Snapshot에 일부 이미 있으나 XP/SkillPoint는 없다.

3. 스킬 레벨업은 커맨드가 이미 있으므로 가장 짧은 서버 권위 루프다.
   - `Command.fbs`에 `LevelSkill = 4`가 있다.
   - 서버 `HandleLevelSkill`만 현재 단순 증가라 정식 검증으로 교체하면 된다.

4. 데미지 공식은 JSON 먼저, Lua는 나중이다.
   - JSON은 데이터 테이블 이전에 적합하다.
   - Lua는 sandbox, determinism, hot reload, 빌드 편입, 서버/클라 동일성 문제가 있으므로 D1 이후 L1로 둔다.

---

## 3. 현재 코드베이스 파악

### 3.1 UI

- `Engine/Public/Manager/UI/UI_Manager.h`
  - `CUI_Manager`는 Phase A HP bar, Phase B+ damage popup/champion HUD/skill cooldown/scoreboard를 이미 목표로 적고 있다.
  - 현재 public API: `Initialize`, `Render_Overlay`, `Render_Cursor`, `OnImGui_Tuner`, cursor/show flag 제어.
  - 현재 private hook: `Draw_HealthBars`, `Draw_ChampionHUD`; `Draw_DamageFloaters`, `Draw_PlayerHUD`, `Draw_Scoreboard`는 주석 상태.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - path 상수는 `single_bar_blue/red`, cursor, `HUD_Irelia.png`까지만 있다.
  - `Initialize`에서 HP bar/cursor/static HUD를 로드한다.
  - `Render_Overlay`는 HealthBars와 ChampionHUD만 호출한다.
  - `Draw_ChampionHUD`는 화면 하단에 정적 `HUD_Irelia.png`만 그린다.
  - `OnImGui_Tuner`에는 Phase B+ placeholder가 남아 있다.
- `Client/Private/Scene/InGameRenderBridge.cpp`
  - `CGameInstance::Get()->UI_Render_Overlay(vp);`에서 인게임 UI 오버레이가 호출된다.
- `Client/Private/Scene/InGameDebugBridge.cpp`
  - `CGameInstance::Get()->UI_OnImGui_Tuner();`에서 UI 튜너가 호출된다.

### 3.2 Snapshot / Event / Command

- `Shared/Schemas/Snapshot.fbs`
  - `EntitySnapshot`에는 `level`, `hp`, `mana`, `maxHp`, `maxMana`, `skillCooldowns`, `skillRanks`가 있다.
  - XP, 다음 레벨 XP, 남은 스킬 포인트는 아직 없다.
- `Shared/Schemas/Event.fbs`
  - enum에는 `SkillRankUp = 10`, `LevelUp = 13`이 있으나 전용 payload table은 아직 없다.
- `Shared/Schemas/Command.fbs`
  - `CommandKind::LevelSkill = 4`가 이미 있다.
  - `CommandPacket.slot`으로 스킬 슬롯 전달 가능.

### 3.3 Skill Rank

- `Shared/GameSim/Components/SkillRankComponent.h`
  - `ranks[5]`, `pointsAvailable` 보유.
- `Shared/GameSim/Systems/SkillRankSystem.cpp`
  - `GetMaxRankForSlot`, `SyncPointsForLevel`, `TryLevelSkill`이 이미 있다.
- `Shared/GameSim/Systems/CommandExecutor.cpp`
  - `HandleLevelSkill`은 현재 `rank.ranks[cmd.slot] < 5`만 보고 증가한다.
  - 포인트, 슬롯별 최대 랭크, R 제한, BA 슬롯 금지 검증을 통과하지 않는다.
- `Client/Public/Network/Client/CommandSerializer.h`
  - `SendMove`, `SendCastSkill`, `SendBasicAttack`은 있으나 `SendLevelSkill`은 없다.

### 3.4 Damage

- `Shared/GameSim/Components/StatComponent.h`
  - AD/AP/Armor/MR/AttackSpeed/Crit/CDR 등 데미지 공식의 입력값이 이미 있다.
- `Shared/GameSim/Systems/DamagePipeline.cpp`
  - `BuildRawDamage`가 `CSkillScalingRegistry`와 rank를 사용한다.
  - `ApplyResistance`는 `100 / (100 + resistance)` 계열 공식이다.
  - `ApplyDamageRequest`가 HP 차감과 dead flag 반영을 수행한다.
- `Shared/GameSim/Systems/DamageQueueSystem.cpp`
  - damage request를 처리하고 `Damage` replicated event를 만든다.
  - kill result / XP grant hook를 넣기 좋은 위치다.
- `Shared/GameSim/Definitions/SkillScalingTable.h`
  - flatDamage, cooldown, manaCost, adRatio, apRatio 등 데이터화 가능한 필드가 이미 있다.

### 3.5 Experience / Death / Respawn

- `Shared/GameSim/Components/ExperienceComponent.h`는 아직 없다.
- `Shared/GameSim/Components/RespawnComponent.h`는 존재한다.
- `Shared/GameSim/Systems/DeathSystem.cpp`
  - HealthComponent 기준으로 dead flag, move target, skill stage를 정리한다.
  - XP 지급과 target invalid 공통화는 아직 별도 작업이 필요하다.
- `Server/Private/Game/GameRoom.cpp`
  - simulation phase에 `CDamageQueueSystem::Execute`, `CDeathSystem::Execute`, `Phase_ServerDeathAndRespawn`이 이미 포함되어 있다.

### 3.6 JSON / Lua

- 소스 트리에서 gameplay `.lua` / `.json` 데이터는 아직 없다.
- `Client/Public/Network/Backend/json.hpp`가 있으며, `Client/Private/GameMode/GameModeCatalog.cpp`가 JSON 로더 패턴을 갖고 있다.
- Shared/Server가 Client Backend header에 직접 의존하면 경계가 깨지므로 JSON 라이브러리는 Shared 또는 ThirdParty 표준 위치로 승격해야 한다.
- Lua 런타임은 아직 편입되지 않았으므로 JSON-first가 안전하다.

---

## 4. 파일 터치 계획

### 4.1 UI

- `Engine/Public/Manager/UI/UI_Manager.h`
  - HUD 상태 캐시 구조 추가.
  - ability slot, XP bar, level-up button, damage floater draw 함수 추가.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - `Client/Bin/Resource/Texture/UI` 기반 신규 SRV 로드.
  - `Draw_ChampionHUD`를 정적 이미지 출력에서 데이터 기반 HUD 출력으로 교체.
  - atlas UV는 1차 수동 rect table 또는 JSON layout로 분리.
- `Client/Bin/Resource/Texture/UI`
  - 자산은 원본 기준으로 읽기만 한다.
  - 필요 시 후속으로 `Client/Bin/Data/UI/lol_hud_layout.json` 생성.
- `Client/Private/Scene/InGameRenderBridge.cpp`
  - 현재 `UI_Render_Overlay` 호출 유지. Scene 직접 HUD 로직 추가는 금지.
- `Client/Private/Scene/InGameDebugBridge.cpp`
  - UI 튜너에 atlas rect, HUD scale, cooldown alpha, XP bar scale 노출.

### 4.2 Experience / Skill Rank

- `Shared/GameSim/Components/ExperienceComponent.h`
  - `xp`, `xpForNext`, `level`, `skillPointsEarned` 또는 `pendingSkillPoints` 추가.
- `Shared/GameSim/Definitions/ExperienceCurveDef.h`
  - 레벨별 필요 XP와 kill XP reward 정의.
- `Shared/GameSim/Systems/ExperienceSystem.h`
- `Shared/GameSim/Systems/ExperienceSystem.cpp`
  - XP grant, level up, skill point sync 담당.
- `Shared/GameSim/Systems/SkillRankSystem.cpp`
  - R 레벨 제한 추가: 6/11/16 또는 현재 목표 레벨 규칙.
- `Shared/GameSim/Systems/CommandExecutor.cpp`
  - `HandleLevelSkill`을 `CSkillRankSystem::TryLevelSkill` 기반으로 교체.
- `Client/Public/Network/Client/CommandSerializer.h`
- `Client/Private/Network/Client/CommandSerializer.cpp`
  - `SendLevelSkill(u16_t entityNetId, u8_t slot)` 추가.
- `Client/Private/Scene/InGameCombatInputBridge.cpp` 또는 신규 UI input bridge
  - Ctrl+Q/W/E/R, HUD level-up button 입력을 command로 연결.

### 4.3 Snapshot / Event

- `Shared/Schemas/Snapshot.fbs`
  - `EntitySnapshot`에 `xp:uint`, `xpForNext:uint`, `skillPoints:ubyte` 추가.
- `Shared/Schemas/Event.fbs`
  - `LevelUpEvent`, `SkillRankUpEvent` payload 추가 여부 결정.
  - 1차 UI는 Snapshot만으로도 가능하나, 플로터/사운드/애니메이션은 Event가 더 적합하다.
- `Shared/Schemas/run_codegen.bat`
  - schema 변경 후 C++/Go 코드 생성.
- `Server/Private/Game/SnapshotBuilder.cpp`
  - ExperienceComponent와 SkillRankComponent points를 snapshot에 포함.
- `Client/Private/Network/Client/SnapshotApplier.cpp`
  - 클라이언트 HUD용 component/cache에 XP, next XP, skillPoints 반영.

### 4.4 Damage Formula Data

- `Shared/GameSim/Definitions/DamageFormulaDef.h`
  - damage type, base table, AD/AP ratio, bonus ratio, resistance policy, flags 정의.
- `Shared/GameSim/Registries/DamageFormulaRegistry.h`
- `Shared/GameSim/Registries/DamageFormulaRegistry.cpp`
  - formula id / skill id 기반 lookup.
- `Shared/GameSim/Systems/DamagePipeline.cpp`
  - 기존 `BuildRawDamage` 계산을 registry 기반 evaluator로 교체.
  - 기존 `ApplyResistance`는 서버 deterministic 함수로 유지.
- `Shared/Data/GameSim/damage_formulas.json`
- `Shared/Data/GameSim/skill_scaling.json`
- `Shared/Data/GameSim/champion_stats.json`
- `Shared/Data/GameSim/experience_curve.json`
  - source-of-truth 데이터 파일.
- `Server/Bin/Data/GameSim`
- `Client/Bin/Data/GameSim`
  - post-build copy 또는 runtime content resolver 대상.

### 4.5 Lua

- 즉시 편입하지 않는다.
- L1에서만 검토:
  - `Engine/ThirdPartyLib/Lua54`
  - `Shared/GameSim/Scripting/DamageFormulaLua.h/.cpp`
  - sandboxed pure function: no IO, no random, no clock.
- 서버와 클라가 같은 결과를 내야 하므로 Lua는 authoring/hot reload 계층으로만 시작하고, 최종 전투 판정은 서버 C++ evaluator가 책임진다.

---

## 5. 삽입 / 교체 포인트

### 5.1 UI_Manager

- `Engine/Public/Manager/UI/UI_Manager.h`
  - anchor: `void Draw_HealthBars(ImDrawList* draw, const ImVec2& viewportSize);`
  - 이 주변의 Phase B+ 주석 hook를 실제 함수 선언으로 교체한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - anchor: `static constexpr const wchar_t* kChampionHUD = L"Client/Bin/Resource/Texture/UI/HUD_Irelia.png";`
  - 이 path 상수 블록에 HP/MP/HUD atlas/ability atlas/resource atlas를 추가한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - anchor: `if (!Load_TextureSRV(kChampionHUD, m_pSRV_ChampionHUD.ReleaseAndGetAddressOf(), &m_fChampionHUD_W, &m_fChampionHUD_H))`
  - 이 로딩 블록 뒤에 atlas SRV 로딩과 실패 fallback 로그를 추가한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - anchor: `Draw_HealthBars(draw, viewportSize);`
  - `Render_Overlay`에서 health bars 이후 `Draw_PlayerHUD`, `Draw_AbilityBar`, `Draw_DamageFloaters` 호출을 추가한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - anchor: `void CUI_Manager::Draw_ChampionHUD(ImDrawList* draw, const ImVec2& viewportSize)`
  - 정적 HUD 이미지 출력만 하는 현 구현을 데이터 기반 bottom HUD로 교체한다.

### 5.2 LevelSkill

- `Client/Public/Network/Client/CommandSerializer.h`
  - anchor: `bool SendBasicAttack(uint16_t attackerNetId, uint16_t targetNetId, uint32_t commandSeq);`
  - 바로 아래 `SendLevelSkill` 선언 추가.
- `Client/Private/Network/Client/CommandSerializer.cpp`
  - anchor: `bool CCommandSerializer::SendSingle(const GameCommand& cmd)`
  - 이 함수 앞에 `SendLevelSkill` 구현 추가.
- `Shared/GameSim/Systems/CommandExecutor.cpp`
  - anchor: `void CDefaultCommandExecutor::HandleLevelSkill(CWorld& world, const GameCommand& cmd)`
  - 현재 단순 rank 증가 로직을 `CSkillRankSystem::TryLevelSkill` 호출로 교체한다.

### 5.3 Snapshot

- `Shared/Schemas/Snapshot.fbs`
  - anchor: `level:ubyte = 1;`
  - XP 관련 필드는 level 근처에 추가한다.
- `Server/Private/Game/SnapshotBuilder.cpp`
  - anchor: `if (const auto* skills = world.GetComponent<SkillRankComponent>(e))`
  - skill ranks 직후 pointsAvailable과 ExperienceComponent를 수집한다.
- `Client/Private/Network/Client/SnapshotApplier.cpp`
  - anchor: `champ.level = src->level();`
  - champion/HUD cache에 XP와 skillPoints를 같이 반영한다.

### 5.4 Damage / XP

- `Shared/GameSim/Systems/DamagePipeline.cpp`
  - anchor: `DamageBreakdown CDamagePipeline::BuildRawDamage(CWorld& world, const DamageRequest& req)`
  - flat/ration 계산부를 formula registry lookup으로 교체한다.
- `Shared/GameSim/Systems/DamageQueueSystem.cpp`
  - anchor: `const auto result = CDamagePipeline::ApplyDamageRequest(world, req);`
  - result가 kill을 만들었는지 판단하고 ExperienceSystem grant hook를 호출한다.
- `Server/Private/Game/GameRoom.cpp`
  - anchor: `CDamageQueueSystem::Execute(world, evtCtx);`
  - damage/death/respawn 순서를 유지하면서 XP grant flush 위치를 확정한다.

---

## 6. 검증 로그

### 6.1 Build

```powershell
.\Shared\Schemas\run_codegen.bat
MSBuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
MSBuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

### 6.2 Runtime Smoke

- UI
  - `[UI_Manager] HUD atlas loaded path=Client/Bin/Resource/Texture/UI/HUD/clarity_hudatlas.png`
  - `[UI_Manager] Ability atlas loaded path=Client/Bin/Resource/Texture/UI/HUD/clarity_abilityatlas.png`
  - HP/MP/XP bar가 화면 하단 HUD에 표시된다.
  - `single_bar_blue/red` 팀별 HP bar가 월드 오버레이에 표시된다.
- XP / Level
  - `[Experience] grant killerNetId=... victimNetId=... xp=...`
  - `[Experience] level-up netId=... old=... new=... skillPoints=...`
  - Snapshot에서 xp/xpForNext/skillPoints가 클라이언트에 도달한다.
- Skill Rank
  - `[Command] LevelSkill recv entity=... slot=...`
  - `[SkillRank] accept entity=... slot=... rank=... points=...`
  - reject 케이스: no point, invalid slot, max rank, R level gate.
- Damage Formula
  - `[DamageFormula] loaded formulas=... skills=... stats=... source=json`
  - `[Damage] source=... target=... raw=... final=... type=... formula=...`

### 6.3 Manual Checklist

- 미니언 last hit 후 XP bar가 증가한다.
- 레벨업 직후 level-up 버튼 또는 Ctrl+Q/W/E/R이 활성화된다.
- 스킬을 올리면 rank 숫자와 skillPoints가 즉시 Snapshot 기준으로 바뀐다.
- rank가 오른 스킬의 damage/cooldown/manaCost가 JSON table 기준으로 바뀐다.
- 죽은 entity가 bot/minion/turret/projectile target이 되지 않는다.
- projectile hit 직전 stale/dead target은 무효화된다.

---

## 7. Next Slice

### Slice A: UI asset shell

- `CUI_Manager`에 HUD/ability/resource atlas SRV 로딩 추가.
- `Draw_ChampionHUD`를 하단 LoL HUD shell로 교체.
- 아직 서버 데이터가 없는 XP/skillPoints는 placeholder cache로 표시하되, code path 이름은 실제 데이터 기준으로 만든다.

### Slice B: LevelSkill server authority

- `SendLevelSkill` 추가.
- Ctrl+Q/W/E/R 입력 연결.
- `HandleLevelSkill`을 `CSkillRankSystem::TryLevelSkill`로 교체.
- Snapshot의 기존 `skillRanks`로 HUD rank 표시.

### Slice C: ExperienceComponent + Snapshot 확장

- `ExperienceComponent`와 `ExperienceSystem` 추가.
- `Snapshot.fbs`에 XP/nextXP/skillPoints 추가 후 codegen.
- 서버 SnapshotBuilder와 클라 SnapshotApplier 연결.

### Slice D: JSON damage data

- `Shared/Data/GameSim/*.json` 도입.
- champion stats / skill scaling / damage formula registry 로딩.
- `DamagePipeline::BuildRawDamage`를 registry evaluator로 이전.

### Slice E: Lua authoring layer

- Lua는 JSON evaluator 안정화 이후 편입한다.
- deterministic 검증 harness부터 만든다.
- 서버 권위 전투 판정에는 직접 Lua 결과를 신뢰하지 않는다.

---

## 8. 이번 구현의 금지선

- Scene에 HUD 계산 로직을 직접 쌓지 않는다.
- Client가 XP/Level/SkillRank를 임의 확정하지 않는다.
- `LevelSkill`을 client-only UI 상태 변경으로 처리하지 않는다.
- Shared/Server에서 `Client/Public/Network/Backend/json.hpp`를 직접 include하지 않는다.
- Lua를 JSON 이전 단계에서 먼저 넣지 않는다.
- S10 BotAIStage1 smoke 기준선, 특히 Death / TargetInvalid / Respawn 작업을 우회하지 않는다.
