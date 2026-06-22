Session - LoL data를 ServerPrivate canonical, ClientPublic projection, SharedContract manifest로 나누고 legacy reader를 generated compatibility로 회귀 없이 전환한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Data/LoL

최상위 본질:

```text
Data는 코드가 아니다.
Data는 협업자가 바꾸는 사실이다.
하지만 모든 Data가 플레이어 파일에 들어가면 안 된다.
```

최종 구조:

```text
Data/LoL/
  ServerPrivate/
    Game/
      Champion/
        Identity/
        Stats/
        Skill/
        Scaling/
        Passive/
      Object/
        Minion/
        Structure/
        Jungle/
      Match/
        GameMode/
        SpawnPolicy/
        LoadoutPolicy/
        BotPolicy/
      Map/
        StageBinding/
  ClientPublic/
    GameHint/
      Champion/
      Skill/
      Ruleset/
    Visual/
      Champion/
      Object/
    FX/
      Champions/
      Object/
  SharedContract/
    DataPackManifest.json
    ActionIdRegistry.json
    CueIdRegistry.json
    SchemaVersion.json
  Test/
    SmokeRoster/
    DebugScenario/
```

의심:

```text
Data/LoL/Game 하나로 충분한가?
아니다. Server가 신뢰하는 canonical value와 Client가 보여주는 public hint는 같은 값처럼 보여도 본질이 다르다.

ClientPublic에 cooldown/range hint를 둬도 되는가?
된다. UI/툴팁/사거리 표시/약한 예측에 필요하다.
하지만 Server 판정은 ClientPublic을 절대 신뢰하지 않는다.

Server가 runtime에 모든 정보를 내려줘야 하는가?
아니다. Server는 match data version/hash와 판정 결과를 내려준다.
Client는 설치된 ClientPublic projection으로 보여주고, Server snapshot/event/action으로 최종 상태를 맞춘다.
```

실제 배포 기준:

```text
Player client package
- Data/LoL/ClientPublic/**
- Data/LoL/SharedContract/**
- Client/Bin/Resource/**

Dedicated server package
- Data/LoL/ServerPrivate/**
- Data/LoL/SharedContract/**
- Server binaries

QA/dev package
- Data/LoL/Test/**
- legacy extraction sources
- parity reports
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/DataPackManifest.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

enum class DataPackVisibility : u8_t
{
    ServerPrivate = 0,
    ClientPublic = 1,
    SharedContract = 2,
    TestOnly = 3,
};

struct DataPackManifest
{
    u32_t schemaVersion = 1;
    u32_t dataVersion = 1;
    u32_t buildHash = 0;
    u32_t rulesetId = 0;
    DataPackVisibility visibility = DataPackVisibility::SharedContract;
};
```

본질:

```text
schemaVersion
- 파일 구조가 바뀌었는가?

dataVersion
- 기획/디자인 데이터 내용이 바뀌었는가?

buildHash
- ServerPrivate와 ClientPublic이 같은 cook에서 나온 projection인가?

rulesetId
- 같은 LoL 안에서도 normal/practice/aram 같은 규칙이 다른가?

visibility
- 이 data pack이 어느 package에 들어갈 수 있는가?
```

의심:

```text
manifest가 gameplay truth인가?
아니다. 양쪽이 같은 버전 언어로 대화하기 위한 계약이다.
따라서 SharedContract에 둔다.
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/LoLPublicGameHintData.h

새 파일:

```cpp
#pragma once

#include "GameContext.h"
#include "WintersTypes.h"

inline constexpr u8_t kLoLPublicSkillHintSlotCount = 5;

struct LoLPublicSkillHintData
{
    bool_t bVisible = false;
    u8_t slot = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
};

