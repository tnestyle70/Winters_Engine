Session - LoL의 모든 gameplay, visual, policy, test data를 원자 단위로 추출하고 generated compatibility layer로 회귀 없이 협업 구조로 전환한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Data/LoL

LoL Data의 최상위 본질은 "런타임 코드가 아니라 협업자가 소유하는 게임/시각/운영 사실"이다.

아래 구조로 정리한다:

```text
Data/LoL/
  ServerPrivate/
    Champion/
      ChampionIdentity.json
      ChampionStats.json
      ChampionSkillSlot.json
      SkillTarget.json
      SkillCost.json
      SkillCooldown.json
      SkillRange.json
      SkillStageLock.json
      SkillFacing.json
      SkillEffectPolicy.json
      SkillScaling.json
      ChampionPassiveDash.json
    SummonerSpell/
      SummonerSpellGameData.json
    Object/
      MinionGameData.json
      MinionWaveData.json
      StructureGameData.json
      JungleGameData.json
    Match/
      GameModeData.json
      SpawnPolicyData.json
      LoadoutPolicyData.json
      BotSkillRankPolicyData.json
    Map/
      SummonersRiftStageBinding.json
  ClientPublic/
    GameHint/
      ChampionTooltipHint.json
      SkillRangeHint.json
      SkillCooldownHint.json
      PublicRulesetManifest.json
    Visual/
      Champion/
        ChampionModelVisualData.json
        ChampionPoseVisualData.json
        ChampionActionVisualData.json
        SkillVisualData.json
      Object/
        ProjectileVisualData.json
        MinionVisualData.json
        StructureVisualData.json
        JungleVisualData.json
    FX/
      Champions/
      Object/
  SharedContract/
    DataPackManifest.json
    ActionIdRegistry.json
    CueIdRegistry.json
    SchemaVersion.json
  Test/
    SmokeRosterData.json
    DebugScenarioData.json
```

플레이어가 받는 client package에 들어가도 되는 것은 `ClientPublic`과 `SharedContract`뿐이다.
`ServerPrivate`는 server cook/build artifact로만 배포한다.

이 기준을 적용하면 기존의 더 단순한 `Game/Visual/Test` 구조는 아래처럼 접힌다:

```text
Data/LoL/
  ServerPrivate/Game/
  ClientPublic/GameHint/
  ClientPublic/Visual/
  ClientPublic/FX/
  SharedContract/
  Test/
```

기존 문서에서 `Data/LoL/Game/**`라고 쓴 항목은 구현 시 기본적으로 `Data/LoL/ServerPrivate/Game/**`로 이동한다.
단, UI/툴팁/약한 예측에 필요한 공개 정보는 `Data/LoL/ClientPublic/GameHint/**`에 서버 데이터에서 생성한 projection으로 둔다.

ClientPublic projection 원칙:

```text
클라이언트에 있어도 되는 것
- 이름, 설명, 아이콘, 모델, 애니메이션, FX
- 사거리 표시, 쿨다운 UI, 툴팁, 공개 룰셋 버전
- 약한 예측을 위한 public hint

클라이언트에 있으면 안 되는 것
- 계정, 결제, 구매, MMR, 대전 내부 점수, 운영/제재 정보
- 서버 보상 계산의 최종 진실
- hidden server rule, anti-cheat rule, 운영용 매치 내부 상태
- 서버가 판정을 위해 신뢰하는 canonical value 자체
```

Server runtime 원칙:

```text
Server는 match 시작 시 ServerPrivate data pack의 version/hash를 확정한다.
Client에는 data version, ruleset id, public manifest, snapshot/event/action 결과만 내려준다.
ClientPublic value는 표시와 예측을 돕는 힌트일 뿐이고, Server GameSim 판정을 대체하지 않는다.
```

이 원칙 때문에 아래 기존 simple visual 구조는 `ClientPublic/Visual`의 하위 구조로 유지한다:

```text
ClientPublic/Visual/
    Champion/
      ChampionModelVisualData.json
      ChampionPoseVisualData.json
      ChampionActionVisualData.json
      SkillVisualData.json
    Object/
      ProjectileVisualData.json
      MinionVisualData.json
      StructureVisualData.json
      JungleVisualData.json
ClientPublic/FX/
  Champions/
  Object/
```

첫 번째 정리 방식, 소유권 기준:

```text
Game data
- 서버 판정, 쿨다운, 사거리, 코스트, HP, 공격력, 스폰, 웨이브, 룰셋에 영향을 주면 Game.
- ServerPrivate/Game에 둔다.
- Shared/GameSim 또는 Server만 authoritative source로 직접 소비한다.
- Engine, Client Renderer, UI, ImGui, DX 타입을 포함하지 않는다.
- Client가 필요한 공개 표시값은 ServerPrivate에서 생성한 ClientPublic/GameHint projection으로만 둔다.
- ClientPublic/GameHint는 절대 gameplay truth가 아니다.

Visual data
- 모델, 텍스처, 애니메이션 키, playback speed, frame event, FX key, projectile texture, yaw offset에 영향을 주면 Visual.
- ClientPublic/Visual 또는 ClientPublic/FX에 둔다.
- Client만 직접 소비한다.
- 서버 판정 결과를 바꾸면 안 된다.

Policy data
- 게임 시작 시 어떤 champion, spell, rune, gold, bot skill rank, spawn slot을 주는지 결정하면 Policy.
- ServerPrivate/Game/Match에 둔다.
- Server가 match 시작 시 한 번 읽고 GameSim component로 변환한다.
- Client에는 policy result 또는 public manifest만 내려간다.

Test data
- smoke roster, dummy HP, debug patrol, temporary scenario는 Test.
- production Game data와 섞지 않는다.

SharedContract data
- action id, cue id, schema version, data pack version, build hash처럼 양쪽이 같은 언어로 대화하기 위한 값이다.
- 판정값도 아니고 visual asset도 아니다.
- Client와 Server가 모두 읽을 수 있지만, 여기에도 계정/결제/MMR/운영 정보는 들어가지 않는다.
```

두 번째 정리 방식, 런타임 읽기 흐름 기준:

```text
Client Input
-> GameCommand
-> Server GameSim reads Game atoms
-> Snapshot/Event/ReplicatedAction
-> Client Visual reads Visual atoms
-> animation/FX/UI playback
```

세 번째 정리 방식, 협업 직군 기준:

```text
기획자
- ChampionStats, SkillTarget, SkillCost, SkillCooldown, SkillRange, SkillStageLock, SkillScaling
- SummonerSpellGameData, MinionGameData, StructureGameData, JungleGameData, GameModeData

디자이너/아티스트
- ChampionModelVisualData, ChampionPoseVisualData, ChampionActionVisualData, SkillVisualData
- ProjectileVisualData, MinionVisualData, StructureVisualData, JungleVisualData, FX/*.wfx

개발자
- SkillEffectPolicy id와 실제 Shared/GameSim/Champions/* execution code
- generator, registry, validation, compatibility adapter

QA/운영
- SmokeRosterData, DebugScenarioData, parity report, live patch diff report
```

네 번째 정리 방식, 회귀 방지 기준:

```text
1. 현재 legacy 값과 동일한 atom json을 먼저 생성한다.
2. 새 atom에서 legacy SkillDef/ChampionDef 호환 테이블을 다시 생성한다.
3. 기존 reader가 같은 값을 읽는지 parity report를 만든다.
4. Server reader만 Game atom으로 바꾼다.
5. Client reader만 Visual atom으로 바꾼다.
6. rg로 legacy reader가 0이 된 뒤 SkillTable/ChampionTable을 삭제한다.
```

1-2. C:/Users/tnest/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

기존 역할:

```text
- 17개 champion의 stats, skill gameplay, summoner spell, passive dash를 담고 있다.
- 동시에 visualYawOffset, animPlaySpeed, castFrame, recoveryFrame도 담고 있다.
```

의심:

```text
visualYawOffset은 서버 판정에 필요하지 않다.
animPlaySpeed는 animation playback data다.
castFrame/recoveryFrame은 visual event data다.
summonerSpells는 champion 소유가 아니라 match loadout 또는 player loadout의 spell data다.
```

아래로 분해한다:

```text
ChampionIdentity.json
- champion
- dataVersion
- authoringHash

ChampionStats.json
- champion
- baseHp, hpPerLevel
- baseMana, manaPerLevel
- baseAd, adPerLevel
- baseAp, apPerLevel
- baseArmor, armorPerLevel
- baseMr, mrPerLevel
- baseAttackSpeed, attackSpeedRatio, attackSpeedPerLevel
- baseAttackRange
- baseMoveSpeed
- navArriveRadius
- spatialRadius
- sightRange

ChampionSkillSlot.json
- champion
- slot
- skillId

SkillTarget.json
- champion
- slot
- stage
- targetShape
- targetResolvePolicy

SkillCost.json
- champion
- slot
- manaCost

SkillCooldown.json
- champion
- slot
- cooldownSec

SkillRange.json
- champion
- slot
- rangeMax

SkillStageLock.json
- champion
- slot
- stage
- lockDurationSec
- stageWindowSec

SkillFacing.json
- champion
- slot
- stage
- facingMode

SkillEffectPolicy.json
- champion
- slot
- scalingTableId
- gameplayPolicyId
- replicatedCueId

ChampionPassiveDash.json
- champion
- distance
- durationSec
- inputGraceSec

SummonerSpellGameData.json
- spellId
- rangeMax
- cooldownSec
- gameplayPolicyId
- replicatedCueId
```

삭제 대상:

```text
ChampionGameData.visualYawOffset
ChampionGameDataSkillStage.animPlaySpeed
ChampionGameDataSkillStage.castFrame
ChampionGameDataSkillStage.recoveryFrame
ChampionGameData.summonerSpells
```

단, 즉시 삭제하지 않는다. generated compatibility 단계에서 기존 `ChampionGameData`를 계속 생성한 뒤, reader 전환 완료 후 삭제한다.

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

기존 역할:

```text
SkillDef s_SkillTable이 target, cooldown, range, mana, animation key, FX key, lock, rotate,
stage, cast frame, recovery frame, playback speed, transition animation, hook id,
skillId, scalingTableId를 한 struct에 섞어 들고 있다.
```

의심:

```text
target/cooldown/range/mana/lock/facing/effect id는 Game atom이다.
animation key/playback speed/cast frame/recovery frame/transition animation/hook id는 Visual atom이다.
legacy local hook bridge 때문에 SkillDef가 살아 있지만 본질 data owner는 아니다.
```

아래로 분해한다:

```text
SkillDef.game 부분
-> SkillTarget.json
-> SkillCost.json
-> SkillCooldown.json
-> SkillRange.json
-> SkillStageLock.json
-> SkillFacing.json
-> SkillEffectPolicy.json

SkillDef.visual 부분
-> SkillVisualData.json
-> ChampionActionVisualData.json
-> VisualEventData

SkillDef.legacy bridge 부분
-> generated compatibility only
-> 최종 삭제 대상
```

최종 삭제 조건:

```text
rg "FindSkillDef|g_SkillTable|SkillDef" Client Shared Server
```

위 결과에서 compatibility adapter와 삭제 계획 문서 외 runtime reader가 0이어야 한다.

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

기존 역할:

```text
ChampionDef s_ChampionTable이 display name, animation prefix, idle/run/basic attack key,
basic attack range, model path, shader path, texture paths, spawn position, scale을 한 struct에 섞어 들고 있다.
```

의심:

```text
display name은 identity 또는 UI label이다.
model/shader/texture/anim keys/model scale/yaw offset은 Visual atom이다.
spawn position은 match spawn policy다.
basic attack range는 ChampionStats 또는 SkillRange다.
ChampionDef는 data owner가 아니라 legacy bootstrap이다.
```

아래로 분해한다:

```text
ChampionDef.identity 부분
-> ChampionIdentity.json

ChampionDef.game 부분
-> ChampionStats.json
-> SkillRange.json

ChampionDef.visual 부분
-> ChampionModelVisualData.json
-> ChampionPoseVisualData.json

ChampionDef.spawn/loadout 부분
-> SpawnPolicyData.json
-> LoadoutPolicyData.json
```

최종 삭제 조건:

```text
rg "FindChampionDef|g_ChampionTable|ChampionDef" Client Shared Server
```

위 결과에서 generated visual registry와 compatibility adapter 외 runtime reader가 0이어야 한다.

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