struct LoLPublicChampionHintData
{
    bool_t bVisible = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t publicHash = 0;
    LoLPublicSkillHintData skills[kLoLPublicSkillHintSlotCount] = {};
};
```

본질:

```text
ClientPublic hint는 UI/툴팁/사거리/약한 예측용 공개 정보다.
damage, hidden proc, anti-cheat, server-only policy는 넣지 않는다.
Server GameSim은 이 struct를 include하거나 읽지 않는다.
```

의심:

```text
이 header가 Shared에 있어도 되는가?
가능하다. 이것은 gameplay truth가 아니라 wire/contract 성격의 POD다.
하지만 Server authoritative system이 이 데이터를 읽기 시작하면 실패다.
```

1-4. C:/Users/tnest/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

기존 파일의 역할:

```text
현재 17개 champion의 legacy source다.
stats, skill gameplay, summoner spell, passive dash와 visual playback field가 섞여 있다.
```

아래로 추출한다:

```text
ServerPrivate/Game/Champion/Identity/*.json
ServerPrivate/Game/Champion/Stats/*.json
ServerPrivate/Game/Champion/Skill/Slot/*.json
ServerPrivate/Game/Champion/Skill/Target/*.json
ServerPrivate/Game/Champion/Skill/Cost/*.json
ServerPrivate/Game/Champion/Skill/Cooldown/*.json
ServerPrivate/Game/Champion/Skill/Range/*.json
ServerPrivate/Game/Champion/Skill/StageLock/*.json
ServerPrivate/Game/Champion/Skill/Facing/*.json
ServerPrivate/Game/Champion/Skill/EffectPolicy/*.json
ServerPrivate/Game/Champion/Passive/*.json
ServerPrivate/Game/Match/SummonerSpell/*.json
ClientPublic/GameHint/Champion/*.json
ClientPublic/GameHint/Skill/*.json
ClientPublic/Visual/Champion/*.json
```

필드별 판단:

```text
champion, dataVersion
-> SharedContract 또는 ServerPrivate identity

stats
-> ServerPrivate canonical
-> 필요한 일부만 ClientPublic hint로 projection 가능

targetMode, cooldownSec, rangeMax, manaCost, lockDurationSec, stageWindowSec
-> ServerPrivate canonical
-> cooldownSec/rangeMax는 ClientPublic hint로 projection 가능

skillId, scalingTableId, gameplayPolicyId
-> ServerPrivate canonical
-> action/cue id만 SharedContract에 projection

visualCueId
-> 이름을 replicatedCueId로 바꾸고 SharedContract/CueIdRegistry와 연결

visualYawOffset, animPlaySpeed, castFrame, recoveryFrame
-> ClientPublic/Visual

summonerSpells
-> ChampionGameData에서 삭제
-> ServerPrivate/Game/Match/SummonerSpell
-> ClientPublic/GameHint에는 tooltip에 필요한 공개 정보만 projection
```

회귀 없는 순서:

```text
1. champions.json을 삭제하지 않는다.
2. 새 atom json을 생성한다.
3. atom json에서 기존 ChampionGameData.generated.*와 같은 값을 다시 생성한다.
4. parity report가 통과하면 Server reader만 새 generated table로 바꾼다.
5. 모든 reader 전환 후 legacy champions.json을 source-of-truth에서 내린다.
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

기존 역할:

```text
SkillDef 하나가 Game, Visual, Hook, Legacy bridge를 모두 들고 있다.
```

아래로 나눈다:

```text
ServerPrivate/Game/Champion/Skill/*
- target
- cost
- cooldown
- range
- stage lock
- facing
- effect policy

ClientPublic/Visual/Champion/*
- animation key
- playback speed
- visual event frame
- hook id
- end transition animation
- vfx/sfx key

SharedContract/*
- action id
- cue id
- schema/data version
```

아래 기존 코드가 사라져야 하는 최종 조건:

```cpp
static SkillDef s_SkillTable[] =
{
};
```

교체 방향:

```text
Client/Generated/LoLLegacySkillTable.generated.cpp가 migration 동안 같은 symbol을 제공한다.
Scene_InGame과 SkillRegistry가 SkillGameAtomBundle + ChampionActionVisualData를 직접 읽게 되면 generated legacy table도 삭제한다.
```

의심:

```text
SkillDef를 바로 삭제할 수 있는가?
아니다. 아직 Scene_InGame, SkillRegistry, legacy hook bridge가 읽는다.
삭제는 마지막 단계다.
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

기존 역할:

```text
ChampionDef 하나가 display, model, texture, anim key, basic attack range, spawn position을 섞고 있다.
```

아래로 나눈다:

```text
ServerPrivate/Game/Champion/Stats
- basic attack range

ServerPrivate/Game/Match/SpawnPolicy
- spawn slot
- team spawn

ClientPublic/Visual/Champion
- display name
- model path
- shader path
- texture path
- pose animation key
- model scale
- model yaw offset

ClientPublic/GameHint/Champion
- displayed champion name
- public skill/range hint
```

교체 방향:

```text
Client/Generated/LoLLegacyChampionTable.generated.cpp가 migration 동안 같은 symbol을 제공한다.
ChampionCatalog와 scene spawn/read path가 visual registry + spawn policy projection을 읽으면 legacy table을 삭제한다.
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

기존 역할:

```text
Projectile kind별 visual desc가 client code constexpr로 박혀 있다.
```

아래로 이동:

```text
ClientPublic/Visual/Object/Projectile/*.json
```

교체 방향:

```text
ProjectileVisualCatalog::Resolve
-> LoLProjectileVisualDataDB::Resolve
-> fallback generic visual
```

의심:

```text
projectile damage/speed/hit 판정은 여기 들어가면 안 된다.
여기는 visual desc만이다.
```

1-8. C:/Users/tnest/Desktop/Winters/Server/Public/Game/ServerMinionTuning.h

남길 값:

```text
path agent radius
lane clearance radius
path rebuild interval
path build budget
blocked frames before repath
flow field fallback thresholds
target scan staggering
```

옮길 값:

```text
kWaveIntervalTicks
kInitialWaveDelayTicks
kPerMinionSpawnDelayTicks
kWaveStartX
```

이동:

```text
ServerPrivate/Game/Object/Minion/WaveSchedule.json
ClientPublic/GameHint/Ruleset/MinionWaveHint.json
```

의심:

```text
pathfinding algorithm tuning은 code에 남겨도 된다.
LoL 웨이브 규칙은 data로 빼야 한다.
```

1-9. C:/Users/tnest/Desktop/Winters/Server/Private/Game/ServerMinionWaveRuntime.cpp

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

교체 방향:

```cpp
const LoLMinionWaveData* pWave = LoLMinionGameDataDB::FindWaveData(rulesetId);
```

CONFIRM_NEEDED:

```text
실제 CServerMinionWaveRuntime 생성자 또는 GameRoom 초기화 경로에 rulesetId/data handle을 어떻게 주입할지 구현 직전 확인한다.
현재는 EnqueueWave가 전역 ServerMinionTuning을 직접 읽는다.
```

1-10. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

기존 코드에서 data로 빼야 하는 것:

```text
ResolveStageStructureMaxHp
ResolveStageJungleMaxHp
ResolveStageJungleRadius
ResolveStageJungleAttackRange
ResolveStageJungleAttackDamage
ResolveStageJungleAttackCooldown
SpawnServerMinion roleType stats
AssignDefaultBotSkillRanks level order
gold.amount = 10000
runeLoadout.eRunes[0] = eRuneId::LethalTempo
champion initial level
fallback spawn/structure/debug values
```

이동:

```text
ServerPrivate/Game/Object/Structure/*.json
ServerPrivate/Game/Object/Jungle/*.json
ServerPrivate/Game/Object/Minion/*.json
ServerPrivate/Game/Match/BotPolicy/*.json
ServerPrivate/Game/Match/LoadoutPolicy/*.json
ServerPrivate/Game/Match/SpawnPolicy/*.json
Test/DebugScenario/*.json
```

교체 방향:

```text
GameRoomSpawn은 code에서 값을 결정하지 않는다.
GameRoomSpawn은 data를 읽어 component를 만든다.
```

의심:

```text
fallback 값은 production data인가?
아니다. Stage load 실패나 smoke용이면 Test/DebugScenario다.
```

1-11. C:/Users/tnest/Desktop/Winters/Data/GameModes/gameMode.json

분리:

```text
ServerPrivate/Game/Match/GameMode/*.json
- mode id
- ruleset id
- team size
- matchmaking enabled
- practice flag
- server queue rule

ClientPublic/GameHint/Ruleset/*.json
- mode display name
- queue name
- available flag
- practice display flag

SharedContract/DataPackManifest.json
- mode data version/hash
```

의심:

```text
available/matchmakingEnabled가 client에 있어도 되는가?
available은 표시 가능하다.
matchmakingEnabled의 실제 권한은 backend/server가 가진다.
```

1-12. C:/Users/tnest/Desktop/Winters/Tools/LoLData/build_lol_data_pack.py

CONFIRM_NEEDED:

```text
이 파일은 실제 구현 직전 기존 Tools/ChampionData/build_champion_game_data.py 전체와 legacy table parsing 범위를 다시 확인한 뒤 완성한다.
```

필수 동작:

```text
1. legacy source 읽기
   - Data/Gameplay/ChampionGameData/champions.json
   - Client/Private/GameObject/SkillTable.cpp
   - Client/Private/GameObject/ChampionTable.cpp
   - Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp
   - Server/Private/Game/GameRoomSpawn.cpp
   - Server/Private/Game/ServerMinionWaveRuntime.cpp
   - Data/GameModes/gameMode.json

2. atom json 생성
   - ServerPrivate canonical
   - ClientPublic projection
   - SharedContract manifest
   - Test scenario

3. generated code 생성
   - Shared/GameSim/Generated/LoLChampionGameData.generated.*
   - Shared/GameSim/Generated/LoLSummonerSpellGameData.generated.*
   - Shared/GameSim/Generated/LoLObjectGameData.generated.*
   - Client/Generated/LoLChampionVisualData.generated.*
   - Client/Generated/LoLProjectileVisualData.generated.*
   - Client/Generated/LoLLegacySkillTable.generated.cpp
   - Client/Generated/LoLLegacyChampionTable.generated.cpp

4. parity report 생성
   - legacy와 generated compatibility가 같은 값을 반환하는지 비교
```

최소 CLI:

```text
python Tools/LoLData/build_lol_data_pack.py --extract
python Tools/LoLData/build_lol_data_pack.py --generate
python Tools/LoLData/build_lol_data_pack.py --check
```

의심:

```text
첫 구현에서 perfect parser를 만들 필요가 있는가?
아니다. champions.json과 현재 명확히 구조화된 값부터 시작한다.
SkillTable/ChampionTable hardcoded C++는 1차에서는 수동 seed json + parity report로 안전하게 끊고, 이후 parser를 늘린다.
```

1-13. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

현재 문제:

```text
ChampionGameDataDB가 visualYawOffset을 resolve한다.
ResolveSkillTiming이 animPlaySpeed까지 들고 있다.
SummonerSpell도 ChampionGameData 내부에서 찾는다.
```

분리:

```text
ChampionGameDataDB
- ServerPrivate champion stats/skill/passive만 읽는다.

SummonerSpellGameDataDB
- ServerPrivate match/summoner spell만 읽는다.

Client ChampionVisualDataDB
- ClientPublic visual yaw/model/pose/action만 읽는다.

Client PublicGameHintDB
- ClientPublic tooltip/range/cooldown hint만 읽는다.
```

교체 순서:

```text
1. 기존 ChampionGameDataDB API는 유지한다.
2. 내부에서 generated compatibility를 읽게 한다.
3. 새 DB를 추가하고 caller를 하나씩 이동한다.
4. caller 0이 되면 old API를 삭제한다.
```

1-14. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

최종 runtime active skill 본질:

```text
command
game atom
visual action
visual event cursor
```

최종 방향:

```text
SkillDef pointer를 active runtime에서 제거한다.
Client는 Server가 보낸 ReplicatedAction/actionId/stage를 ClientPublic visual data로 재생한다.
쿨다운/사거리 UI는 PublicGameHintDB를 읽는다.
실제 cast 성공/실패는 Server event를 따른다.
```

의심:

```text
Client가 public cooldown hint를 보고 skill 가능 여부를 최종 결정하면 안 된다.
local validation은 UX용이고, server rejection이 최종이다.
```

1-15. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

최종 삭제 대상:

```cpp
f32_t animPlaySpeed = 1.f;
f32_t castFrame = 0.f;
f32_t recoveryFrame = 0.f;
f32_t visualYawOffset = 0.f;
SummonerSpellGameData summonerSpells[kSummonerSpellGameDataSlotCount] = {};
```

최종 유지 대상:

```text
stats
skill target/cost/cooldown/range/stage lock/facing/effect policy
passive gameplay data
dataVersion/authoringHash
```

삭제 조건:

```text
rg "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|summonerSpells" Shared/GameSim Server
```

위 결과에서 generated compatibility와 migration 문서 외 authoritative runtime reader가 0이어야 한다.

1-16. C:/Users/tnest/Desktop/Winters/Client/Include/Client.vcxproj

CONFIRM_NEEDED:

```text
generated client cpp/h 추가 시 실제 vcxproj anchor를 구현 직전 확인한다.
```

추가 대상:

```text
Client/Generated/LoLChampionVisualData.generated.cpp
Client/Generated/LoLProjectileVisualData.generated.cpp
Client/Generated/LoLPublicGameHintData.generated.cpp
Client/Generated/LoLLegacySkillTable.generated.cpp
Client/Generated/LoLLegacyChampionTable.generated.cpp
```

1-17. C:/Users/tnest/Desktop/Winters/Server/Include/Server.vcxproj

CONFIRM_NEEDED:

```text
generated server/shared cpp 추가 시 실제 vcxproj anchor를 구현 직전 확인한다.
```

추가 대상:

```text
Shared/GameSim/Generated/LoLChampionGameData.generated.cpp
Shared/GameSim/Generated/LoLSummonerSpellGameData.generated.cpp
Shared/GameSim/Generated/LoLObjectGameData.generated.cpp
Shared/GameSim/Generated/LoLDataPackManifest.generated.cpp
```

금지:

```text
Server.vcxproj에 Client/Generated/LoLChampionVisualData.generated.cpp를 넣지 않는다.
Server가 ClientPublic/Visual을 include하지 않는다.
```

1-18. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

아래 원칙을 유지한다:

```text
LoL data pack은 product data다.
Engine은 LoL data pack을 모른다.
Shared/GameSim은 ServerPrivate generated gameplay data와 SharedContract만 안다.
Server는 ServerPrivate를 신뢰한다.
Client는 ClientPublic과 SharedContract만 읽는다.
ClientPublic은 표시/예측 힌트일 뿐 gameplay truth가 아니다.
```

1-19. C:/Users/tnest/Desktop/Winters/.md/plan/refactor/09_LOL_DATA_ATOM_EXTRACTION_COLLAB_PLAN.md

이미 반영한 수정:

```text
Data/LoL root를 ServerPrivate, ClientPublic, SharedContract, Test 기준으로 보강했다.
Client package에 들어가도 되는 정보와 server-only 정보를 나눴다.
검증 항목에 ClientPublic/SharedContract secret scan과 Client의 ServerPrivate path 접근 금지를 추가했다.
```

1-20. 실질 구현 순서

반영 순서:

```text
S0. 문서/계약 보강
-> 09/10 계획서, DataPackManifest, PublicHintData 추가
-> verify: git diff --check, header compile

S1. legacy extraction seed 생성
-> current champions.json + SkillTable + ChampionTable + ProjectileVisualCatalog + GameRoom hardcode를 atom json으로 복제
-> verify: generated parity report only, runtime reader 변경 없음

S2. generated compatibility 생성
-> atom json에서 기존 ChampionGameData.generated, SkillTable, ChampionTable equivalent 생성
-> verify: legacy vs generated value parity

S3. ServerPrivate reader 전환
-> ChampionGameDataDB, SummonerSpellGameDataDB, ObjectGameDataDB를 새 generated data로 전환
-> verify: Server/SimLab build, server authority smoke

S4. ClientPublic reader 전환
-> ChampionVisualDataDB, ProjectileVisualDataDB, PublicGameHintDB로 전환
-> verify: Client build, visual smoke, FX cue single source

S5. package boundary 검증
-> Client package에 ServerPrivate 미포함
-> Server package에 ClientPublic/Visual 미포함
-> verify: rg + package manifest diff

S6. legacy 삭제
-> SkillTable.cpp, ChampionTable.cpp, old visual field, old summoner spell embedding 제거
-> verify: rg legacy symbol 0, build, runtime smoke
```

2. 검증

검증 1, 문서/공백:

```powershell
git diff --check
```

통과 기준:

```text
markdown/code whitespace 오류가 없다.
```

검증 2, secret/public boundary:

```powershell
rg -n "Account|Purchase|Payment|Receipt|MMR|Ban|Sanction|AntiCheat|Hidden|ServerSecret" Data/LoL/ClientPublic Data/LoL/SharedContract
rg -n "Data/LoL/ServerPrivate" Client
rg -n "ClientPublic/Visual|ChampionVisualData|SkillVisualData|ProjectileVisualData" Shared/GameSim Server
```

통과 기준:

```text
ClientPublic/SharedContract에 계정/결제/MMR/운영/안티치트/hidden server rule 정보가 없다.
Client runtime은 ServerPrivate path를 직접 읽지 않는다.
Shared/GameSim과 Server authoritative code는 Client visual data를 읽지 않는다.
```

검증 3, ownership boundary:

```powershell
rg -n "#include .*Client|#include .*Renderer|#include .*UI|#include .*ImGui|#include .*d3d|#include .*DX" Shared/GameSim Server
rg -n "#include .*Server" Client Shared/GameSim
```

통과 기준:

```text
Shared/GameSim은 Engine/Client/Renderer/UI/ImGui/DX를 모른다.
Client는 Server를 include하지 않는다.
Server는 Client visual을 include하지 않는다.
```

검증 4, legacy parity:

```powershell
python Tools/LoLData/build_lol_data_pack.py --check
```

통과 기준:

```text
기존 legacy source와 generated compatibility output의 값이 같다.
초기 migration에서 gameplay/visual 동작이 바뀌면 실패다.
```

비교 대상:

```text
17 champion count
champion stats
skill target/cost/cooldown/range/stage lock/facing/effect id
skill animation/playback/frame event/transition
champion model/texture/pose/yaw/scale/display
summoner spell public/private split
projectile visual desc
minion role stats
minion wave schedule/slots
structure HP
jungle HP/radius/attack
game mode
smoke roster
```

검증 5, generated project inclusion:

```text
확인 필요:
- 새 generated cpp가 Client/Server vcxproj에 포함되어 있는지 확인한다.
- Server project에 client visual generated cpp가 들어가지 않았는지 확인한다.
- Client project에 ServerPrivate generated cpp가 들어가지 않았는지 확인한다.
```

검증 6, build:

```powershell
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:SimLab /p:Configuration=Debug /p:Platform=x64
```

CONFIRM_NEEDED:

```text
실제 solution target 이름은 구현 직전 `rg "<Project"` 또는 solution inspect로 확인한다.
```

검증 7, runtime:

```text
Server authority smoke
- champion spawn
- loadout/rune/gold
- skill cast/reject
- cooldown
- damage/hit
- basic attack windup
- minion wave
- structure/jungle spawn
- projectile event
- replicated action

Client visual smoke
- champion model/texture
- idle/run/basic attack
- skill stage 1/2 animation
- visual frame event
- FX cue one-shot
- projectile spawn/hit visual
- cooldown/range UI hint
```

통과 기준:

```text
서버 로그만으로 통과 처리하지 않는다.
server sim acceptance, server event/cue emission, client event/cue application, actual visual playback을 분리해서 확인한다.
```

검증 8, package:

```powershell
rg -n "ServerPrivate" Client/Bin/Resource
rg -n "Account|Purchase|Payment|MMR|AntiCheat|Hidden" Client/Bin/Resource
```

통과 기준:

```text
플레이어가 받는 resource/package에는 ServerPrivate와 server-only secret이 없다.
ClientPublic과 SharedContract만 포함된다.
```

검증 9, legacy deletion:

```powershell
rg -n "SkillDef|FindSkillDef|g_SkillTable|ChampionDef|FindChampionDef|g_ChampionTable" Client Shared Server
rg -n "visualYawOffset|animPlaySpeed|castFrame|recoveryFrame|summonerSpells" Shared/GameSim Server
```

통과 기준:

```text
compatibility adapter 외 runtime reader가 0이 된 뒤에만 legacy 삭제를 진행한다.
```

최종 판단:

```text
정말 본질만 남겼는가?
- 서버가 신뢰해야 하면 ServerPrivate.
- 클라이언트가 보여줘야 하면 ClientPublic.
- 양쪽이 같은 언어로 대화해야 하면 SharedContract.
- 검증 장면이면 Test.
- 실행 원리이면 code.
- 추출/검증/생성 반복이면 Tool.

150명 champion과 live patch에 견디는가?
- champion 추가는 code table 수동 편집이 아니라 data pack 추가와 generated 검증으로 끝나야 한다.
- champion-specific 실행 원리만 Shared/GameSim/Champions code에 추가한다.
- balance patch는 ServerPrivate canonical 변경에서 시작하고 ClientPublic projection과 manifest hash를 함께 생성한다.
- client는 public hint를 표시하지만 server result를 최종 truth로 받는다.

플레이어 파일에 없어야 하는 것은 없는가?
- 계정/결제/MMR/운영/안티치트/hidden server rule은 client package에 없다.
- 공개 tooltip/range/cooldown/visual/FX는 있어도 되지만 신뢰하지 않는다.
```