기존 코드에서 유지할 본질:

```cpp
struct ChampionGameDataSkill
{
    bool_t bValid = false;
    u8_t slot = 0;
    eTargetMode targetMode = eTargetMode::Self;
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    f32_t cooldownSec = 0.f;
    f32_t rangeMax = 0.f;
    f32_t manaCost = 0.f;
    u16_t skillId = 0;
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
};
```

아래로 교체할 최종 방향:

```cpp
struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t authoringHash = 0;
    ChampionStatsDef stats{};
    SkillGameAtomBundle skills[kChampionGameDataSkillSlotCount] = {};
    ChampionGameDataPassiveDash passiveDash{};
};
```

의심:

```text
ChampionGameData가 SkillGameAtomBundle을 직접 들면 struct는 더 크지만 개념 중복이 줄어든다.
반대로 기존 ChampionGameDataSkill을 계속 유지하면 SkillAtomData와 같은 정보를 두 번 정의한다.
본질 기준에서는 SkillAtomData 하나가 더 낫다.
```

단계:

```text
1. schema version 2 generated file을 추가한다.
2. schema version 1 ChampionGameData는 compatibility로 유지한다.
3. ChampionGameDataDB reader를 version 2 우선, version 1 fallback으로 바꾼다.
4. 모든 caller가 atom reader로 이동하면 version 1 struct의 visual field를 삭제한다.
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

삭제 대상:

```cpp
f32_t ResolveVisualYawOffset(eChampion champion)
```

아래로 분해한다:

```text
ChampionGameDataDB
- ResolveStats
- ResolveSkillRange
- ResolveSkillCooldown
- ResolveSkillTiming
- ResolveBasicAttackTiming
- ResolvePassiveDash*

SummonerSpellGameDataDB
- ResolveSummonerSpellRange
- ResolveSummonerSpellCooldown

ChampionVisualDataDB 또는 Client visual registry
- ResolveVisualYawOffset
- ResolveModelVisualData
- ResolvePoseVisualData
- ResolveActionVisualData
```

의심:

```text
ResolveSkillTiming의 animPlaySpeed는 이름상 timing이지만 실제 animation playback이다.
Server GameSim에 필요한 것은 lockDurationSec와 deterministic tick뿐이다.
BasicAttackTiming의 fAnimPlaySpeed도 client playback으로 이동해야 한다.
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionVisualData.h

유지할 본질:

```text
ChampionModelVisualData
- displayName
- fbxPath
- shaderPath
- texturePath
- modelYawOffset
- modelScale

ChampionPoseVisualData
- poseId
- animationKey
- playbackSpeed
- bLoop

ChampionActionVisualData
- actionId
- stageCount
- ChampionActionVisualStageData
```

수정 방향:

```text
ChampionDef에서 옮겨온 displayName, model path, texture path, scale, yaw offset을 모두 여기로 모은다.
SkillVisualData와 중복되는 action stage 표현은 하나로 합친다.
actionId는 ReplicatedAction id와 매칭한다.
```

의심:

```text
SkillVisualData와 ChampionActionVisualData가 둘 다 stage animation을 가진다.
최종 본질은 "서버가 보낸 actionId/stage를 클라이언트가 어떤 animation/event로 재생하는가" 하나다.
따라서 SkillVisualData는 ChampionActionVisualData 생성용 입력이거나 삭제 대상이다.
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/SkillVisualData.h

최종 판단:

```text
SkillVisualData는 migration 중간 단계로는 유효하다.
최종 구조에서는 ChampionActionVisualData와 통합하거나, SkillVisualData를 "authoring file schema"로만 남긴다.
runtime active skill은 SkillDef가 아니라 SkillGameAtomBundle + ChampionActionVisualData + VisualEventData를 읽는다.
```

옮길 값:

```text
animKey
animPlaySpeed
stage2AnimKey
stage2PlaySpeed
castFrame
recoveryFrame
stage2CastFrame
stage2RecoveryFrame
keySwapHookId
onCastAcceptedHookId
castFrameHookId
recoveryHookId
endTransitionIdleAnim
endTransitionRunAnim
endTransitionDuration
vfxKey
sfxKey
```

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

기존 역할:

```text
projectile kind별 spawn FX key, hit FX key, mark FX key, texture path, size, hit size를 코드 상수로 들고 있다.
```

아래로 분해한다:

```text
Data/LoL/Visual/Object/ProjectileVisualData.json
- projectileKind
- spawnFxKey
- hitFxKey
- markFxKey
- texturePath
- hitTexturePath
- radius
- hitRadius
- scale
- hitScale
- bSpawnTrail
- bHitFx
```

삭제 조건:

```text
ProjectileVisualCatalog::Resolve가 generated ProjectileVisualDataDB를 읽고,
hardcoded constexpr ProjectileVisualDesc가 fallback generic 1개만 남거나 0개가 되면 삭제한다.
```

1-10. C:/Users/tnest/Desktop/Winters/Server/Public/Game/ServerMinionTuning.h

의심:

```text
모든 constexpr가 LoL data는 아니다.
path rebuild interval, scan stagger, flow field fallback은 서버 알고리즘 튜닝이다.
wave interval, first wave delay, per minion spawn delay, wave start X는 game mode/wave data다.
```

아래로 분해한다:

```text
Data/LoL/Game/Object/MinionWaveData.json
- initialWaveDelayTicks
- waveIntervalTicks
- perMinionSpawnDelayTicks
- waveStartX
- lanes
- spawnSlots

ServerMinionTuning.h에 남길 값
- path agent radius
- lane clearance radius
- path rebuild intervals
- path build budget
- blocked frames before repath
- flow field fallback thresholds
- target scan staggering
```

1-11. C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerMinionWaveRuntime.cpp

기존 코드:

```cpp
static constexpr MinionSpawnSlot kSpawnSlots[] =
{
    { kRoleMelee, 3.6f, -0.9f },
    { kRoleMelee, 4.8f, 0.0f },
    { kRoleMelee, 6.0f, 0.9f },
    { kRoleRanged, 0.0f, -0.9f },
    { kRoleRanged, 1.2f, 0.0f },
    { kRoleRanged, 2.4f, 0.9f },
};
```

아래로 교체할 방향:

```text
MinionWaveDataDB::ResolveWaveSlots(modeId)
MinionWaveDataDB::ResolveWaveSchedule(modeId)
```

의심:

```text
lane waypoint path resolving은 algorithm이다.
spawn slot composition은 LoL game data다.
```

1-12. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

아래로 분해한다:

```text
ResolveStageStructureMaxHp
-> StructureGameData.json

ResolveStageJungleMaxHp
ResolveStageJungleRadius
ResolveStageJungleAttackRange
ResolveStageJungleAttackDamage
ResolveStageJungleAttackCooldown
-> JungleGameData.json

SpawnServerMinion roleType stats
-> MinionGameData.json

AssignDefaultBotSkillRanks
-> BotSkillRankPolicyData.json

gold.amount = 10000
runeLoadout.eRunes[0] = eRuneId::LethalTempo
champion initial level
-> LoadoutPolicyData.json

fallback structures and champion spawn positions
-> SpawnPolicyData.json 또는 Test/DebugScenarioData.json
```

의심:

```text
stage entry position은 map placement다.
structure HP/radius/attack은 game object data다.
fallback spawn은 production rule이 아니라 debug fallback일 수 있다.
따라서 production SpawnPolicy와 Test DebugScenario를 분리한다.
```

1-13. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSmokeRoster.cpp

아래로 분해한다:

```text
Data/LoL/Test/SmokeRosterData.json
- team
- slot
- champion
- bBot
- bDummy
- spawnPosition
- maxHp override
- patrol points
```

의심:

```text
SmokeRoster는 게임 밸런스가 아니라 검증 장면이다.
Game data로 섞이면 출시/운영 데이터와 테스트 데이터가 오염된다.
```

1-14. C:/Users/tnest/Desktop/Winters/Data/GameModes/gameMode.json

이동 방향:

```text
Data/LoL/Game/Match/GameModeData.json
```

유지할 값:

```text
mode id
displayName
mapID
rulesetID
queueName
teamSize
available
matchmakingEnabled
practiceMode
```

의심:

```text
GameModeData는 LoL에 속한다.
Engine 또는 Shared generic data가 아니다.
Class & Servant는 같은 schema를 참고할 수 있지만 같은 file을 공유하면 안 된다.
```

1-15. C:/Users/tnest/Desktop/Winters/Data/Stage1.dat

유지할 본질:

```text
Stage1.dat와 Stage1.navgrid는 현재 map editor/runtime binary artifact다.
좌표, 구조물 배치, 정글 캠프 배치, 미니언 waypoint는 map placement다.
```

추출 방향:

```text
Stage1.dat를 즉시 JSON으로 바꾸지 않는다.
Data/LoL/Game/Map/SummonersRiftStageBinding.json에 stage asset id, ruleset id, object data binding만 둔다.
map editor export가 안정되면 source authoring format과 binary artifact를 분리한다.
```

의심:

```text
map geometry와 game object tuning을 한 JSON으로 합치면 디자이너와 기획자 소유권이 다시 섞인다.
```

1-16. C:/Users/tnest/Desktop/Winters/Data/LoL/FX

유지할 본질:

```text
*.wfx 파일은 visual asset source다.
이 파일 자체를 game data로 옮기지 않는다.
FX key와 action/event binding만 Visual data로 추출한다.
```

아래로 연결한다:

```text
ChampionActionVisualData.json
ProjectileVisualData.json
MinionVisualData.json
StructureVisualData.json
JungleVisualData.json
```

의심:

```text
server cue id는 Game/EffectPolicy에 있을 수 있다.
하지만 실제 FX file path와 playback choice는 Client Visual data다.
```

1-17. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions

유지할 본질:

```text
champion-specific behavior code는 data가 아니라 실행 규칙이다.
damage application, projectile spawn, state transition, mark consume, dash resolve는 Shared/GameSim code에 남긴다.
```

추출할 값:

```text
scaling id
policy id
base numeric tuning
range/cooldown/cost/stage lock
replicated cue id
```

의심:

```text
모든 champion behavior를 data로 밀어내는 것은 본질이 아니다.
행동의 원리는 code, 조절값과 연결 id는 data다.
```

1-18. C:/Users/tnest/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py

대체 방향:

```text
Tools/LoLData/build_lol_atom_data.py
```

새 generator가 해야 할 일:

```text
1. Data/Gameplay/ChampionGameData/champions.json과 Client legacy table을 읽어 첫 atom json을 생성한다.
2. atom json에서 Shared/GameSim generated game table을 생성한다.
3. atom json에서 Client generated visual table을 생성한다.
4. atom json에서 legacy SkillDef/ChampionDef compatibility table을 생성한다.
5. legacy source와 generated compatibility output을 비교하는 parity report를 출력한다.
```

생성물:

```text
Shared/GameSim/Generated/LoLChampionGameData.generated.h
Shared/GameSim/Generated/LoLChampionGameData.generated.cpp
Shared/GameSim/Generated/LoLSummonerSpellGameData.generated.h
Shared/GameSim/Generated/LoLSummonerSpellGameData.generated.cpp
Shared/GameSim/Generated/LoLObjectGameData.generated.h
Shared/GameSim/Generated/LoLObjectGameData.generated.cpp
Client/Generated/LoLChampionVisualData.generated.h
Client/Generated/LoLChampionVisualData.generated.cpp
Client/Generated/LoLProjectileVisualData.generated.h
Client/Generated/LoLProjectileVisualData.generated.cpp
Client/Generated/LoLLegacySkillTable.generated.cpp
Client/Generated/LoLLegacyChampionTable.generated.cpp
```

의심:

```text
첫 구현에서 legacy table을 바로 삭제하면 프레임 안 read path가 끊길 가능성이 높다.
먼저 generated compatibility로 같은 값을 공급한 뒤 reader를 domain별로 바꾼다.
```

1-19. C:/Users/tnest/Desktop/Winters/Client/Include/Client.vcxproj

확인 필요:

```text
새 Client generated cpp가 프로젝트에 포함되어야 한다.
SkillTable.cpp와 ChampionTable.cpp를 바로 제거하지 않는다.
generated compatibility cpp가 동일 symbol을 제공하는 시점에만 교체한다.
```

1-20. C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj

확인 필요:

```text
새 Shared/GameSim generated object/game/summoner data cpp가 Server build에 포함되어야 한다.
Server가 Client generated visual data를 include하면 안 된다.
```

1-21. C:/Users/tnest/Desktop/Winters/Shared/Include 또는 Shared project files

확인 필요:

```text
Shared generated game data는 Client와 Server 둘 다 빌드할 수 있어야 한다.
Shared generated file은 Engine, Client, Renderer, UI, ImGui, DX include를 갖지 않는다.
```

1-22. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

아래 원칙을 유지한다:

```text
Engine
- generic runtime/render/resource primitive만 소유한다.
- LoL Data를 include하지 않는다.

Shared/GameSim
- deterministic gameplay atom과 execution rule만 소유한다.
- visual data를 include하지 않는다.

Server
- match policy, spawn policy, game object data를 읽어 GameSim component를 만든다.
- Client visual data를 include하지 않는다.

Client
- snapshot/event/action을 visual atom으로 재생한다.
- gameplay truth를 만들지 않는다.

Tools
- Data를 읽어 generated code와 parity report를 만든다.
```

1-23. C:/Users/tnest/Desktop/Winters/Data/ClassServant

이번 구현 대상은 아니지만 방향을 고정한다:

```text
LoL은 Winters 검증용 data pack이다.
Elden은 별도 Data/Elden data pack이다.
Class & Servant는 최종 제품용 Data/ClassServant data pack이다.
세 게임은 Engine/Shared atom contract를 공유할 수 있지만 LoL의 champion/object 수치를 공유하지 않는다.
```

의심:

```text
LoL과 Elden 구조를 쪼개는 것은 맞다.
하지만 쪼개야 하는 것은 product data pack이지 Engine/Shared 기본 원리가 아니다.
```

2. 검증

검증 1, 소유권 검증:

```powershell
rg -n "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame" Shared/GameSim Server -g "!Shared/GameSim/Generated/**"
rg -n "fbxPath|texturePath|shaderPath|Visual|FX|ImGui|DX" Shared/GameSim Server -g "!Shared/GameSim/Generated/**"
rg -n "cooldownSec|rangeMax|manaCost|lockDurationSec|gameplayPolicyId" Client/Private/GameObject Client/Public/GameObject
rg -n "Account|Purchase|Payment|MMR|Ban|Sanction|AntiCheat|Hidden" Data/LoL/ClientPublic Data/LoL/SharedContract
rg -n "ServerPrivate" Client
```

통과 기준:

```text
Shared/GameSim과 Server의 authoritative reader에는 visual-only field가 없다.
Client visual data에는 gameplay truth field가 없다.
ClientPublic과 SharedContract에는 계정/결제/MMR/운영/안티치트/hidden server rule 정보가 없다.
Client runtime code는 ServerPrivate path를 직접 읽지 않는다.
legacy compatibility와 migration adapter는 예외로 표시되어야 한다.
```

검증 2, 런타임 흐름 검증:

```text
Client Input -> GameCommand -> Server GameSim -> Snapshot/Event/ReplicatedAction -> Client Visual
```

통과 기준:

```text
Server skill validation은 SkillGameAtomBundle만 읽는다.
Server object spawn은 Minion/Structure/Jungle GameData만 읽는다.
Client animation/FX는 ChampionActionVisualData, VisualEventData, ProjectileVisualData만 읽는다.
Client가 cooldown, damage, HP, hit validation을 최종 결정하지 않는다.
```

검증 3, 협업 직군 검증:

```text
기획자가 바꾸는 파일
- Data/LoL/Game/**

디자이너/아티스트가 바꾸는 파일
- Data/LoL/Visual/**
- Data/LoL/FX/**

개발자가 바꾸는 파일
- Shared/GameSim/Champions/**
- Tools/LoLData/**
- Generated reader/registry

QA/운영이 바꾸는 파일
- Data/LoL/Test/**
- debug scenario data
```

통과 기준:

```text
한 직군이 자신의 데이터를 수정하기 위해 다른 직군 소유 파일을 수정하지 않아도 된다.
```

검증 4, legacy parity 검증:

```text
기존 Data/Gameplay/ChampionGameData/champions.json
기존 Client/Private/GameObject/SkillTable.cpp
기존 Client/Private/GameObject/ChampionTable.cpp
기존 Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp
기존 Server/Private/Game/GameRoomSpawn.cpp
기존 Server/Private/Game/ServerMinionWaveRuntime.cpp
```

위 값과 새 atom generated output을 비교한다.

비교 항목:

```text
Champion count: 17
Champion stats
Skill slot target/cost/cooldown/range/stage lock/facing/effect id
Skill visual animation/event/playback/transition
Champion model/pose/display/scale/yaw
Summoner spell range/cooldown
Projectile visual desc
Minion role stats
Minion wave composition/schedule
Structure HP
Jungle HP/radius/attack
Smoke roster
```

통과 기준:

```text
초기 migration에서는 generated compatibility reader가 legacy와 같은 값을 반환해야 한다.
의도적 변경은 별도 patch로 분리한다.
```

검증 5, forbidden dependency 검증:

```powershell
rg -n "#include .*Client|#include .*Renderer|#include .*UI|#include .*ImGui|#include .*d3d|#include .*DX" Shared/GameSim Server
rg -n "#include .*Server" Client Shared/GameSim
rg -n "Data/LoL/Visual|ChampionVisualData|SkillVisualData|ProjectileVisualData" Shared/GameSim Server
rg -n "Data/LoL/ServerPrivate" Client
```

통과 기준:

```text
Shared/GameSim은 Client/Visual/Renderer/UI/DX를 모른다.
Server는 Client visual data를 모른다.
Client는 Shared Game atom을 읽을 수 있지만 authoritative result를 만들지 않는다.
```

검증 6, build/codegen 검증:

```powershell
python Tools/LoLData/build_lol_atom_data.py --check
git diff --check
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:SimLab /p:Configuration=Debug /p:Platform=x64
```

확인 필요:

```text
현재 repo의 실제 solution target 이름은 구현 직전 확인한다.
새 generated cpp/h는 vcxproj에 포함한다.
Engine public header가 바뀌면 UpdateLib.bat 동기화가 필요하다.
```

검증 7, runtime smoke 검증:

```text
Server authority smoke
- champion spawn
- default loadout
- skill cast
- cooldown
- basic attack windup
- minion wave spawn
- structure/jungle spawn
- projectile event
- replicated action

Client visual smoke
- champion model/texture
- idle/run/basic attack animation
- skill animation stage 1/2
- cast/recovery/key swap/cast accepted visual event
- FX cue one-shot playback
- projectile spawn/hit visual
```

통과 기준:

```text
동작이 바뀌지 않아야 한다.
서버 로그만으로 통과 처리하지 않는다.
client event/cue application과 실제 visual playback을 분리해서 확인한다.
```

검증 8, 삭제 검증:

```powershell
rg -n "SkillDef|FindSkillDef|g_SkillTable|ChampionDef|FindChampionDef|g_ChampionTable" Client Shared Server
```

통과 기준:

```text
legacy compatibility adapter 외 runtime reader가 0이 된 뒤에만 SkillTable/ChampionTable 삭제를 진행한다.
```

최종 판단:

```text
정말 본질만 남겼는가?
- 서버 판정에 필요하면 Game atom.
- 서버가 신뢰하는 canonical value면 ServerPrivate.
- 클라이언트에 보여도 되는 공개 표시값이면 ClientPublic projection.
- 화면 재생에만 필요하면 Visual atom.
- match 시작 조건이면 Policy atom.
- 검증 장면이면 Test atom.
- 서로 같은 id/version/hash로 대화하기 위한 값이면 SharedContract.
- 실행 원리이면 code.
- 데이터 생성/검증 반복이면 Tool.

더 나눌 수 있는가?
- 하나의 파일이 두 직군의 소유권을 갖는다면 더 나눈다.
- 하나의 struct가 Server와 Client 이유를 동시에 갖는다면 더 나눈다.
- 하나의 값이 runtime 중 두 방향으로 해석된다면 이름을 바꾸거나 atom을 나눈다.

회귀 없이 가능한가?
- legacy value extraction
- generated compatibility
- parity report
- reader switch
- forbidden dependency check
- build
- runtime smoke
- legacy deletion
순서가 지켜질 때만 가능하다.
```
